#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <time.h>

#include "actuator_manager.h"
#include "ap_provision_manager.h"
#include "cloud_client.h"
#include "config_store.h"
#include "control_rules.h"
#include "device_config.h"
#include "sensor_manager.h"
#include "secrets.h"
#include "display_manager.h"
#include "button_manager.h"

namespace {

ConfigStore g_config_store;
DeviceConfig g_config;
SensorManager g_sensor_manager;
ActuatorManager g_actuator_manager;
APProvisionManager g_ap_manager;
CloudClient g_cloud_client;
DisplayManager g_display_manager;
ButtonManager g_button_manager;

bool g_in_settings = false;
bool g_editing_item = false;

SensorSnapshot g_latest_snapshot;
SensorSnapshot g_last_published_snapshot;
RuntimeStatus g_last_published_status;

uint32_t g_last_sample_ms = 0;
uint32_t g_last_publish_ms = 0;
uint32_t g_last_change_publish_ms = 0;

uint32_t g_low_food_since_ms = 0;
uint32_t g_water_low_since_ms = 0;
uint32_t g_water_ok_since_ms = 0;
uint32_t g_pump_started_ms = 0;
uint32_t g_pump_cooldown_until_ms = 0;
uint32_t g_voc_high_since_ms = 0;
uint32_t g_voc_low_since_ms = 0;
uint32_t g_fan_started_ms = 0;
bool g_fan_overtime_reported = false;

uint32_t g_wifi_retry_count = 0;
uint32_t g_next_wifi_retry_at_ms = 0;
uint32_t g_last_offline_event_try_ms = 0;
uint32_t g_temp_hum_fault_since_ms = 0;

bool g_pump_locked = false;
bool g_low_food_event_active = false;
bool g_low_water_event_active = false;
bool g_auto_feed_waiting_recover = false;
bool g_wifi_was_connected = false;
bool g_cloud_was_online = false;
bool g_pending_wifi_offline_event = false;
bool g_pending_cloud_offline_event = false;

String g_pending_feed_request_id = "";
String g_pending_feed_command_name = "";
bool g_pending_feed_active = false;

constexpr uint32_t kFeedLowHoldMs = 3000;
constexpr uint32_t kOfflineEventRetryMs = 5000;
constexpr uint32_t kWiFiConnectTimeoutMs = 10000;
constexpr uint32_t kMaxWiFiRetryBeforeAp = 3;

uint64_t NowTs() {
  const time_t now = time(nullptr);
  if (now > 1700000000) {
    return static_cast<uint64_t>(now) * 1000ULL;
  }
  return static_cast<uint64_t>(millis());
}

String BuildChipSuffix() {
  const uint64_t chip_id = ESP.getEfuseMac();
  char suffix[9] = {0};
  snprintf(suffix, sizeof(suffix), "%08llX",
           static_cast<unsigned long long>(chip_id & 0xFFFFFFFFULL));
  return String(suffix);
}

bool IsWithinNightWindow(const DeviceConfig& config) {
  if (!config.night_mode_enabled) {
    return true;
  }

  struct tm time_info;
  if (!getLocalTime(&time_info, 10)) {
    return true;
  }

  const int now_minutes = time_info.tm_hour * 60 + time_info.tm_min;
  const int start_hour = config.night_mode_start.substring(0, 2).toInt();
  const int start_min = config.night_mode_start.substring(3, 5).toInt();
  const int end_hour = config.night_mode_end.substring(0, 2).toInt();
  const int end_min = config.night_mode_end.substring(3, 5).toInt();
  const int start_minutes = start_hour * 60 + start_min;
  const int end_minutes = end_hour * 60 + end_min;

  if (start_minutes <= end_minutes) {
    return now_minutes >= start_minutes && now_minutes <= end_minutes;
  }
  return now_minutes >= start_minutes || now_minutes <= end_minutes;
}

bool HasSignificantChange(const SensorSnapshot& a, const SensorSnapshot& b,
                          const RuntimeStatus& sa, const RuntimeStatus& sb) {
  if (fabs(a.temp_c - b.temp_c) >= 0.5F) {
    return true;
  }
  if (fabs(a.hum_rh - b.hum_rh) >= 2.0F) {
    return true;
  }
  if (fabs(a.food_g - b.food_g) >= 5.0F) {
    return true;
  }
  if (a.water_low != b.water_low) {
    return true;
  }
  if (abs(a.voc_index - b.voc_index) >= 20) {
    return true;
  }
  if (sa.fan_on != sb.fan_on || sa.led_on != sb.led_on ||
      sa.pump_on != sb.pump_on || sa.servo_active != sb.servo_active) {
    return true;
  }
  return false;
}

bool PublishEvent(const String& event_type, const String& detail,
                  const String& level = "WARN") {
  g_display_manager.PushEvent(event_type.c_str(), level.c_str());
  return g_cloud_client.PublishEvent(event_type, level, detail);
}

void PublishAck(const String& request_id, const AckResult result,
                const String& reason, const String& command_name) {
  CommandAck ack;
  ack.request_id = request_id;
  ack.command_name = command_name;
  ack.result = result;
  ack.reason = reason;
  ack.ts = NowTs();
  g_cloud_client.PublishAck(ack);
}

void ApplyConfigField(JsonVariantConst params, const char* key, int& value) {
  if (params[key].isNull()) {
    return;
  }
  value = params[key].as<int>();
}

void ApplyConfigField(JsonVariantConst params, const char* key, uint32_t& value) {
  if (params[key].isNull()) {
    return;
  }
  value = params[key].as<uint32_t>();
}

void ApplyConfigField(JsonVariantConst params, const char* key, bool& value) {
  if (params[key].isNull()) {
    return;
  }
  value = params[key].as<bool>();
}

void ApplyConfigField(JsonVariantConst params, const char* key, String& value) {
  if (params[key].isNull()) {
    return;
  }
  value = params[key].as<String>();
}

void ApplyConfigFromJson(JsonVariantConst params, DeviceConfig& config) {
  ApplyConfigField(params, "sample_interval_ms", config.sample_interval_ms);
  ApplyConfigField(params, "publish_interval_ms", config.publish_interval_ms);
  ApplyConfigField(params, "change_publish_min_interval_ms",
                   config.change_publish_min_interval_ms);
  ApplyConfigField(params, "food_low_threshold_g", config.food_low_threshold_g);
  ApplyConfigField(params, "feed_target_g", config.feed_target_g);
  ApplyConfigField(params, "feed_max_run_ms", config.feed_max_run_ms);
  ApplyConfigField(params, "water_debounce_ms", config.water_debounce_ms);
  ApplyConfigField(params, "water_ok_hold_ms", config.water_ok_hold_ms);
  ApplyConfigField(params, "pump_max_run_ms", config.pump_max_run_ms);
  ApplyConfigField(params, "pump_cooldown_ms", config.pump_cooldown_ms);
  ApplyConfigField(params, "voc_on_threshold", config.voc_on_threshold);
  ApplyConfigField(params, "voc_off_threshold", config.voc_off_threshold);
  ApplyConfigField(params, "voc_debounce_ms", config.voc_debounce_ms);
  ApplyConfigField(params, "voc_ok_hold_ms", config.voc_ok_hold_ms);
  ApplyConfigField(params, "fan_min_on_ms", config.fan_min_on_ms);
  ApplyConfigField(params, "fan_max_on_ms", config.fan_max_on_ms);
  ApplyConfigField(params, "light_on_threshold", config.light_on_threshold);
  ApplyConfigField(params, "light_off_threshold", config.light_off_threshold);
  ApplyConfigField(params, "night_mode_enabled", config.night_mode_enabled);
  ApplyConfigField(params, "night_mode_start", config.night_mode_start);
  ApplyConfigField(params, "night_mode_end", config.night_mode_end);
  ApplyConfigField(params, "temp_high_threshold_c", config.temp_high_threshold_c);
  ApplyConfigField(params, "temp_low_threshold_c", config.temp_low_threshold_c);
  ApplyConfigField(params, "hum_high_threshold_rh", config.hum_high_threshold_rh);
  ApplyConfigField(params, "hum_low_threshold_rh", config.hum_low_threshold_rh);
  NormalizeConfig(config);
}

void HandleCloudCommand(const String& cmd, const String& request_id,
                        JsonVariantConst params) {
  if (cmd == "set_config") {
    ApplyConfigFromJson(params, g_config);
    g_config_store.Save(g_config);
    PublishAck(request_id, AckResult::kOk, "CONFIG_APPLIED", "set_config");
    return;
  }

  if (cmd == "set_night_window") {
    ApplyConfigField(params, "night_mode_start", g_config.night_mode_start);
    ApplyConfigField(params, "night_mode_end", g_config.night_mode_end);
    NormalizeConfig(g_config);
    g_config_store.Save(g_config);
    PublishAck(request_id, AckResult::kOk, "NIGHT_WINDOW_APPLIED",
               "set_night_window");
    return;
  }

  if (cmd == "control" || cmd == "actuate") {
    bool ok = true;
    String reason = "OK";
    bool has_effective_paras = false;

    if (!params["led_on"].isNull()) {
      has_effective_paras = true;
      const bool led_target = params["led_on"].as<bool>();
      ok = ok && g_actuator_manager.SetLed(led_target);
    }
    if (!params["fan_on"].isNull()) {
      has_effective_paras = true;
      const bool fan_target = params["fan_on"].as<bool>();
      ok = ok && g_actuator_manager.SetFan(fan_target);
    }
    if (!params["pump_on"].isNull()) {
      has_effective_paras = true;
      const bool pump_target = params["pump_on"].as<bool>();
      g_pump_locked = false; // "仅手动命令或明确复位动作才能清锁"
      const bool pump_ok = g_actuator_manager.SetPump(pump_target);
      ok = ok && pump_ok;
      if (!pump_ok) {
        reason = "PUMP_SERVO_INTERLOCK";
      }
    }

    bool feed_requested = false;
    if (!params["feed"].isNull()) {
      feed_requested = params["feed"].as<bool>();
    }
    if (!params["feed_once"].isNull()) {
      feed_requested = feed_requested || params["feed_once"].as<bool>();
    }
    if (!params["feed_target_g"].isNull()) {
      g_config.feed_target_g = params["feed_target_g"].as<int>();
      NormalizeConfig(g_config);
      g_config_store.Save(g_config);
    }
    if (feed_requested) {
      has_effective_paras = true;
      const bool feed_ok =
          g_actuator_manager.StartFeed(millis(), g_config.feed_max_run_ms, g_config.feed_target_g);
      ok = ok && feed_ok;
      if (!feed_ok) {
        PublishEvent("FEED_FAILED", "manual_feed_interlock");
        reason = "FEED_INTERLOCK_REJECTED";
      } else {
        g_pending_feed_request_id = request_id;
        g_pending_feed_command_name = "control";
        g_pending_feed_active = true;
      }
    }

    if (g_pending_feed_active && feed_requested && ok) {
      // Do not publish ACK now, wait for feed to finish
      return;
    }

    if (!has_effective_paras) {
      PublishAck(request_id, AckResult::kFailed, "NO_EFFECTIVE_PARAS",
                 "control");
      return;
    }

    AckResult ack_result = ok ? AckResult::kOk : AckResult::kFailed;
    if (!ok && reason.indexOf("INTERLOCK") >= 0) {
      ack_result = AckResult::kRejected;
    }
    PublishAck(request_id, ack_result, reason, "control");
    return;
  }

  if (cmd == "feed_once") {
    if (!params["feed_target_g"].isNull()) {
      g_config.feed_target_g = params["feed_target_g"].as<int>();
      NormalizeConfig(g_config);
      g_config_store.Save(g_config);
    }
    const bool ok =
        g_actuator_manager.StartFeed(millis(), g_config.feed_max_run_ms, g_config.feed_target_g);
    if (!ok) {
      PublishEvent("FEED_FAILED", "manual_feed_interlock");
      PublishAck(request_id, AckResult::kRejected, "FEED_INTERLOCK_REJECTED",
                 "feed_once");
      return;
    }
    g_pending_feed_request_id = request_id;
    g_pending_feed_command_name = "feed_once";
    g_pending_feed_active = true;
    return;
  }

  PublishAck(request_id, AckResult::kFailed, "UNKNOWN_COMMAND", cmd);
}

uint32_t WiFiRetryBackoffMs(const uint32_t retry_count) {
  switch (retry_count) {
    case 0:
      return 0;
    case 1:
      return 1000;
    case 2:
      return 2000;
    case 3:
      return 5000;
    default:
      return 10000;
  }
}

void StartApProvisionIfNeeded() {
  if (!g_ap_manager.IsActive()) {
    g_ap_manager.Begin(BuildChipSuffix());
  }
}

void ManageWiFiConnectivity(const uint32_t now_ms) {
  if (WiFi.status() == WL_CONNECTED) {
    g_wifi_retry_count = 0;
    g_next_wifi_retry_at_ms = now_ms;
    return;
  }

  if (g_config.wifi_ssid.isEmpty()) {
    StartApProvisionIfNeeded();
    return;
  }

  if (g_ap_manager.IsActive()) {
    return;
  }

  if (now_ms < g_next_wifi_retry_at_ms) {
    return;
  }

  const bool ok = g_cloud_client.ConnectWiFi(g_config.wifi_ssid,
                                             g_config.wifi_password,
                                             kWiFiConnectTimeoutMs);
  if (ok) {
    g_wifi_retry_count = 0;
    g_next_wifi_retry_at_ms = now_ms;
    g_cloud_client.EnsureTimeSync(10000);
    return;
  }

  g_wifi_retry_count++;
  if (g_wifi_retry_count >= kMaxWiFiRetryBeforeAp) {
    StartApProvisionIfNeeded();
    return;
  }
  g_next_wifi_retry_at_ms = now_ms + WiFiRetryBackoffMs(g_wifi_retry_count);
}

void ProcessConnectivityEvents(const uint32_t now_ms) {
  const bool wifi_connected = (WiFi.status() == WL_CONNECTED);
  const bool cloud_online = g_cloud_client.IsOnline();

  if (g_wifi_was_connected && !wifi_connected) {
    g_pending_wifi_offline_event = true;
  }
  if (g_cloud_was_online && !cloud_online) {
    g_pending_cloud_offline_event = true;
  }

  g_wifi_was_connected = wifi_connected;
  g_cloud_was_online = cloud_online;

  if (!cloud_online) {
    return;
  }
  if ((now_ms - g_last_offline_event_try_ms) < kOfflineEventRetryMs) {
    return;
  }

  if (g_pending_wifi_offline_event) {
    const bool sent = PublishEvent("WIFI_OFFLINE", "wifi_connection_lost",
                                   "ERROR");
    if (sent) {
      g_pending_wifi_offline_event = false;
    }
    g_last_offline_event_try_ms = now_ms;
    return;
  }

  if (g_pending_cloud_offline_event) {
    const bool sent = PublishEvent("CLOUD_OFFLINE", "cloud_connection_lost",
                                   "ERROR");
    if (sent) {
      g_pending_cloud_offline_event = false;
    }
    g_last_offline_event_try_ms = now_ms;
  }
}

void AutoControlLoop(const uint32_t now_ms, const SensorSnapshot& snapshot) {
  RuntimeStatus status = g_actuator_manager.GetStatus();

  if (snapshot.food_g < g_config.food_low_threshold_g) {
    if (g_low_food_since_ms == 0) {
      g_low_food_since_ms = now_ms;
    }
  } else {
    g_low_food_since_ms = 0;
    g_low_food_event_active = false;
    g_auto_feed_waiting_recover = false;
  }

  if (control_rules::ShouldTriggerLowFoodEvent(
          snapshot.food_g, g_config.food_low_threshold_g, kFeedLowHoldMs,
          g_low_food_since_ms, now_ms, g_low_food_event_active)) {
    PublishEvent("LOW_FOOD", "food_weight_low");
    g_low_food_event_active = true;
  }

  if (control_rules::ShouldTriggerFeed(snapshot.food_g, g_config.food_low_threshold_g,
                                       kFeedLowHoldMs, status.servo_active,
                                       g_auto_feed_waiting_recover,
                                       g_low_food_since_ms, now_ms)) {
    const bool ok =
        g_actuator_manager.StartFeed(now_ms, g_config.feed_max_run_ms, g_config.feed_target_g);
    if (!ok) {
      PublishEvent("FEED_FAILED", "auto_feed_interlock");
    } else {
      g_auto_feed_waiting_recover = true;
    }
  }

  const bool water_low = snapshot.water_low;
  if (water_low) {
    if (g_water_low_since_ms == 0) {
      g_water_low_since_ms = now_ms;
    }
    g_water_ok_since_ms = 0;
  } else {
    g_water_low_since_ms = 0;
    g_low_water_event_active = false;
    if (g_water_ok_since_ms == 0) {
      g_water_ok_since_ms = now_ms;
    }
  }

  if (control_rules::ShouldTriggerLowWaterEvent(
          water_low, g_config.water_debounce_ms, g_water_low_since_ms, now_ms,
          g_low_water_event_active)) {
    PublishEvent("LOW_WATER", "water_level_low");
    g_low_water_event_active = true;
  }

  status = g_actuator_manager.GetStatus();
  if (!status.pump_on && water_low && now_ms >= g_pump_cooldown_until_ms && !g_pump_locked) {
    if (control_rules::ShouldStartRefill(water_low, g_config.water_debounce_ms,
                                         status.pump_on, g_water_low_since_ms,
                                         now_ms)) {
      const bool ok = g_actuator_manager.SetPump(true);
      if (ok) {
        g_pump_started_ms = now_ms;
      }
    }
  }

  status = g_actuator_manager.GetStatus();
  if (status.pump_on && !water_low &&
      control_rules::ShouldStopRefill(true, g_config.water_ok_hold_ms,
                                      g_water_ok_since_ms, now_ms)) {
    g_actuator_manager.SetPump(false);
    g_pump_cooldown_until_ms = now_ms + g_config.pump_cooldown_ms;
  }

  status = g_actuator_manager.GetStatus();
  if (status.pump_on && g_pump_started_ms > 0 &&
      (now_ms - g_pump_started_ms) >= g_config.pump_max_run_ms) {
    g_actuator_manager.SetPump(false);
    g_pump_cooldown_until_ms = now_ms + g_config.pump_cooldown_ms;
    g_pump_locked = true;
    PublishEvent("WATER_REFILL_FAILED", "pump_timeout_lock");
  }

  if (snapshot.voc_index >= g_config.voc_on_threshold) {
    if (g_voc_high_since_ms == 0) {
      g_voc_high_since_ms = now_ms;
    }
  } else {
    g_voc_high_since_ms = 0;
  }

  if (snapshot.voc_index <= g_config.voc_off_threshold) {
    if (g_voc_low_since_ms == 0) {
      g_voc_low_since_ms = now_ms;
    }
  } else {
    g_voc_low_since_ms = 0;
  }

  status = g_actuator_manager.GetStatus();
  if (control_rules::ShouldStartFan(snapshot.voc_index, g_config.voc_on_threshold,
                                    g_config.voc_debounce_ms, g_voc_high_since_ms,
                                    status.fan_on, now_ms)) {
    if (g_actuator_manager.SetFan(true)) {
      g_fan_started_ms = now_ms;
      g_fan_overtime_reported = false;
    }
  }

  status = g_actuator_manager.GetStatus();
  if (status.fan_on && g_fan_started_ms > 0 &&
      (now_ms - g_fan_started_ms) >= g_config.fan_max_on_ms) {
    // 风扇运行超时，仅告警不停止（PRD: 超时告警但可继续运行，策略可配置）
    if (!g_fan_overtime_reported) {
      PublishEvent("FAN_OVERTIME", "fan_exceeded_max_on_ms", "WARN");
      g_fan_overtime_reported = true;
    }
  }

  status = g_actuator_manager.GetStatus();
  if (status.fan_on && g_fan_started_ms > 0 &&
      (now_ms - g_fan_started_ms) >= g_config.fan_min_on_ms &&
      control_rules::ShouldStopFan(snapshot.voc_index, g_config.voc_off_threshold,
                                   g_config.voc_ok_hold_ms, g_voc_low_since_ms,
                                   true, now_ms)) {
    g_actuator_manager.SetFan(false);
  }

  const bool in_night_window = IsWithinNightWindow(g_config);
  status = g_actuator_manager.GetStatus();
  if (!status.led_on &&
      control_rules::ShouldTurnOnLight(snapshot.lux, g_config.light_on_threshold,
                                       g_config.night_mode_enabled,
                                       in_night_window)) {
    g_actuator_manager.SetLed(true);
  }
  status = g_actuator_manager.GetStatus();
  if (status.led_on &&
      control_rules::ShouldTurnOffLight(snapshot.lux, g_config.light_off_threshold,
                                        g_config.night_mode_enabled,
                                        in_night_window)) {
    g_actuator_manager.SetLed(false);
  }
  
  bool temp_hum_fault = false;
  String fault_detail;
  if (g_sensor_manager.IsFaulty()) { temp_hum_fault = true; fault_detail = g_sensor_manager.GetLastError(); }
  else if (snapshot.temp_c > g_config.temp_high_threshold_c) { temp_hum_fault = true; fault_detail = "temp_high"; }
  else if (snapshot.temp_c < g_config.temp_low_threshold_c) { temp_hum_fault = true; fault_detail = "temp_low"; }
  else if (snapshot.hum_rh > g_config.hum_high_threshold_rh) { temp_hum_fault = true; fault_detail = "hum_high"; }
  else if (snapshot.hum_rh < g_config.hum_low_threshold_rh) { temp_hum_fault = true; fault_detail = "hum_low"; }
  
  if (temp_hum_fault) {
      if (g_temp_hum_fault_since_ms == 0) {
          g_temp_hum_fault_since_ms = now_ms;
      } else if (now_ms - g_temp_hum_fault_since_ms >= 10000) { // 超过10秒后上报异常
          PublishEvent("SENSOR_FAULT", fault_detail, "WARN");
          g_temp_hum_fault_since_ms = now_ms; // 每10秒重复上报
      }
  } else {
      g_temp_hum_fault_since_ms = 0;
  }
}

void PublishIfNeeded(const uint32_t now_ms, const SensorSnapshot& snapshot) {
  RuntimeStatus status = g_actuator_manager.GetStatus();
  if (g_sensor_manager.IsFaulty() && status.last_error.isEmpty()) {
    status.last_error = g_sensor_manager.GetLastError();
  }

  const bool periodic_due =
      (now_ms - g_last_publish_ms) >= g_config.publish_interval_ms;
  const bool changed = HasSignificantChange(snapshot, g_last_published_snapshot,
                                            status, g_last_published_status);
  const bool change_due =
      changed && ((now_ms - g_last_change_publish_ms) >=
                  g_config.change_publish_min_interval_ms);

  if (!periodic_due && !change_due) {
    return;
  }

  if (g_cloud_client.PublishTelemetry(snapshot, status)) {
    g_last_publish_ms = now_ms;
    if (change_due) {
      g_last_change_publish_ms = now_ms;
    }
    g_last_published_snapshot = snapshot;
    g_last_published_status = status;
  }
}

void OnSingleClick() {
  if (g_in_settings) {
    if (g_editing_item) {
      // 单击在编辑模式：交替增/减
      static bool s_increase_next = true;
      if (s_increase_next) {
        g_display_manager.IncreaseSettingValue(g_config);
      } else {
        g_display_manager.DecreaseSettingValue(g_config);
      }
      s_increase_next = !s_increase_next;
    } else {
      // 切换参数项
      g_display_manager.NextSettingItem();
    }
  } else if (g_display_manager.GetCurrentPage() == DisplayPage::kAlerts &&
             g_display_manager.CanScrollAlertNext()) {
    // 告警页有更多记录时，单击向下滚动
    g_display_manager.NextAlertPage();
  } else {
    // 切换到下一个显示页面（告警页滚动到底部时也会切页）
    g_display_manager.NextPage();
  }
  g_display_manager.Update(g_latest_snapshot, g_actuator_manager.GetStatus(), g_config, g_in_settings, g_editing_item);
}

void OnDoubleClick() {
  if (g_in_settings) {
    if (g_editing_item) {
      // 双击退出编辑模式回到选择模式
      g_editing_item = false;
    } else {
      // 双击退出设置模式
      g_in_settings = false;
    }
  } else {
    // 双击进入设置模式
    g_in_settings = true;
    g_editing_item = false;
    g_display_manager.SetPage(DisplayPage::kSettings);
    g_display_manager.ResetSettingIndex();
  }
  g_display_manager.Update(g_latest_snapshot, g_actuator_manager.GetStatus(), g_config, g_in_settings, g_editing_item);
}

void OnLongPress2s() {
  if (!g_in_settings) {
    return;
  }

  if (!g_editing_item) {
    // 首次长按2秒进入编辑态
    g_editing_item = true;
    g_display_manager.Update(g_latest_snapshot, g_actuator_manager.GetStatus(),
                             g_config, g_in_settings, g_editing_item);
    return;
  }

  // 编辑态长按2秒确认保存并退出编辑态
  NormalizeConfig(g_config);
  g_config_store.Save(g_config);
  PublishEvent("CONFIG_CONFIRMED", "local_button", "INFO");
  g_editing_item = false;
  g_display_manager.Update(g_latest_snapshot, g_actuator_manager.GetStatus(),
                           g_config, g_in_settings, g_editing_item);
}

void OnLongPress5s() {
  if (g_in_settings) {
    // 长按5秒恢复默认
    String wifi_s = g_config.wifi_ssid;
    String wifi_p = g_config.wifi_password;
    g_config = DeviceConfig();
    g_config.wifi_ssid = wifi_s;
    g_config.wifi_password = wifi_p;
    NormalizeConfig(g_config);
    g_config_store.Save(g_config);
    PublishEvent("CONFIG_RESET_DEFAULT", "local_button", "WARN");
    g_in_settings = false;
    g_editing_item = false;
    g_display_manager.Update(g_latest_snapshot, g_actuator_manager.GetStatus(), g_config, g_in_settings, g_editing_item);
  }
}

}  // namespace

#if !defined(PIO_UNIT_TESTING)
void setup() {
  Serial.begin(115200);
  delay(300);

  g_config_store.Begin();
  g_config_store.Load(g_config);

  g_sensor_manager.Begin();
  g_actuator_manager.Begin();

  g_cloud_client.Begin(secrets::kMqttHost, secrets::kMqttPort, secrets::kDeviceId,
                       secrets::kDeviceSecret);
  g_cloud_client.SetCommandHandler(HandleCloudCommand);

  g_display_manager.Begin();
  g_button_manager.Begin();
  g_button_manager.SetOnClick(OnSingleClick);
  g_button_manager.SetOnDoubleClick(OnDoubleClick);
  g_button_manager.SetOnLongPress2s(OnLongPress2s);
  g_button_manager.SetOnLongPress5s(OnLongPress5s);

  ManageWiFiConnectivity(0);
  g_wifi_was_connected = (WiFi.status() == WL_CONNECTED);
  g_cloud_was_online = false;
}

void loop() {
  const uint32_t now_ms = millis();

  ManageWiFiConnectivity(now_ms);
  g_ap_manager.Loop();
  if (g_ap_manager.IsActive() && g_ap_manager.HasProvisionedCredentials()) {
    const String ssid = g_ap_manager.ProvisionedSsid();
    const String pwd = g_ap_manager.ProvisionedPassword();
    g_ap_manager.ClearProvisionedFlag();
    if (!ssid.isEmpty()) {
      g_config.wifi_ssid = ssid;
      g_config.wifi_password = pwd;
      g_config_store.SaveWiFi(ssid, pwd);
      delay(300);
      ESP.restart();
    }
  }

  g_cloud_client.Loop();
  ProcessConnectivityEvents(now_ms);
  g_actuator_manager.SetOnlineStatus(g_cloud_was_online);
  g_actuator_manager.Loop(now_ms);
  g_button_manager.Loop();

  if (g_actuator_manager.CheckAndClearFeedOk()) {
    if (g_pending_feed_active) {
      PublishAck(g_pending_feed_request_id, AckResult::kOk, "FEED_COMPLETED", g_pending_feed_command_name);
      g_pending_feed_active = false;
    }
  } else if (g_actuator_manager.CheckAndClearFeedTimeout()) {
    PublishEvent("FEED_FAILED", "feed_exceeded_max_run_ms");
    if (g_pending_feed_active) {
      PublishAck(g_pending_feed_request_id, AckResult::kFailed, "FEED_TIMEOUT", g_pending_feed_command_name);
      g_pending_feed_active = false;
    }
  }

  if ((now_ms - g_last_sample_ms) < g_config.sample_interval_ms) {
    delay(5);
    return;
  }

  g_last_sample_ms = now_ms;
  g_latest_snapshot = g_sensor_manager.Read();
  AutoControlLoop(now_ms, g_latest_snapshot);
  PublishIfNeeded(now_ms, g_latest_snapshot);
  g_display_manager.Update(g_latest_snapshot, g_actuator_manager.GetStatus(), g_config, g_in_settings, g_editing_item);
}
#endif  // !defined(PIO_UNIT_TESTING)

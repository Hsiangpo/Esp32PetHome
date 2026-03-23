#include "config_store.h"

#include <Arduino.h>

namespace {
constexpr char kNamespace[] = "pet_home_cfg";
}  // namespace

bool ConfigStore::Begin() {
  if (started_) {
    return true;
  }
  started_ = preferences_.begin(kNamespace, false);
  return started_;
}

bool ConfigStore::Load(DeviceConfig& config) {
  if (!Begin()) {
    return false;
  }

  config.wifi_ssid = preferences_.getString("wifi_ssid", "");
  config.wifi_password = preferences_.getString("wifi_pwd", "");
  config.sample_interval_ms =
      preferences_.getUInt("sample_ms", config.sample_interval_ms);
  config.publish_interval_ms =
      preferences_.getUInt("publish_ms", config.publish_interval_ms);
  config.change_publish_min_interval_ms = preferences_.getUInt(
      "change_pub_ms", config.change_publish_min_interval_ms);

  config.food_low_threshold_g =
      preferences_.getInt("food_low_g", config.food_low_threshold_g);
  config.feed_target_g = preferences_.getInt("feed_g", config.feed_target_g);
  config.feed_max_run_ms =
      preferences_.getUInt("feed_max_ms", config.feed_max_run_ms);

  config.water_debounce_ms =
      preferences_.getUInt("water_db_ms", config.water_debounce_ms);
  config.water_ok_hold_ms =
      preferences_.getUInt("water_ok_ms", config.water_ok_hold_ms);
  config.pump_max_run_ms =
      preferences_.getUInt("pump_max_ms", config.pump_max_run_ms);
  config.pump_cooldown_ms =
      preferences_.getUInt("pump_cool_ms", config.pump_cooldown_ms);

  config.voc_on_threshold = preferences_.getInt("voc_on", config.voc_on_threshold);
  config.voc_off_threshold =
      preferences_.getInt("voc_off", config.voc_off_threshold);
  config.voc_debounce_ms =
      preferences_.getUInt("voc_db_ms", config.voc_debounce_ms);
  config.voc_ok_hold_ms =
      preferences_.getUInt("voc_ok_ms", config.voc_ok_hold_ms);
  config.fan_min_on_ms = preferences_.getUInt("fan_min_ms", config.fan_min_on_ms);
  config.fan_max_on_ms = preferences_.getUInt("fan_max_ms", config.fan_max_on_ms);

  config.temp_high_threshold_c = preferences_.getInt("temp_high", config.temp_high_threshold_c);
  config.temp_low_threshold_c = preferences_.getInt("temp_low", config.temp_low_threshold_c);
  config.hum_high_threshold_rh = preferences_.getInt("hum_high", config.hum_high_threshold_rh);
  config.hum_low_threshold_rh = preferences_.getInt("hum_low", config.hum_low_threshold_rh);

  config.light_on_threshold =
      preferences_.getInt("light_on", config.light_on_threshold);
  config.light_off_threshold =
      preferences_.getInt("light_off", config.light_off_threshold);
  config.night_mode_enabled =
      preferences_.getBool("night_en", config.night_mode_enabled);
  config.night_mode_start =
      preferences_.getString("night_s", config.night_mode_start);
  config.night_mode_end = preferences_.getString("night_e", config.night_mode_end);

  NormalizeConfig(config);
  return true;
}

bool ConfigStore::Save(const DeviceConfig& source_config) {
  if (!Begin()) {
    return false;
  }

  DeviceConfig config = source_config;
  NormalizeConfig(config);

  preferences_.putString("wifi_ssid", config.wifi_ssid);
  preferences_.putString("wifi_pwd", config.wifi_password);
  preferences_.putUInt("sample_ms", config.sample_interval_ms);
  preferences_.putUInt("publish_ms", config.publish_interval_ms);
  preferences_.putUInt("change_pub_ms", config.change_publish_min_interval_ms);

  preferences_.putInt("food_low_g", config.food_low_threshold_g);
  preferences_.putInt("feed_g", config.feed_target_g);
  preferences_.putUInt("feed_max_ms", config.feed_max_run_ms);

  preferences_.putUInt("water_db_ms", config.water_debounce_ms);
  preferences_.putUInt("water_ok_ms", config.water_ok_hold_ms);
  preferences_.putUInt("pump_max_ms", config.pump_max_run_ms);
  preferences_.putUInt("pump_cool_ms", config.pump_cooldown_ms);

  preferences_.putInt("voc_on", config.voc_on_threshold);
  preferences_.putInt("voc_off", config.voc_off_threshold);
  preferences_.putUInt("voc_db_ms", config.voc_debounce_ms);
  preferences_.putUInt("voc_ok_ms", config.voc_ok_hold_ms);
  preferences_.putUInt("fan_min_ms", config.fan_min_on_ms);
  preferences_.putUInt("fan_max_ms", config.fan_max_on_ms);

  preferences_.putInt("temp_high", config.temp_high_threshold_c);
  preferences_.putInt("temp_low", config.temp_low_threshold_c);
  preferences_.putInt("hum_high", config.hum_high_threshold_rh);
  preferences_.putInt("hum_low", config.hum_low_threshold_rh);

  preferences_.putInt("light_on", config.light_on_threshold);
  preferences_.putInt("light_off", config.light_off_threshold);
  preferences_.putBool("night_en", config.night_mode_enabled);
  preferences_.putString("night_s", config.night_mode_start);
  preferences_.putString("night_e", config.night_mode_end);

  return true;
}

bool ConfigStore::SaveWiFi(const String& ssid, const String& password) {
  if (!Begin()) {
    return false;
  }
  preferences_.putString("wifi_ssid", ssid);
  preferences_.putString("wifi_pwd", password);
  return true;
}

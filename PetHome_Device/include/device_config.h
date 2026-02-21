#ifndef PET_HOME_DEVICE_CONFIG_H_
#define PET_HOME_DEVICE_CONFIG_H_

#include <Arduino.h>

struct DeviceConfig {
  String wifi_ssid;
  String wifi_password;

  int food_low_threshold_g = 50;
  int feed_target_g = 50;
  uint32_t feed_max_run_ms = 20000;

  uint32_t water_debounce_ms = 3000;
  uint32_t water_ok_hold_ms = 2000;
  uint32_t pump_max_run_ms = 30000;
  uint32_t pump_cooldown_ms = 60000;

  int voc_on_threshold = 250;
  int voc_off_threshold = 200;
  uint32_t voc_debounce_ms = 5000;
  uint32_t voc_ok_hold_ms = 10000;
  uint32_t fan_min_on_ms = 60000;
  uint32_t fan_max_on_ms = 1800000;

  int temp_high_threshold_c = 30;
  int temp_low_threshold_c = 10;
  int hum_high_threshold_rh = 70;
  int hum_low_threshold_rh = 30;

  int light_on_threshold = 100;
  int light_off_threshold = 150;
  bool night_mode_enabled = false;
  String night_mode_start = "22:00";
  String night_mode_end = "07:00";
};

inline int ClampInt(const int value, const int min_value, const int max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

inline uint32_t ClampUint32(const uint32_t value, const uint32_t min_value,
                            const uint32_t max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

inline void NormalizeConfig(DeviceConfig& config) {
  config.food_low_threshold_g = ClampInt(config.food_low_threshold_g, 10, 500);
  config.feed_target_g = ClampInt(config.feed_target_g, 10, 150);
  config.feed_max_run_ms = ClampUint32(config.feed_max_run_ms, 3000, 60000);

  config.water_debounce_ms = ClampUint32(config.water_debounce_ms, 500, 10000);
  config.water_ok_hold_ms = ClampUint32(config.water_ok_hold_ms, 500, 10000);
  config.pump_max_run_ms = ClampUint32(config.pump_max_run_ms, 5000, 120000);
  config.pump_cooldown_ms = ClampUint32(config.pump_cooldown_ms, 5000, 180000);

  config.voc_on_threshold = ClampInt(config.voc_on_threshold, 50, 500);
  config.voc_off_threshold = ClampInt(config.voc_off_threshold, 0, 450);
  if (config.voc_off_threshold > config.voc_on_threshold) {
    config.voc_off_threshold = config.voc_on_threshold;
  }
  config.voc_debounce_ms = ClampUint32(config.voc_debounce_ms, 500, 30000);
  config.voc_ok_hold_ms = ClampUint32(config.voc_ok_hold_ms, 500, 60000);
  config.fan_min_on_ms = ClampUint32(config.fan_min_on_ms, 5000, 300000);
  config.fan_max_on_ms = ClampUint32(config.fan_max_on_ms, 60000, 7200000);
  if (config.fan_max_on_ms < config.fan_min_on_ms) {
    config.fan_max_on_ms = config.fan_min_on_ms;
  }

  config.temp_high_threshold_c = ClampInt(config.temp_high_threshold_c, 20, 40);
  config.temp_low_threshold_c = ClampInt(config.temp_low_threshold_c, 0, 20);
  config.hum_high_threshold_rh = ClampInt(config.hum_high_threshold_rh, 40, 90);
  config.hum_low_threshold_rh = ClampInt(config.hum_low_threshold_rh, 10, 60);

  config.light_on_threshold = ClampInt(config.light_on_threshold, 10, 1000);
  config.light_off_threshold = ClampInt(config.light_off_threshold, 20, 1200);
  if (config.light_off_threshold < config.light_on_threshold) {
    config.light_off_threshold = config.light_on_threshold;
  }
}

#endif  // PET_HOME_DEVICE_CONFIG_H_

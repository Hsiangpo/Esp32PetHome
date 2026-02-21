#ifndef PET_HOME_TYPES_H_
#define PET_HOME_TYPES_H_

#include <Arduino.h>

struct SensorSnapshot {
  uint64_t ts = 0;
  float temp_c = 0.0F;
  float hum_rh = 0.0F;
  float lux = 0.0F;
  float food_g = 0.0F;
  bool water_low = false;
  int voc_index = 0;
  int tvoc_ppb = 0;
};

struct RuntimeStatus {
  bool fan_on = false;
  bool led_on = false;
  bool pump_on = false;
  bool servo_active = false;
  bool online = false;
  String last_error;
};

enum class AckResult {
  kOk,
  kRejected,
  kFailed,
};

struct CommandAck {
  String request_id;
  String command_name;
  AckResult result = AckResult::kOk;
  String reason;
  uint64_t ts = 0;
};

#endif  // PET_HOME_TYPES_H_

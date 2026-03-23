#include "sensor_manager.h"

#include <Arduino.h>
#include <time.h>
#include <Wire.h>
#include "board_pins.h"

namespace {
int ClampInt(const int value, const int min_value, const int max_value) {
  if (value < min_value) {
    return min_value;
  }
  if (value > max_value) {
    return max_value;
  }
  return value;
}

uint64_t CurrentUnixMs() {
  const time_t now = time(nullptr);
  if (now > 1700000000) {
    return static_cast<uint64_t>(now) * 1000ULL;
  }
  return static_cast<uint64_t>(millis());
}
}  // namespace

void SensorManager::Begin() {
  pinMode(board_pins::kWaterLevelDigital, INPUT_PULLUP);
  
  Wire.begin(board_pins::kI2cSda, board_pins::kI2cScl);
  
  sht31_.begin(0x44);
  sht31_ok_ = sht31_.isConnected();
  
  sgp30_ok_ = sgp30_.begin();
  
  bh1750_ok_ = bh1750_.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  
  hx711_.begin(board_pins::kFoodScaleDt, board_pins::kFoodScaleSck);
  hx711_.set_scale(400.0f); // 默认校准比例
  hx711_.tare();
  hx711_ok_ = true;
}

void SensorManager::PushEnvHistory(const float temp_c, const float hum_rh,
                                   const uint32_t ts_ms) {
  temp_history_[history_head_] = temp_c;
  hum_history_[history_head_] = hum_rh;
  history_ts_ms_[history_head_] = ts_ms;
  history_head_ = (history_head_ + 1U) % kEnvHistoryCapacity;
  if (history_size_ < kEnvHistoryCapacity) {
    history_size_++;
  }
}

void SensorManager::ComputeEnvAverage(const uint32_t now_ms, float& avg_temp_c,
                                      float& avg_hum_rh) const {
  float temp_sum = 0.0f;
  float hum_sum = 0.0f;
  size_t valid_count = 0;

  for (size_t i = 0; i < history_size_; ++i) {
    const size_t offset = (history_head_ + kEnvHistoryCapacity - 1U - i) %
                          kEnvHistoryCapacity;
    const uint32_t sample_ts = history_ts_ms_[offset];
    if (now_ms < sample_ts) {
      continue;
    }
    if ((now_ms - sample_ts) > kEnvAverageWindowMs) {
      continue;
    }
    temp_sum += temp_history_[offset];
    hum_sum += hum_history_[offset];
    valid_count++;
  }

  if (valid_count == 0U) {
    avg_temp_c = last_temp_;
    avg_hum_rh = last_hum_;
    return;
  }

  avg_temp_c = temp_sum / static_cast<float>(valid_count);
  avg_hum_rh = hum_sum / static_cast<float>(valid_count);
}

SensorSnapshot SensorManager::Read() {
  SensorSnapshot snapshot;
  snapshot.ts = CurrentUnixMs();
  const uint32_t now_ms = millis();
  
  failed_ = false;
  last_error_ = "";

  if (sht31_ok_ && sht31_.read()) {
    last_temp_ = sht31_.getTemperature();
    last_hum_ = sht31_.getHumidity();
  } else if (sht31_ok_) {
    failed_ = true;
    last_error_ = "SHT31_READ_FAIL";
  } else {
    failed_ = true;
    last_error_ = "SHT31_NOT_CONNECTED";
  }

  if (sgp30_ok_ && sgp30_.IAQmeasure()) {
    last_tvoc_ = sgp30_.TVOC;
  } else if (sgp30_ok_) {
    failed_ = true;
    last_error_ = "SGP30_READ_FAIL";
  } else {
    failed_ = true;
    last_error_ = "SGP30_NOT_CONNECTED";
  }

  if (bh1750_ok_) {
    float lux = bh1750_.readLightLevel();
    if (lux >= 0) last_lux_ = lux;
  } else {
    failed_ = true;
    last_error_ = "BH1750_NOT_CONNECTED";
  }

  if (hx711_ok_ && hx711_.is_ready()) {
    float f = hx711_.get_units(1);
    if (f < 0.0f) f = 0.0f;
    last_food_ = f;
  }

  PushEnvHistory(last_temp_, last_hum_, now_ms);
  float avg_temp_c = last_temp_;
  float avg_hum_rh = last_hum_;
  ComputeEnvAverage(now_ms, avg_temp_c, avg_hum_rh);

  snapshot.temp_c = avg_temp_c;
  snapshot.hum_rh = avg_hum_rh;
  snapshot.lux = last_lux_;
  snapshot.food_g = last_food_;
  snapshot.water_low = (digitalRead(board_pins::kWaterLevelDigital) == LOW);
  snapshot.tvoc_ppb = last_tvoc_;
  snapshot.voc_index = ClampInt((snapshot.tvoc_ppb + 1) / 2, 0, 500);

  return snapshot;
}

#include "sensor_manager.h"

#include <Arduino.h>
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

SensorSnapshot SensorManager::Read() {
  SensorSnapshot snapshot;
  snapshot.ts = static_cast<uint64_t>(millis());
  
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

  snapshot.temp_c = last_temp_;
  snapshot.hum_rh = last_hum_;
  snapshot.lux = last_lux_;
  snapshot.food_g = last_food_;
  snapshot.water_low = (digitalRead(board_pins::kWaterLevelDigital) == LOW);
  snapshot.tvoc_ppb = last_tvoc_;
  snapshot.voc_index = ClampInt((snapshot.tvoc_ppb + 1) / 2, 0, 500);

  return snapshot;
}

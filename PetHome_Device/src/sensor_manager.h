#ifndef PET_HOME_SENSOR_MANAGER_H_
#define PET_HOME_SENSOR_MANAGER_H_

#include "pet_home_types.h"
#include <SHT31.h>
#include <Adafruit_SGP30.h>
#include <BH1750.h>
#include <HX711.h>

class SensorManager {
 public:
  void Begin();
  SensorSnapshot Read();
  bool IsFaulty() const { return failed_; }
  String GetLastError() const { return last_error_; }

 private:
  void PushEnvHistory(float temp_c, float hum_rh, uint32_t ts_ms);
  void ComputeEnvAverage(uint32_t now_ms, float& avg_temp_c,
                         float& avg_hum_rh) const;

  SHT31 sht31_;
  Adafruit_SGP30 sgp30_;
  BH1750 bh1750_;
  HX711 hx711_;

  bool sht31_ok_ = false;
  bool sgp30_ok_ = false;
  bool bh1750_ok_ = false;
  bool hx711_ok_ = false;

  bool failed_ = false;
  String last_error_ = "";
  
  float last_temp_ = 25.0f;
  float last_hum_ = 50.0f;
  float last_lux_ = 100.0f;
  float last_food_ = 50.0f;
  int last_tvoc_ = 0;

  static constexpr uint32_t kEnvAverageWindowMs = 10000;
  // 最小采样间隔 500ms 时，10 秒窗口至少需要 20 个样本，保留一定冗余。
  static constexpr size_t kEnvHistoryCapacity = 24;
  float temp_history_[kEnvHistoryCapacity] = {0.0f};
  float hum_history_[kEnvHistoryCapacity] = {0.0f};
  uint32_t history_ts_ms_[kEnvHistoryCapacity] = {0};
  size_t history_head_ = 0;
  size_t history_size_ = 0;
};

#endif  // PET_HOME_SENSOR_MANAGER_H_

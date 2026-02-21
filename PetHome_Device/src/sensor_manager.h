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
};

#endif  // PET_HOME_SENSOR_MANAGER_H_

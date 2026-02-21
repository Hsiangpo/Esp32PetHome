#ifndef PET_HOME_CONFIG_STORE_H_
#define PET_HOME_CONFIG_STORE_H_

#include <Preferences.h>

#include "device_config.h"

class ConfigStore {
 public:
  bool Begin();
  bool Load(DeviceConfig& config);
  bool Save(const DeviceConfig& config);
  bool SaveWiFi(const String& ssid, const String& password);

 private:
  Preferences preferences_;
  bool started_ = false;
};

#endif  // PET_HOME_CONFIG_STORE_H_

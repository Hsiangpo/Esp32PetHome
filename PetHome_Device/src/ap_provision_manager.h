#ifndef PET_HOME_AP_PROVISION_MANAGER_H_
#define PET_HOME_AP_PROVISION_MANAGER_H_

#include <Arduino.h>
#include <WebServer.h>

class APProvisionManager {
 public:
  APProvisionManager();

  void Begin(const String& ssid_suffix);
  void Loop();
  bool IsActive() const;
  bool HasProvisionedCredentials() const;
  String ProvisionedSsid() const;
  String ProvisionedPassword() const;
  void ClearProvisionedFlag();

 private:
  void RegisterRoutes();
  String BuildJsonStatus() const;
  String BuildScanResult();

  bool active_ = false;
  bool provisioned_ = false;
  String ssid_;
  String password_;
  String ap_ssid_;
  String ap_password_;
  WebServer server_;
};

#endif  // PET_HOME_AP_PROVISION_MANAGER_H_

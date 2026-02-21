#include "ap_provision_manager.h"

#include <ArduinoJson.h>
#include <WiFi.h>

APProvisionManager::APProvisionManager() : server_(80) {}

void APProvisionManager::Begin(const String& ssid_suffix) {
  if (active_) {
    return;
  }

  ap_ssid_ = "PetHome_Setup_" + ssid_suffix;
  const uint32_t seed = static_cast<uint32_t>((ESP.getEfuseMac() & 0xFFFFFFFFULL) ^
                                               static_cast<uint64_t>(millis()));
  char password_buf[11] = {0};
  snprintf(password_buf, sizeof(password_buf), "ph%08lX",
           static_cast<unsigned long>(seed));
  ap_password_ = String(password_buf);

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid_.c_str(), ap_password_.c_str());

  RegisterRoutes();
  server_.begin();
  active_ = true;
}

void APProvisionManager::Loop() {
  if (!active_) {
    return;
  }
  server_.handleClient();
}

bool APProvisionManager::IsActive() const { return active_; }

bool APProvisionManager::HasProvisionedCredentials() const { return provisioned_; }

String APProvisionManager::ProvisionedSsid() const { return ssid_; }

String APProvisionManager::ProvisionedPassword() const { return password_; }

void APProvisionManager::ClearProvisionedFlag() { provisioned_ = false; }

void APProvisionManager::RegisterRoutes() {
  server_.on("/", HTTP_GET, [this]() { server_.send(200, "application/json", BuildJsonStatus()); });

  server_.on("/status", HTTP_GET,
             [this]() { server_.send(200, "application/json", BuildJsonStatus()); });

  server_.on("/scan", HTTP_GET, [this]() {
    const String body = BuildScanResult();
    server_.send(200, "application/json", body);
  });

  server_.on("/config", HTTP_POST, [this]() {
    String ssid;
    String pwd;

    if (server_.hasArg("plain")) {
      StaticJsonDocument<256> doc;
      DeserializationError err = deserializeJson(doc, server_.arg("plain"));
      if (!err) {
        ssid = doc["ssid"] | "";
        pwd = doc["pwd"] | "";
      }
    }

    if (ssid.isEmpty() && server_.hasArg("ssid")) {
      ssid = server_.arg("ssid");
    }
    if (pwd.isEmpty() && server_.hasArg("pwd")) {
      pwd = server_.arg("pwd");
    }

    if (ssid.isEmpty()) {
      server_.send(400, "application/json",
                   "{\"ok\":false,\"reason\":\"ssid_or_pwd_missing\"}");
      return;
    }
    ssid_ = ssid;
    password_ = pwd;
    provisioned_ = !ssid_.isEmpty();
    server_.send(200, "application/json", "{\"ok\":true,\"message\":\"saved\"}");
  });
}

String APProvisionManager::BuildJsonStatus() const {
  StaticJsonDocument<256> doc;
  doc["active"] = active_;
  doc["ap_ssid"] = ap_ssid_;
  doc["ap_pwd"] = ap_password_;
  doc["provisioned"] = provisioned_;
  String body;
  serializeJson(doc, body);
  return body;
}

String APProvisionManager::BuildScanResult() {
  StaticJsonDocument<1024> doc;
  JsonArray arr = doc.createNestedArray("networks");
  const int count = WiFi.scanNetworks();
  for (int i = 0; i < count && i < 12; ++i) {
    JsonObject item = arr.createNestedObject();
    item["ssid"] = WiFi.SSID(i);
    item["rssi"] = WiFi.RSSI(i);
  }
  String body;
  serializeJson(doc, body);
  return body;
}

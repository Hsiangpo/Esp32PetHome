#ifndef PET_HOME_CLOUD_CLIENT_H_
#define PET_HOME_CLOUD_CLIENT_H_

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClient.h>
#include <functional>

#include "pet_home_types.h"

class CloudClient {
 public:
  using CommandHandler =
      std::function<void(const String&, const String&, JsonVariantConst)>;

  CloudClient();

  void Begin(const String& host, uint16_t port, const String& device_id,
             const String& device_secret);
  bool ConnectWiFi(const String& ssid, const String& password,
                   uint32_t timeout_ms);
  bool EnsureTimeSync(uint32_t timeout_ms);
  void Loop();

  bool IsOnline();
  bool PublishTelemetry(const SensorSnapshot& snapshot,
                        const RuntimeStatus& status);
  bool PublishEvent(const String& event_type, const String& level,
                    const String& detail);
  bool PublishAck(const CommandAck& ack);

  void SetCommandHandler(CommandHandler handler);

 private:
  String TelemetryTopic() const;
  String EventTopic() const;
  String CommandDownTopic() const;
  String CommandUpTopic(const String& request_id) const;

  bool EnsureMqttConnected();
  bool TryExtractRequestId(const String& topic, String& request_id) const;
  String BuildEventTime() const;
  void HandleMqttMessage(char* topic, uint8_t* payload, unsigned int length);
  static void MqttTrampoline(char* topic, uint8_t* payload, unsigned int length);
  int AckResultCode(AckResult result) const;

  static CloudClient* self_;

  WiFiClient wifi_client_;
  PubSubClient mqtt_client_;
  CommandHandler command_handler_;

  String host_;
  uint16_t port_ = 1883;
  String device_id_;
  String device_secret_;
  uint32_t last_mqtt_retry_ms_ = 0;
};

#endif  // PET_HOME_CLOUD_CLIENT_H_

#include "cloud_client.h"

#include <WiFi.h>
#include <time.h>

CloudClient* CloudClient::self_ = nullptr;

CloudClient::CloudClient() : mqtt_client_(wifi_client_) { self_ = this; }

void CloudClient::Begin(const String& host, const uint16_t port,
                        const String& device_id,
                        const String& device_secret) {
  host_ = host;
  port_ = port;
  device_id_ = device_id;
  device_secret_ = device_secret;

  mqtt_client_.setServer(host_.c_str(), port_);
  mqtt_client_.setCallback(CloudClient::MqttTrampoline);
  mqtt_client_.setBufferSize(2048);
}

bool CloudClient::ConnectWiFi(const String& ssid, const String& password,
                              const uint32_t timeout_ms) {
  if (ssid.isEmpty()) {
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());

  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    if ((millis() - start) >= timeout_ms) {
      return false;
    }
  }
  return true;
}

bool CloudClient::EnsureTimeSync(const uint32_t timeout_ms) {
  configTime(8 * 3600, 0, "ntp.aliyun.com", "ntp1.aliyun.com");
  const uint32_t start = millis();
  while ((millis() - start) < timeout_ms) {
    const time_t now = time(nullptr);
    if (now > 1700000000) {
      return true;
    }
    delay(200);
  }
  return false;
}

void CloudClient::Loop() {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }
  EnsureMqttConnected();
  mqtt_client_.loop();
}

bool CloudClient::IsOnline() {
  return WiFi.status() == WL_CONNECTED && mqtt_client_.connected();
}

bool CloudClient::PublishTelemetry(const SensorSnapshot& snapshot,
                                   const RuntimeStatus& status) {
  if (!EnsureMqttConnected()) {
    return false;
  }

  StaticJsonDocument<1536> doc;
  JsonArray services = doc.createNestedArray("services");
  JsonObject service = services.createNestedObject();
  service["service_id"] = "PetHome";
  const String telemetry_event_time = BuildEventTime();
  if (!telemetry_event_time.isEmpty()) {
    service["event_time"] = telemetry_event_time;
  }

  JsonObject props = service.createNestedObject("properties");
  props["ts"] = snapshot.ts;
  props["temp_c"] = snapshot.temp_c;
  props["hum_rh"] = snapshot.hum_rh;
  props["lux"] = snapshot.lux;
  props["food_g"] = snapshot.food_g;
  props["water_level_state"] = snapshot.water_low ? "LOW" : "OK";
  props["voc_index"] = snapshot.voc_index;
  props["tvoc_ppb"] = snapshot.tvoc_ppb;
  props["fan_on"] = status.fan_on;
  props["led_on"] = status.led_on;
  props["pump_on"] = status.pump_on;
  props["servo_active"] = status.servo_active;
  props["online"] = IsOnline();
  props["fw_version"] = "v1.0.0";
  props["last_error"] = status.last_error;

  String payload;
  serializeJson(doc, payload);
  return mqtt_client_.publish(TelemetryTopic().c_str(), payload.c_str());
}

bool CloudClient::PublishEvent(const String& event_type, const String& level,
                               const String& detail) {
  if (!EnsureMqttConnected()) {
    return false;
  }

  StaticJsonDocument<768> doc;
  JsonArray services = doc.createNestedArray("services");
  JsonObject service = services.createNestedObject();
  service["service_id"] = "PetHome";
  service["event_type"] = event_type;
  const String event_time = BuildEventTime();
  if (!event_time.isEmpty()) {
    service["event_time"] = event_time;
  }

  JsonObject paras = service.createNestedObject("paras");
  paras["level"] = level;
  paras["detail"] = detail;
  paras["ts"] = static_cast<uint64_t>(millis());

  String payload;
  serializeJson(doc, payload);
  return mqtt_client_.publish(EventTopic().c_str(), payload.c_str());
}

bool CloudClient::PublishAck(const CommandAck& ack) {
  if (!EnsureMqttConnected()) {
    return false;
  }

  if (ack.request_id.isEmpty()) {
    return false;
  }

  const String command_name =
      ack.command_name.isEmpty() ? String("command") : ack.command_name;

  StaticJsonDocument<512> doc;
  doc["result_code"] = AckResultCode(ack.result);
  doc["response_name"] = command_name + "_response";
  JsonObject paras = doc.createNestedObject("paras");
  paras["result"] = ack.result == AckResult::kOk ? "success" : "failed";
  paras["reason"] = ack.reason;
  paras["ts"] = ack.ts;

  String payload;
  serializeJson(doc, payload);
  return mqtt_client_.publish(CommandUpTopic(ack.request_id).c_str(),
                              payload.c_str());
}

void CloudClient::SetCommandHandler(CommandHandler handler) {
  command_handler_ = std::move(handler);
}

String CloudClient::TelemetryTopic() const {
  return "$oc/devices/" + device_id_ + "/sys/properties/report";
}

String CloudClient::EventTopic() const {
  return "$oc/devices/" + device_id_ + "/sys/events/up";
}

String CloudClient::CommandDownTopic() const {
  return "$oc/devices/" + device_id_ + "/sys/commands/#";
}

String CloudClient::CommandUpTopic(const String& request_id) const {
  return "$oc/devices/" + device_id_ +
         "/sys/commands/response/request_id=" + request_id;
}

bool CloudClient::EnsureMqttConnected() {
  if (mqtt_client_.connected()) {
    return true;
  }

  const uint32_t now_ms = millis();
  if ((now_ms - last_mqtt_retry_ms_) < 3000U) {
    return false;
  }
  last_mqtt_retry_ms_ = now_ms;

  const String client_id =
      "pet_home_" + device_id_ + "_" + String(random(100000, 999999));
  const bool ok = mqtt_client_.connect(client_id.c_str(), device_id_.c_str(),
                                       device_secret_.c_str());
  if (!ok) {
    return false;
  }

  mqtt_client_.subscribe(CommandDownTopic().c_str());
  return true;
}

void CloudClient::HandleMqttMessage(char* topic, uint8_t* payload,
                                    unsigned int length) {
  if (command_handler_ == nullptr) {
    return;
  }
  if (topic == nullptr || length == 0U) {
    return;
  }

  String request_id;
  if (!TryExtractRequestId(topic, request_id)) {
    return;
  }

  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, payload, length);
  if (err) {
    return;
  }

  const String cmd =
      doc["command_name"] | doc["cmd"] | doc["command"] | "";
  JsonVariantConst params =
      doc["paras"].isNull() ? doc["params"] : doc["paras"];
  command_handler_(cmd, request_id, params);
}

void CloudClient::MqttTrampoline(char* topic, uint8_t* payload,
                                 unsigned int length) {
  if (self_ == nullptr) {
    return;
  }
  self_->HandleMqttMessage(topic, payload, length);
}

bool CloudClient::TryExtractRequestId(const String& topic,
                                      String& request_id) const {
  const int marker_pos = topic.indexOf("request_id=");
  if (marker_pos < 0) {
    return false;
  }
  const int start = marker_pos + 11;
  int end = topic.indexOf('/', start);
  if (end < 0) {
    end = topic.length();
  }
  if (end <= start) {
    return false;
  }
  request_id = topic.substring(start, end);
  return request_id.length() > 0;
}

String CloudClient::BuildEventTime() const {
  time_t now = time(nullptr);
  if (now <= 0) {
    return "";
  }
  struct tm utc_tm;
  gmtime_r(&now, &utc_tm);
  char out[20] = {0};
  strftime(out, sizeof(out), "%Y%m%dT%H%M%SZ", &utc_tm);
  return String(out);
}

int CloudClient::AckResultCode(const AckResult result) const {
  switch (result) {
    case AckResult::kOk:
      return 0;
    case AckResult::kRejected:
      return 1;
    case AckResult::kFailed:
      return 2;
    default:
      return 2;
  }
}

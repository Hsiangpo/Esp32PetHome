#include "actuator_manager.h"

#include "board_pins.h"

void ActuatorManager::Begin() {
  pinMode(board_pins::kLedLight, OUTPUT);
  pinMode(board_pins::kFanRelay, OUTPUT);
  pinMode(board_pins::kPumpRelay, OUTPUT);
  pinMode(board_pins::kServoPwm, OUTPUT);

  status_ = RuntimeStatus();
  ApplyPinStates();
}

void ActuatorManager::Loop(const uint32_t now_ms) {
  if (!feed_started_) {
    return;
  }
  if (now_ms < feed_stop_at_ms_) {
    return;
  }
  
  status_.servo_active = false;
  feed_started_ = false;
  feed_stop_at_ms_ = 0;
  if (feed_will_timeout_) {
    feed_did_timeout_ = true;
  } else {
    feed_did_finish_ok_ = true;
  }
  ApplyPinStates();
}

bool ActuatorManager::SetLed(const bool on) {
  status_.led_on = on;
  ApplyPinStates();
  return true;
}

bool ActuatorManager::SetFan(const bool on) {
  status_.fan_on = on;
  ApplyPinStates();
  return true;
}

bool ActuatorManager::SetPump(const bool on) {
  if (on && status_.servo_active) {
    status_.last_error = "PUMP_SERVO_INTERLOCK";
    return false;
  }
  status_.pump_on = on;
  ApplyPinStates();
  return true;
}

bool ActuatorManager::StartFeed(const uint32_t now_ms, const uint32_t max_run_ms, const int target_g) {
  if (status_.pump_on || status_.servo_active) {
    status_.last_error = "FEED_INTERLOCK_REJECTED";
    return false;
  }

  uint32_t desired_ms = target_g * 50U; // 50g -> 2500ms
  if (desired_ms < 500U) desired_ms = 500U;
  
  status_.servo_active = true;
  feed_started_ = true;
  
  if (desired_ms > max_run_ms) {
    feed_stop_at_ms_ = now_ms + max_run_ms;
    feed_will_timeout_ = true;
  } else {
    feed_stop_at_ms_ = now_ms + desired_ms;
    feed_will_timeout_ = false;
  }
  feed_did_finish_ok_ = false;
  feed_did_timeout_ = false;

  ApplyPinStates();
  return true;
}

void ActuatorManager::StopFeed() {
  status_.servo_active = false;
  feed_started_ = false;
  feed_stop_at_ms_ = 0;
  feed_will_timeout_ = false;
  ApplyPinStates();
}

bool ActuatorManager::CheckAndClearFeedOk() {
  if (feed_did_finish_ok_) {
    feed_did_finish_ok_ = false;
    return true;
  }
  return false;
}

bool ActuatorManager::CheckAndClearFeedTimeout() {
  if (feed_did_timeout_) {
    feed_did_timeout_ = false;
    return true;
  }
  return false;
}

RuntimeStatus ActuatorManager::GetStatus() const { return status_; }

void ActuatorManager::ApplyPinStates() {
  digitalWrite(board_pins::kLedLight, status_.led_on ? HIGH : LOW);
  digitalWrite(board_pins::kFanRelay, status_.fan_on ? HIGH : LOW);
  digitalWrite(board_pins::kPumpRelay, status_.pump_on ? HIGH : LOW);
  digitalWrite(board_pins::kServoPwm, status_.servo_active ? HIGH : LOW);
}

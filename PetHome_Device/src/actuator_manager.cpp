#include "actuator_manager.h"

#include <stdint.h>

#include "board_pins.h"

#if !defined(PIO_UNIT_TESTING)
#include <esp32-hal-ledc.h>
#endif

namespace {
constexpr uint8_t kServoChannel = 0;
constexpr uint32_t kServoFrequencyHz = 50;
constexpr uint8_t kServoResolutionBits = 16;
constexpr uint32_t kServoPeriodUs = 20000;
constexpr uint32_t kServoMinPulseUs = 500;
constexpr uint32_t kServoMaxPulseUs = 2400;
constexpr int kServoClosedAngleDeg = 0;
constexpr int kServoOpenAngleDeg = 70;
constexpr uint32_t kServoMoveDurationMs = 200;
constexpr uint32_t kServoHoldMinMs = 300;
constexpr uint32_t kServoHoldMaxMs = 1500;
constexpr uint32_t kServoHoldPer10gMs = 120;

int ClampAngleDeg(const int angle_deg) {
  if (angle_deg < 0) {
    return 0;
  }
  if (angle_deg > 180) {
    return 180;
  }
  return angle_deg;
}

uint32_t ServoPulseUsForAngle(const int angle_deg) {
  const int clamped_angle = ClampAngleDeg(angle_deg);
  const uint32_t pulse_range_us = kServoMaxPulseUs - kServoMinPulseUs;
  return kServoMinPulseUs +
         static_cast<uint32_t>((static_cast<uint64_t>(pulse_range_us) *
                                static_cast<uint32_t>(clamped_angle)) /
                               180U);
}

uint32_t ServoDutyForAngle(const int angle_deg) {
  const uint32_t pulse_us = ServoPulseUsForAngle(angle_deg);
  const uint32_t duty_max = (1UL << kServoResolutionBits) - 1UL;
  return static_cast<uint32_t>((static_cast<uint64_t>(pulse_us) * duty_max) /
                               kServoPeriodUs);
}
}  // namespace

void ActuatorManager::Begin() {
  pinMode(board_pins::kLedLight, OUTPUT);
  pinMode(board_pins::kFanRelay, OUTPUT);
  pinMode(board_pins::kPumpRelay, OUTPUT);

  status_ = RuntimeStatus();
  feed_phase_ = FeedPhase::kIdle;
  phase_end_at_ms_ = 0;
  feed_timeout_at_ms_ = 0;
  feed_hold_ms_ = 0;
  feed_will_timeout_ = false;
  feed_did_finish_ok_ = false;
  feed_did_timeout_ = false;
#if !defined(PIO_UNIT_TESTING)
  ledcSetup(kServoChannel, kServoFrequencyHz, kServoResolutionBits);
  ledcAttachPin(board_pins::kServoPwm, kServoChannel);
#endif
  WriteServoAngle(kServoClosedAngleDeg);
  ApplyPinStates();
}

void ActuatorManager::Loop(const uint32_t now_ms) {
  if (feed_phase_ == FeedPhase::kIdle) {
    return;
  }

  while (feed_phase_ != FeedPhase::kIdle) {
    if (feed_will_timeout_ && now_ms >= feed_timeout_at_ms_) {
      FinishFeed(true);
      return;
    }
    if (now_ms < phase_end_at_ms_) {
      return;
    }

    switch (feed_phase_) {
      case FeedPhase::kOpening:
        feed_phase_ = FeedPhase::kHolding;
        phase_end_at_ms_ = now_ms + feed_hold_ms_;
        break;
      case FeedPhase::kHolding:
        feed_phase_ = FeedPhase::kClosing;
        WriteServoAngle(kServoClosedAngleDeg);
        phase_end_at_ms_ = now_ms + kServoMoveDurationMs;
        break;
      case FeedPhase::kClosing:
        FinishFeed(false);
        return;
      case FeedPhase::kIdle:
      default:
        return;
    }
  }
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

  const uint32_t desired_hold_ms = HoldMsForTarget(target_g);
  const uint32_t planned_total_ms =
      kServoMoveDurationMs + desired_hold_ms + kServoMoveDurationMs;

  status_.servo_active = true;
  status_.last_error = "";
  feed_phase_ = FeedPhase::kOpening;
  feed_hold_ms_ = desired_hold_ms;
  phase_end_at_ms_ = now_ms + kServoMoveDurationMs;
  feed_will_timeout_ = planned_total_ms > max_run_ms;
  feed_timeout_at_ms_ = feed_will_timeout_ ? (now_ms + max_run_ms) : 0;
  feed_did_finish_ok_ = false;
  feed_did_timeout_ = false;
  WriteServoAngle(kServoOpenAngleDeg);

  return true;
}

void ActuatorManager::StopFeed() {
  ResetFeedState();
  WriteServoAngle(kServoClosedAngleDeg);
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
}

void ActuatorManager::WriteServoAngle(const int angle_deg) {
  servo_angle_deg_ = ClampAngleDeg(angle_deg);
#if !defined(PIO_UNIT_TESTING)
  ledcWrite(kServoChannel, ServoDutyForAngle(servo_angle_deg_));
#endif
}

void ActuatorManager::ResetFeedState() {
  status_.servo_active = false;
  feed_phase_ = FeedPhase::kIdle;
  phase_end_at_ms_ = 0;
  feed_timeout_at_ms_ = 0;
  feed_hold_ms_ = 0;
  feed_will_timeout_ = false;
}

void ActuatorManager::FinishFeed(const bool timed_out) {
  ResetFeedState();
  WriteServoAngle(kServoClosedAngleDeg);
  if (timed_out) {
    status_.last_error = "FEED_TIMEOUT";
    feed_did_timeout_ = true;
    feed_did_finish_ok_ = false;
  } else {
    feed_did_timeout_ = false;
    feed_did_finish_ok_ = true;
  }
  ApplyPinStates();
}

uint32_t ActuatorManager::HoldMsForTarget(const int target_g) {
  uint32_t hold_ms = kServoHoldMinMs;
  if (target_g > 0) {
    hold_ms += static_cast<uint32_t>(target_g / 10) * kServoHoldPer10gMs;
  }
  if (hold_ms > kServoHoldMaxMs) {
    hold_ms = kServoHoldMaxMs;
  }
  return hold_ms;
}

#if defined(PIO_UNIT_TESTING)
uint32_t ActuatorManager::DebugHoldMsForTarget(const int target_g) {
  return HoldMsForTarget(target_g);
}
#endif

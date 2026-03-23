#ifndef PET_HOME_ACTUATOR_MANAGER_H_
#define PET_HOME_ACTUATOR_MANAGER_H_

#include <Arduino.h>

#include "pet_home_types.h"

class ActuatorManager {
 public:
  struct FeedPhase {
    enum Value {
      kIdle = 0,
      kOpening,
      kHolding,
      kClosing,
    };
  };

  void Begin();
  void Loop(uint32_t now_ms);

  bool SetLed(bool on);
  bool SetFan(bool on);
  bool SetPump(bool on);

  bool StartFeed(uint32_t now_ms, uint32_t max_run_ms, int target_g);
  void StopFeed();
  void SetOnlineStatus(bool online) { status_.online = online; }

  bool CheckAndClearFeedOk();
  bool CheckAndClearFeedTimeout();

  RuntimeStatus GetStatus() const;

#if defined(PIO_UNIT_TESTING)
  static uint32_t DebugHoldMsForTarget(int target_g);
  FeedPhase::Value DebugGetFeedPhase() const { return feed_phase_; }
  int DebugGetServoAngleDeg() const { return servo_angle_deg_; }
#endif

 private:
  void ApplyPinStates();
  void WriteServoAngle(int angle_deg);
  void ResetFeedState();
  void FinishFeed(bool timed_out);
  static uint32_t HoldMsForTarget(int target_g);

  RuntimeStatus status_;
  FeedPhase::Value feed_phase_ = FeedPhase::kIdle;
  uint32_t phase_end_at_ms_ = 0;
  uint32_t feed_timeout_at_ms_ = 0;
  uint32_t feed_hold_ms_ = 0;
  bool feed_will_timeout_ = false;
  bool feed_did_finish_ok_ = false;
  bool feed_did_timeout_ = false;
  int servo_angle_deg_ = 0;
};

#endif  // PET_HOME_ACTUATOR_MANAGER_H_

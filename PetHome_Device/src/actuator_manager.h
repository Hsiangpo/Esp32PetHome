#ifndef PET_HOME_ACTUATOR_MANAGER_H_
#define PET_HOME_ACTUATOR_MANAGER_H_

#include <Arduino.h>

#include "pet_home_types.h"

class ActuatorManager {
 public:
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

 private:
  void ApplyPinStates();

  RuntimeStatus status_;
  bool feed_started_ = false;
  uint32_t feed_stop_at_ms_ = 0;
  bool feed_will_timeout_ = false;
  bool feed_did_finish_ok_ = false;
  bool feed_did_timeout_ = false;
};

#endif  // PET_HOME_ACTUATOR_MANAGER_H_

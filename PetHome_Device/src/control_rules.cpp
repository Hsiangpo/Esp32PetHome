#include "control_rules.h"

namespace control_rules {

namespace {
bool HoldSatisfied(const uint32_t since_ms, const uint32_t now_ms,
                   const uint32_t hold_ms) {
  if (now_ms < since_ms) {
    return false;
  }
  return (now_ms - since_ms) >= hold_ms;
}
}  // namespace

bool ShouldTriggerFeed(const float food_g, const float threshold_g,
                       const uint32_t low_hold_ms,
                       const bool currently_feeding,
                       const bool waiting_food_recover,
                       const uint32_t low_since_ms, const uint32_t now_ms) {
  if (currently_feeding) {
    return false;
  }
  if (waiting_food_recover) {
    return false;
  }
  if (food_g >= threshold_g) {
    return false;
  }
  return HoldSatisfied(low_since_ms, now_ms, low_hold_ms);
}

bool ShouldTriggerLowFoodEvent(const float food_g, const float threshold_g,
                               const uint32_t low_hold_ms,
                               const uint32_t low_since_ms,
                               const uint32_t now_ms,
                               const bool event_active) {
  if (event_active) {
    return false;
  }
  if (food_g >= threshold_g) {
    return false;
  }
  return HoldSatisfied(low_since_ms, now_ms, low_hold_ms);
}

bool ShouldTriggerLowWaterEvent(const bool water_low, const uint32_t low_hold_ms,
                                const uint32_t low_since_ms,
                                const uint32_t now_ms,
                                const bool event_active) {
  if (event_active || !water_low) {
    return false;
  }
  return HoldSatisfied(low_since_ms, now_ms, low_hold_ms);
}

bool ShouldStartRefill(const bool water_low, const uint32_t debounce_ms,
                       const bool pump_on, const uint32_t low_since_ms,
                       const uint32_t now_ms) {
  if (!water_low || pump_on) {
    return false;
  }
  return HoldSatisfied(low_since_ms, now_ms, debounce_ms);
}

bool ShouldStopRefill(const bool water_ok, const uint32_t ok_hold_ms,
                      const uint32_t ok_since_ms, const uint32_t now_ms) {
  if (!water_ok) {
    return false;
  }
  return HoldSatisfied(ok_since_ms, now_ms, ok_hold_ms);
}

bool ShouldStartFan(const int voc_index, const int on_threshold,
                    const uint32_t debounce_ms, const uint32_t high_since_ms,
                    const bool fan_on, const uint32_t now_ms) {
  if (fan_on || voc_index < on_threshold) {
    return false;
  }
  return HoldSatisfied(high_since_ms, now_ms, debounce_ms);
}

bool ShouldStopFan(const int voc_index, const int off_threshold,
                   const uint32_t hold_ms, const uint32_t low_since_ms,
                   const bool fan_on, const uint32_t now_ms) {
  if (!fan_on || voc_index > off_threshold) {
    return false;
  }
  return HoldSatisfied(low_since_ms, now_ms, hold_ms);
}

bool ShouldTurnOnLight(const float lux, const float on_threshold,
                       const bool night_mode_enabled,
                       const bool in_night_window) {
  if (night_mode_enabled && !in_night_window) {
    return false;
  }
  return lux <= on_threshold;
}

bool ShouldTurnOffLight(const float lux, const float off_threshold,
                        const bool night_mode_enabled,
                        const bool in_night_window) {
  if (night_mode_enabled && !in_night_window) {
    return true;
  }
  return lux >= off_threshold;
}

}  // namespace control_rules

#ifndef PET_HOME_CONTROL_RULES_H_
#define PET_HOME_CONTROL_RULES_H_

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <cstdint>
#endif

namespace control_rules {

bool ShouldTriggerFeed(float food_g, float threshold_g, uint32_t low_hold_ms,
                       bool currently_feeding, bool waiting_food_recover,
                       uint32_t low_since_ms, uint32_t now_ms);

bool ShouldTriggerLowFoodEvent(float food_g, float threshold_g,
                               uint32_t low_hold_ms, uint32_t low_since_ms,
                               uint32_t now_ms, bool event_active);

bool ShouldTriggerLowWaterEvent(bool water_low, uint32_t low_hold_ms,
                                uint32_t low_since_ms, uint32_t now_ms,
                                bool event_active);

bool ShouldStartRefill(bool water_low, uint32_t debounce_ms, bool pump_on,
                       uint32_t low_since_ms, uint32_t now_ms);

bool ShouldStopRefill(bool water_ok, uint32_t ok_hold_ms, uint32_t ok_since_ms,
                      uint32_t now_ms);

bool ShouldStartFan(int voc_index, int on_threshold, uint32_t debounce_ms,
                    uint32_t high_since_ms, bool fan_on, uint32_t now_ms);

bool ShouldStopFan(int voc_index, int off_threshold, uint32_t hold_ms,
                   uint32_t low_since_ms, bool fan_on, uint32_t now_ms);

bool ShouldTurnOnLight(float lux, float on_threshold, bool night_mode_enabled,
                       bool in_night_window);

bool ShouldTurnOffLight(float lux, float off_threshold, bool night_mode_enabled,
                        bool in_night_window);

}  // namespace control_rules

#endif  // PET_HOME_CONTROL_RULES_H_

#include <unity.h>

#include "control_rules.h"

using control_rules::ShouldStartFan;
using control_rules::ShouldStartRefill;
using control_rules::ShouldStopFan;
using control_rules::ShouldStopRefill;
using control_rules::ShouldTriggerLowFoodEvent;
using control_rules::ShouldTriggerLowWaterEvent;
using control_rules::ShouldTriggerFeed;
using control_rules::ShouldTurnOffLight;
using control_rules::ShouldTurnOnLight;

void test_feed_should_trigger_after_hold_ms(void) {
  const bool triggered =
      ShouldTriggerFeed(20.0F, 50.0F, 3000U, false, false, 1000U, 4500U);
  TEST_ASSERT_TRUE(triggered);
}

void test_feed_should_not_trigger_when_already_feeding(void) {
  const bool triggered =
      ShouldTriggerFeed(20.0F, 50.0F, 3000U, true, false, 1000U, 5000U);
  TEST_ASSERT_FALSE(triggered);
}

void test_feed_should_not_trigger_before_hold_ms(void) {
  const bool triggered =
      ShouldTriggerFeed(20.0F, 50.0F, 3000U, false, false, 1000U, 3500U);
  TEST_ASSERT_FALSE(triggered);
}

void test_feed_should_wait_until_low_food_state_recovered(void) {
  const bool blocked =
      ShouldTriggerFeed(20.0F, 50.0F, 3000U, false, true, 1000U, 5000U);
  TEST_ASSERT_FALSE(blocked);
}

void test_refill_start_and_stop_rules(void) {
  const bool start_ok = ShouldStartRefill(true, 2000U, false, 1000U, 3500U);
  TEST_ASSERT_TRUE(start_ok);

  const bool stop_ok = ShouldStopRefill(true, 2000U, 5000U, 7500U);
  TEST_ASSERT_TRUE(stop_ok);
}

void test_refill_should_not_start_when_pump_is_on(void) {
  const bool start_ok = ShouldStartRefill(true, 2000U, true, 1000U, 5000U);
  TEST_ASSERT_FALSE(start_ok);
}

void test_refill_should_not_start_before_debounce(void) {
  const bool start_ok = ShouldStartRefill(true, 2000U, false, 3000U, 4500U);
  TEST_ASSERT_FALSE(start_ok);
}

void test_fan_hysteresis_rules(void) {
  const bool start_ok = ShouldStartFan(260, 250, 3000U, 1000U, false, 4500U);
  TEST_ASSERT_TRUE(start_ok);

  const bool stop_ok = ShouldStopFan(180, 200, 5000U, 1000U, true, 7000U);
  TEST_ASSERT_TRUE(stop_ok);
}

void test_fan_should_not_stop_before_hold_ms(void) {
  const bool stop_ok = ShouldStopFan(180, 200, 5000U, 1000U, true, 5500U);
  TEST_ASSERT_FALSE(stop_ok);
}

void test_light_rules_with_night_mode(void) {
  const bool on_ok = ShouldTurnOnLight(80.0F, 100.0F, true, true);
  TEST_ASSERT_TRUE(on_ok);

  const bool off_ok = ShouldTurnOffLight(220.0F, 150.0F, true, true);
  TEST_ASSERT_TRUE(off_ok);
}

void test_light_should_force_off_when_outside_night_window(void) {
  const bool off_ok = ShouldTurnOffLight(10.0F, 150.0F, true, false);
  TEST_ASSERT_TRUE(off_ok);
}

void test_low_food_event_should_only_trigger_once_while_active(void) {
  const bool first =
      ShouldTriggerLowFoodEvent(25.0F, 50.0F, 3000U, 1000U, 4500U, false);
  TEST_ASSERT_TRUE(first);

  const bool second =
      ShouldTriggerLowFoodEvent(25.0F, 50.0F, 3000U, 1000U, 5000U, true);
  TEST_ASSERT_FALSE(second);
}

void test_low_water_event_should_wait_for_hold_duration(void) {
  const bool early =
      ShouldTriggerLowWaterEvent(true, 3000U, 2000U, 4500U, false);
  TEST_ASSERT_FALSE(early);

  const bool due =
      ShouldTriggerLowWaterEvent(true, 3000U, 2000U, 5500U, false);
  TEST_ASSERT_TRUE(due);
}

void setup(void) {
  UNITY_BEGIN();
  RUN_TEST(test_feed_should_trigger_after_hold_ms);
  RUN_TEST(test_feed_should_not_trigger_when_already_feeding);
  RUN_TEST(test_feed_should_not_trigger_before_hold_ms);
  RUN_TEST(test_feed_should_wait_until_low_food_state_recovered);
  RUN_TEST(test_refill_start_and_stop_rules);
  RUN_TEST(test_refill_should_not_start_when_pump_is_on);
  RUN_TEST(test_refill_should_not_start_before_debounce);
  RUN_TEST(test_fan_hysteresis_rules);
  RUN_TEST(test_fan_should_not_stop_before_hold_ms);
  RUN_TEST(test_light_rules_with_night_mode);
  RUN_TEST(test_light_should_force_off_when_outside_night_window);
  RUN_TEST(test_low_food_event_should_only_trigger_once_while_active);
  RUN_TEST(test_low_water_event_should_wait_for_hold_duration);
  UNITY_END();
}

void loop(void) {}

#include <unity.h>

#include "actuator_manager.h"

void test_feed_hold_ms_should_clamp_to_bounds(void) {
  TEST_ASSERT_EQUAL_UINT32(300U, ActuatorManager::DebugHoldMsForTarget(1));
  TEST_ASSERT_EQUAL_UINT32(900U, ActuatorManager::DebugHoldMsForTarget(50));
  TEST_ASSERT_EQUAL_UINT32(1500U, ActuatorManager::DebugHoldMsForTarget(200));
}

void test_feed_should_progress_open_hold_close_then_finish(void) {
  ActuatorManager manager;
  manager.Begin();

  const bool started = manager.StartFeed(1000U, 5000U, 50);
  TEST_ASSERT_TRUE(started);
  TEST_ASSERT_TRUE(manager.GetStatus().servo_active);
  TEST_ASSERT_EQUAL_INT(ActuatorManager::FeedPhase::kOpening,
                        manager.DebugGetFeedPhase());
  TEST_ASSERT_EQUAL_INT(70, manager.DebugGetServoAngleDeg());

  manager.Loop(1300U);
  TEST_ASSERT_EQUAL_INT(ActuatorManager::FeedPhase::kHolding,
                        manager.DebugGetFeedPhase());
  TEST_ASSERT_EQUAL_INT(70, manager.DebugGetServoAngleDeg());

  manager.Loop(2200U);
  TEST_ASSERT_EQUAL_INT(ActuatorManager::FeedPhase::kClosing,
                        manager.DebugGetFeedPhase());
  TEST_ASSERT_EQUAL_INT(0, manager.DebugGetServoAngleDeg());

  manager.Loop(2300U);
  TEST_ASSERT_FALSE(manager.GetStatus().servo_active);
  TEST_ASSERT_EQUAL_INT(ActuatorManager::FeedPhase::kIdle,
                        manager.DebugGetFeedPhase());
  TEST_ASSERT_TRUE(manager.CheckAndClearFeedOk());
}

void test_feed_should_timeout_when_budget_is_too_small(void) {
  ActuatorManager manager;
  manager.Begin();

  const bool started = manager.StartFeed(500U, 400U, 100);
  TEST_ASSERT_TRUE(started);

  manager.Loop(901U);
  TEST_ASSERT_FALSE(manager.GetStatus().servo_active);
  TEST_ASSERT_TRUE(manager.CheckAndClearFeedTimeout());
}

void setup(void) {
  UNITY_BEGIN();
  RUN_TEST(test_feed_hold_ms_should_clamp_to_bounds);
  RUN_TEST(test_feed_should_progress_open_hold_close_then_finish);
  RUN_TEST(test_feed_should_timeout_when_budget_is_too_small);
  UNITY_END();
}

void loop(void) {}

#include <Arduino.h>
#include <unity.h>

void test_always_passes() {
  TEST_ASSERT_TRUE(true);
}

void setup() {
  delay(2000);
  UNITY_BEGIN();
  RUN_TEST(test_always_passes);
  UNITY_END();
}

void loop() {}

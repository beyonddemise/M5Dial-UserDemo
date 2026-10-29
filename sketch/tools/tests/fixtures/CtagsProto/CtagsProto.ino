// Fixture for test_ctags_compat.py: every helper is used before it is defined, so the
// sketch only compiles if arduino-cli's ctags step generates correct prototypes.
#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  Serial.println(label());
}

void loop() {
  delay(helper(1));
}

int helper(int x) {
  return x * 2;
}

const char* label() {
  return "ok";
}

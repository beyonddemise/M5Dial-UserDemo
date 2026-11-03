#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  helper(3);
}

void loop() {
  delay(helper(1));
}

int helper(int x) {
  return x * 2;
}

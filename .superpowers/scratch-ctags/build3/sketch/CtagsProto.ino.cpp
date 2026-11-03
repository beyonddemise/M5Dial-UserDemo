#line 1 "/Users/stw/Code/M5Dial-UserDemo/.superpowers/wt-pr24/sketch/tools/tests/fixtures/CtagsProto/CtagsProto.ino"
// Fixture for test_ctags_compat.py: every helper is used before it is defined, so the
// sketch only compiles if arduino-cli's ctags step generates correct prototypes.
#include <Arduino.h>

#line 5 "/Users/stw/Code/M5Dial-UserDemo/.superpowers/wt-pr24/sketch/tools/tests/fixtures/CtagsProto/CtagsProto.ino"
void setup();
#line 10 "/Users/stw/Code/M5Dial-UserDemo/.superpowers/wt-pr24/sketch/tools/tests/fixtures/CtagsProto/CtagsProto.ino"
void loop();
#line 14 "/Users/stw/Code/M5Dial-UserDemo/.superpowers/wt-pr24/sketch/tools/tests/fixtures/CtagsProto/CtagsProto.ino"
int helper(int x);
#line 18 "/Users/stw/Code/M5Dial-UserDemo/.superpowers/wt-pr24/sketch/tools/tests/fixtures/CtagsProto/CtagsProto.ino"
const char * label();
#line 5 "/Users/stw/Code/M5Dial-UserDemo/.superpowers/wt-pr24/sketch/tools/tests/fixtures/CtagsProto/CtagsProto.ino"
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


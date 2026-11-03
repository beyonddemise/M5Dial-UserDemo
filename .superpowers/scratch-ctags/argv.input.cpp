# 1 "/Users/stw/Code/M5Dial-UserDemo/.superpowers/wt-pr24/sketch/tools/tests/fixtures/CtagsProto/CtagsProto.ino"
// Fixture for test_ctags_compat.py: every helper is used before it is defined, so the
// sketch only compiles if arduino-cli's ctags step generates correct prototypes.
# 4 "/Users/stw/Code/M5Dial-UserDemo/.superpowers/wt-pr24/sketch/tools/tests/fixtures/CtagsProto/CtagsProto.ino" 2

void setup() {
  HWCDCSerial.begin(115200);
  HWCDCSerial.println(label());
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

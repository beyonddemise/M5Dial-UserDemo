#line 1 "/Users/stw/Code/M5Dial-UserDemo/.superpowers/scratch-ctags/Repro/Repro.ino"
#include <Arduino.h>

#line 3 "/Users/stw/Code/M5Dial-UserDemo/.superpowers/scratch-ctags/Repro/Repro.ino"
void setup();
#line 8 "/Users/stw/Code/M5Dial-UserDemo/.superpowers/scratch-ctags/Repro/Repro.ino"
void loop();
#line 12 "/Users/stw/Code/M5Dial-UserDemo/.superpowers/scratch-ctags/Repro/Repro.ino"
int helper(int x);
#line 3 "/Users/stw/Code/M5Dial-UserDemo/.superpowers/scratch-ctags/Repro/Repro.ino"
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


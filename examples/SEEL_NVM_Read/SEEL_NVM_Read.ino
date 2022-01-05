#include <SEEL_Assert.h>

void setup() {
  Serial.begin(9600);
  SEEL_Print::init(&Serial);

  SEEL_Assert::init_nvm();
  SEEL_Assert::print_nvm_block();
  SEEL_Assert::print_nvm_fails();
}

void loop() {
}

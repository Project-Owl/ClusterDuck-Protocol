#include "include/DuckLed.h"

DuckLed* DuckLed::instance = NULL;

DuckLed::DuckLed() {}

DuckLed* DuckLed::getInstance() {
  return (instance == NULL) ? new DuckLed : instance;
}

void DuckLed::setupLED(int redPin, int greenPin, int bluePin) {
#ifdef ESP32
  ledcAttach(redPin, 12000, 8); // assign RGB led pins to channels
  ledcAttach(greenPin, 12000, 8);
  ledcAttach(bluePin, 12000, 8);

  // Initialize channels
  // channels 0-15, resolution 1-16 bits, freq limits depend on resolution
  // ledcSetup(uint8_t channel, uint32_t freq, uint8_t resolution_bits);
  // ledcSetup(1, 12000, 8); // 12 kHz PWM, 8-bit resolution
  // ledcSetup(2, 12000, 8);
  // ledcSetup(3, 12000, 8);

  // ledcAttachChannel(redPin, 12000, 8, 1);
  // ledcAttachChannel(greenPin, 12000, 8, 2);
  // ledcAttachChannel(bluePin, 12000, 8, 3);
#endif
}

void DuckLed::setColor(int ledR, int ledG, int ledB) {
#ifdef ESP32
  ledcWrite(1, ledR);
  ledcWrite(2, ledG);
  ledcWrite(3, ledB);
#endif
}

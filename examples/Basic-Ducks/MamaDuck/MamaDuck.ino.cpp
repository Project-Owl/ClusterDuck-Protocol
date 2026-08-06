# 1 "/var/folders/3h/n7g9d8x521gg4nfvf31y3_840000gn/T/tmpwddleu8k"
#include <Arduino.h>
# 1 "/Users/tqrahman/Documents/Arduino/libraries/ClusterDuck-Protocol/examples/Basic-Ducks/MamaDuck/MamaDuck.ino"
# 12 "/Users/tqrahman/Documents/Arduino/libraries/ClusterDuck-Protocol/examples/Basic-Ducks/MamaDuck/MamaDuck.ino"
 #include <string>
 #include <CDP.h>

 #ifdef SERIAL_PORT_USBVIRTUAL
 #define Serial SERIAL_PORT_USBVIRTUAL
 #endif



#include <FastLED.h>
#include <pixeltypes.h>
#define LED_TYPE WS2812
#define NUM_LEDS 1
#define COLOR_ORDER GRB
#define BRIGHTNESS 128
CRGB leds[NUM_LEDS];


 bool runSensor(void *);


 MamaDuck duck("MAMADUCK");
 auto timer = timer_create_default();
 const int INTERVAL_MS = 10000;
 int counter = 1;
 bool setupOK = false;
# 46 "/Users/tqrahman/Documents/Arduino/libraries/ClusterDuck-Protocol/examples/Basic-Ducks/MamaDuck/MamaDuck.ino"
 void setup() {

  FastLED.addLeds<LED_TYPE, CDPCFG_PIN_LED1, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalSMD5050 );
  FastLED.setBrightness(BRIGHTNESS);
  leds[0] = CRGB::Cyan;
  FastLED.show();

   if (duck.setupWithDefaults() != DUCK_ERR_NONE) {
      loginfo_ln("[MAMA] Failed to setup MamaDuck");
      leds[0] = CRGB::Red;
      FastLED.show();
      return;
    } else {
      leds[0] = CRGB::Gold;
      FastLED.show();
    }

   timer.every(INTERVAL_MS, runSensor);

   setupOK = true;
   loginfo_ln("[MAMA] Setup OK!");
 }






 void loop() {
   if (!setupOK) {
     return;
   }
   timer.tick();

   duck.run();
 }
# 92 "/Users/tqrahman/Documents/Arduino/libraries/ClusterDuck-Protocol/examples/Basic-Ducks/MamaDuck/MamaDuck.ino"
 bool runSensor(void *) {
    bool failure;

    std::string message = "Placeholder Sensor Data. C: " + std::to_string(counter);

    failure = duck.sendData(topics::sensor, message);
    if (!failure) {
    Serial.println("[MAMA] runSensor ok.");
    } else {
    Serial.println("[MAMA] runSensor failed.");
    }
    return true;
 }

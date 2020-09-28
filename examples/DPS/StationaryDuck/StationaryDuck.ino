#include "timer.h"
#include <ClusterDuck.h>

auto timer = timer_create_default(); // create a timer with default settings
ClusterDuck duck;

void setup() {
  duck.begin();
  duck.setDeviceId("S01");
  duck.setupMamaDuck();

  timer.every(5000, sendPing);
}

void loop() {
  timer.tick();
  duck.runMamaDuck();
}

bool sendPing(void *) {
  duck.sendPayloadStandard("Here", "rssi");
  return true;
}

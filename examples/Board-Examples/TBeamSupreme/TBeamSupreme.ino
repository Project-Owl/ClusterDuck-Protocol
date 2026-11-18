// Minimal Supreme radio/PMU bring-up, not the tracker application.
// Allocate a unique eight-byte ID before running more than one copy.
#include <CDP.h>
#include <Wire.h>
#include <SPI.h>
#include <XPowersLib.h>

#ifndef CDPCFG_PMU_AXP2101
#error "Build this example with the T-Beam Supreme board profile"
#endif

XPowersAXP2101 pmu;
MamaDuck duck("SUPTEST1");
bool ready = false;
uint32_t lastReportMs = 0;

void setup() {
  Serial.begin(115200); // USB CDC; no wait for a serial monitor.
  Wire1.begin(CDPCFG_PIN_PMU_SDA, CDPCFG_PIN_PMU_SCL);
  Wire1.setTimeOut(20);
  if (!pmu.begin(Wire1, AXP2101_SLAVE_ADDRESS,
                 CDPCFG_PIN_PMU_SDA, CDPCFG_PIN_PMU_SCL)) {
    Serial.println("[SUPREME] AXP2101 not found; radio setup skipped");
    return;
  }
  if (!pmu.setALDO3Voltage(3300) || !pmu.enableALDO3() ||
      !pmu.isEnableALDO3() || pmu.getALDO3Voltage() != 3300) {
    Serial.println("[SUPREME] Radio rail configuration failed; setup skipped");
    return;
  }

  // Leave SD/IMU pins untouched until the tracker has enabled their rails.

  const int rc = duck.setupWithDefaults();
  ready = rc == DUCK_ERR_NONE;
  Serial.printf("[SUPREME] radio rc=%d; ring DIN GPIO%d reserved, ring not started\n",
                rc, CDPCFG_PIN_LED_RING);
}

void loop() {
  if (!ready) {
    delay(1);
    return;
  }
  duck.run();
  const uint32_t now = millis();
  if (uint32_t(now - lastReportMs) >= 60000) {
    lastReportMs = now;
    const int rc = duck.sendData(topics::sensor, "Supreme radio bring-up");
    Serial.printf("[SUPREME] queue rc=%d\n", rc);
  }
}

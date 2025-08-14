/**
 * @file MamaDuck.ino
 * @brief Uses the built in Mama Duck.
 */

#include <string>
#include <vector>
#include <arduino-timer.h>
#include <CDP.h>

#ifdef SERIAL_PORT_USBVIRTUAL
#define Serial SERIAL_PORT_USBVIRTUAL
#endif

bool sendData(std::vector<byte> message);
bool runSensor(void *);

// create a built-in mama duck
MamaDuck duck;

// create a timer with default settings
auto timer = timer_create_default();

// for sending the counter message
const int INTERVAL_MS = 150000;
int counter = 1;
bool setupOK = false;

std::string arduinoStringFromHex(byte* data, int size) 
{
  std::string buf = "";
  buf.reserve(size * 2); // 2 digit hex
  const char* cs = "0123456789ABCDEF";
  for (int i = 0; i < size; i++) {
    byte val = data[i];
    buf += cs[(val >> 4) & 0x0F];
    buf += cs[val & 0x0F];
  }
  return buf;
}

void setup() {
  // We are using a hardcoded device id here, but it should be retrieved or
  // given during the device provisioning then converted to a byte vector to
  // setup the duck NOTE: The Device ID must be exactly 8 bytes otherwise it
  // will get rejected
  std::string deviceId("MAMAATAK");
  std::array<byte,8> devId;
  std::copy(deviceId.begin(), deviceId.end(), devId.begin());
  if (duck.setupWithDefaults(devId) != DUCK_ERR_NONE) {
    Serial.println("[MAMA] Failed to setup MamaDuck");
    return;
  }

  Serial1.begin(9600, SERIAL_8N1, 4, 0);
  setupOK = true;
  Serial.println("[MAMA] Setup OK!");

}

std::vector<byte> stringToByteVector(const std::string& str) {
    std::vector<byte> byteVec;
    byteVec.reserve(str.length());

    for (unsigned int i = 0; i < str.length(); ++i) {
        byteVec.push_back(static_cast<byte>(str[i]));
    }

    return byteVec;
}

void loop() {
  if (!setupOK) {
    return; 
  }
  timer.tick();
  // Use the default run(). The Mama duck is designed to also forward data it receives
  // from other ducks, across the network. It has a basic routing mechanism built-in
  // to prevent messages from hoping endlessly.
  if (Serial1.available() > 0) {
    String data = Serial1.readStringUntil('\n'); 
    if ((data.indexOf("DuckGPS:") >= 0) || (data.indexOf("DuckBMP:") >= 0)) {
      Serial.println("[MAMA]: DETECTION -> " + data);
      duck.storeSensorData(stringToByteVector(data.c_str()));
    }

  }
  duck.run();
}


bool sendData(std::vector<byte> message) {
  bool sentOk = false;
  
  int err = duck.sendData(topics::status, message);
  if (err == DUCK_ERR_NONE) {
     counter++;
     sentOk = true;
  }
  if (!sentOk) {
    std::string errMessage = "[MAMA] Failed to send data. error = " + std::to_string(err);
    Serial.println(errMessage.c_str());
  }
  return sentOk;
}

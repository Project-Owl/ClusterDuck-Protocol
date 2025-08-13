/**
 * @file MamaDuck.ino
 * @brief Uses the built in Mama Duck.
 */

#include <string>
#include <vector>
#include <arduino-timer.h>
#include <CDP.h>

// GPS Setup
#include <TinyGPS++.h>
TinyGPSPlus tgps;
HardwareSerial GPS(1);

// // Setup BMP180
// #include <Adafruit_BMP085_U.h>
// Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

#ifdef SERIAL_PORT_USBVIRTUAL
#define Serial SERIAL_PORT_USBVIRTUAL
#endif

bool sendData(std::vector<byte> message);
bool runSensor(void *);

static void smartDelay(unsigned long ms);
String getGPSData();
// String getBMPData();

// create a built-in mama duck
MamaDuck duck;

// create a timer with default settings
auto timer = timer_create_default();

// for sending the counter message
const int INTERVAL_MS = 60000;
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
  std::string deviceId("MAMA0001");
  std::array<byte,8> devId;
  std::copy(deviceId.begin(), deviceId.end(), devId.begin());
  if (duck.setupWithDefaults(devId) != DUCK_ERR_NONE) {
    Serial.println("[MAMA] Failed to setup MamaDuck");
    return;
  }
  setupOK = true;

  GPS.begin(9600, SERIAL_8N1, 34, 12);  

  // // BMP setup
  // if (!bmp.begin()) {
  //   /* There was a problem detecting the BMP085 ... check your connections */
  //   Serial.print(
  //       "Ooops, no BMP085 detected ... Check your wiring or I2C ADDR!");
  //   while (1)
  //     ;
  // } else {
  //   Serial.println("BMP on");
  // }

  // Initialize the timer. The timer thread runs separately from the main loop
  // and will trigger sending a counter message.
  timer.every(INTERVAL_MS, runSensor);
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
  duck.run();
}

bool runSensor(void *) {
  bool result;

  // String bmpData = getBMPData();
  String gpsData = getGPSData();

  String message = "\"" + String("GPS: ") + gpsData + "\"";
  // String message = "\"" + String("BMP: ") + bmpData + "\"";
  Serial.print("[MAMA] sensor data: ");
  Serial.println(message.c_str());

  duck.storeSensorData(stringToByteVector(message.c_str()));
  return true;
}

static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do
  {
    while (GPS.available())
      tgps.encode(GPS.read());
  } while (millis() - start < ms);
}

// Getting GPS data
String getGPSData() {

  // Encoding the GPS
  smartDelay(5000);
  
  // Printing the GPS data
  Serial.println("--- GPS ---");
  Serial.print("Latitude  : ");
  Serial.println(tgps.location.lat(), 5);  
  Serial.print("Longitude : ");
  Serial.println(tgps.location.lng(), 4);
  Serial.print("Altitude  : ");
  Serial.print(tgps.altitude.feet() / 3.2808);
  Serial.println("M");
  Serial.print("Satellites: ");
  Serial.println(tgps.satellites.value());
  Serial.println("**********************");

  // Creating a message of the Latitude and Longitude
  String sensorVal = "Lat:" + String(tgps.location.lat(), 5) + " Lng:" + String(tgps.location.lng(), 4);

  // Check to see if GPS data is being received
  if (millis() > 5000 && tgps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS data received: check wiring"));
  }

  return sensorVal;
}

// String getBMPData() {
  
//   float T, P;

//   bmp.getTemperature(&T);
//   Serial.println(T);
//   bmp.getPressure(&P);
//   Serial.println(P);

//   String sensorVal = "Temp:" + String(T) + " Pres:" + String(P);


//   return sensorVal;
// }

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

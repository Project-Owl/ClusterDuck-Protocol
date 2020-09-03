#include <ClusterDuck.h>
#include "timer.h"
#include <TinyGPS++.h>
//#include <axp20x.h>

TinyGPSPlus gps;
HardwareSerial GPS(1);
//AXP20X_Class axp;

auto timer = timer_create_default(); // create a timer with default settings

//Setup BMP180
#include <Adafruit_BMP085_U.h>
Adafruit_BMP085_Unified bmp = Adafruit_BMP085_Unified(10085);

ClusterDuck duck;

void setup() {
  
  // put your setup code here, to run once:
  duck.begin();
  duck.setDeviceId("rD");
  duck.setupMamaDuck();

  //Temperature and pressure
  if(!bmp.begin())
  {
    /* There was a problem detecting the BMP085 ... check your connections */
    Serial.print("Ooops, no BMP085 detected ... Check your wiring or I2C ADDR!");
    while(1);
  } else {
    Serial.println("BMP on");
  }

  timer.every(3000, runSensor);

  //GPS Setup
//  Wire.begin(21, 22);
//  if (!axp.begin(Wire, AXP192_SLAVE_ADDRESS)) {
//    Serial.println("AXP192 Begin PASS");
//  } else {
//    Serial.println("AXP192 Begin FAIL");
//  }
//  axp.setPowerOutPut(AXP192_LDO2, AXP202_ON);
//  axp.setPowerOutPut(AXP192_LDO3, AXP202_ON);
//  axp.setPowerOutPut(AXP192_DCDC2, AXP202_ON);
//  axp.setPowerOutPut(AXP192_EXTEN, AXP202_ON);
//  axp.setPowerOutPut(AXP192_DCDC1, AXP202_ON);
  GPS.begin(9600, SERIAL_8N1, 34, 12);

  timer.every(3600, runGPS);
}

void loop() {
  timer.tick();
  
  // put your main code here, to run repeatedly:
  duck.runMamaDuck();
  
//  Serial.print("Latitude  : ");
//  Serial.println(gps.location.lat(), 5);
//  Serial.print("Longitude : ");
//  Serial.println(gps.location.lng(), 4);
//  Serial.print("Satellites: ");
//  Serial.println(gps.satellites.value());
//  Serial.print("Altitude  : ");
//  Serial.print(gps.altitude.feet() / 3.2808);
//  Serial.println("M");
//  Serial.print("Time      : ");
//  Serial.print(gps.time.hour());
//  Serial.print(":");
//  Serial.print(gps.time.minute());
//  Serial.print(":");
//  Serial.println(gps.time.second());
//  Serial.print("Speed     : ");
//  Serial.println(gps.speed.kmph()); 
//  Serial.println("**********************");
//  String sensorVal = "Lat: " + String(gps.location.lat(), 5) + " Lng: " + String(gps.location.lng(), 4);
//  Serial.println(sensorVal);
//  Serial.println(" ");
//  duck.sendPayloadStandard(sensorVal, "gps");
//  smartDelay(7500);
//  if (millis() > 5000 && gps.charsProcessed() < 10)
//    Serial.println(F("No GPS data received: check wiring"));
//
}

bool runSensor(void *) {
  float T,P;
  
  bmp.getTemperature(&T);
//  Serial.println(T);
  bmp.getPressure(&P);
//  Serial.println(P);
  
  String sensorVal = "Temp: " + String(T) + " Pres: " + String(P);
  Serial.println(sensorVal);
  duck.sendPayloadStandard(sensorVal, "BMP");

  return true;
}

static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do
  {
    while (GPS.available())
      gps.encode(GPS.read());
  } while (millis() - start < ms);
}

bool runGPS(void *) {
  Serial.print("Latitude  : ");
  Serial.println(gps.location.lat(), 5);  
  Serial.print("Longitude : ");
  Serial.println(gps.location.lng(), 4);
  Serial.print("Satellites: ");
  Serial.println(gps.satellites.value());
//  Serial.print("Altitude  : ");
//  Serial.print(gps.altitude.feet() / 3.2808);
//  Serial.println("M");
//  Serial.print("Time      : ");
//  Serial.print(gps.time.hour());
//  Serial.print(":");
//  Serial.print(gps.time.minute());
//  Serial.print(":");
//  Serial.println(gps.time.second());
//  Serial.print("Speed     : ");
//  Serial.println(gps.speed.kmph());
  Serial.println("**********************");
  
  String sensorVal = "Lat: " + String(gps.location.lat(), 5) + " Lng: " + String(gps.location.lng(), 4);

  smartDelay(7500);
  
  if (millis() > 5000 && gps.charsProcessed() < 10)
  {
    Serial.println(F("No GPS data received: check wiring"));
  }
  Serial.println(" ");
  Serial.println(sensorVal);
  duck.sendPayloadStandard(sensorVal, "gps");
  return true;
}

#include <LoRa.h>
#include <IridiumSBD.h>
#include <ClusterDuck.h>

//RockBlock PinSetup & Serial
#define IridiumSerial Serial2
#define DIAGNOSTICS false// Change this to see diagnostics
#define RXD2 12
#define TXD2 13

// Declare the IridiumSBD object
IridiumSBD modem(IridiumSerial);


ClusterDuck duck;

auto timer = timer_create_default(); // create a timer with default settings

byte ping = 0xF4;

void setup() {
  // put your setup code here, to run once:
  duck.begin();
  duck.setDeviceId("Dish Duck");
  duck.setupLoRa();
  LoRa.receive();
  duck.setupDisplay("DishDuck");

 //RockBlock Setup
  int signalQuality = -1;
  int err;
  
  // Start the console serial port
  Serial.begin(115200);
  while (!Serial);

  // Start the serial port connected to the satellite modem
  IridiumSerial.begin(19200);

  // Begin satellite modem operation
  Serial.println("Starting modem...");
  err = modem.begin();
  if (err != ISBD_SUCCESS)
  {
    Serial.print("Begin failed: error ");
    Serial.println(err);
    if (err == ISBD_NO_MODEM_DETECTED)
      Serial.println("No modem detected: check wiring.");
    return;
  }
  Serial.println("Space Duck Online");
}

void loop() {
  // put your main code here, to run repeatedly:
  int packetSize = LoRa.parsePacket();
  if (packetSize != 0) {
    byte whoIsIt = LoRa.peek();
    if(whoIsIt != ping) {
      Serial.println(packetSize);
      String * val = duck.getPacketData(packetSize);
      Packet lastPacket = duck.getLastPacket();

   // Send the message
  Serial.print("Trying to send the message.  This might take several minutes.\r\n");
  err = modem.sendSBDText("lastpacket");
  if (err != ISBD_SUCCESS)
  {
    Serial.print("sendSBDText failed: error ");
    Serial.println(err);
    if (err == ISBD_SENDRECEIVE_TIMEOUT)
      Serial.println("Try again with a better view of the sky.");
  }

  else
  {
    Serial.println("Hey, it worked!");
  }
}
      
  }
  timer.tick();
}

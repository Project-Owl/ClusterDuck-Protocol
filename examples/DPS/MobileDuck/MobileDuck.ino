#include <ClusterDuck.h>

ClusterDuck duck;

void setup() {
  
  duck.begin();
  duck.setDeviceId("M01");
  duck.setupDisplay("Mama");
  duck.setupLoRa();

  //Sets up wifi access point
  duck.setupWifiAp();

  //Setup DNS and IP
  duck.setupDns();

  //Setup web server and captive portal
  duck.setupWebServer(true);

  //Initialize ArduinoOTA for over the air updates
  duck.setupOTA();

  Serial.println("MamaDuck Online");
}

void loop() {
  if(duck.getFlag()) {

    //Flip lora flag for next message
    duck.flipFlag(); 

    //Flip interrupt to false
    duck.flipInterrupt(); 

    //Read and store lora packet
    int pSize = duck.handlePacket(); 
    Serial.print("runMamaDuck pSize ");
    Serial.println(pSize);

    //Make sure the packet has data
    if(pSize > 0) { 
      
      String msg = duck.getPacketData(pSize); 

      Packet lastPacket = duck.getLastPacket(); 

      //If packet is from StationaryDuck, then send RSSI data
      if(lastPacket.payload == "Here") {
        duck.sendPayloadStandard(String(duck.getRSSI()), "rssi", lastPacket.senderId, lastPacket.messageId, lastPacket.path); 
      }
     }
     
    //Flip interrupt to true
    duck.flipInterrupt(); 
    Serial.println("runMamaDuck startReceive");

    //Turn on Receiver
    duck.startReceive(); 
   }
}


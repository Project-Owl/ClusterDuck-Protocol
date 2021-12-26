/**
 * @file MamaDuck.ino
 * @brief Uses the built in Mama Duck.
 * 
 * This example is a Mama Duck, but it is also periodically sending a message in the Mesh
 * It is setup to provide a custom Emergency portal, instead of using the one provided by the SDK.
 * Notice the background color of the captive portal is Black instead of the default Red.
 * 
 */

#include <string>
#include <arduino-timer.h>
#include <MamaDuck.h>

#ifdef SERIAL_PORT_USBVIRTUAL
#define Serial SERIAL_PORT_USBVIRTUAL
#endif

// create a built-in mama duck
MamaDuck duck;

#define RXD2 0
#define TXD2 4

// create a timer with default settings
auto timer = timer_create_default();

// for sending the counter message
const int INTERVAL_MS = 60000;
int counter = 0;

std::vector<byte> serialBytes;

char state = 'A';

std::vector<char> piCmd;

struct {
   int nPackets;
   int packetSize;
   int currentPacket;
} FileInfo;

int headerSize = 0;

bool messageAcked = false;
bool sendAckState = false;
bool sendState = false;
bool bridgeOpenState = false;
std::vector<byte> ackBuffer;
std::vector<byte> headerBuffer;
bool buffLoaded = false;
bool gotPiAck = false;

void setup() {
   // We are using a hardcoded device id here, but it should be retrieved or
   // given during the device provisioning then converted to a byte vector to
   // setup the duck NOTE: The Device ID must be exactly 8 bytes otherwise it
   // will get rejected
   Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
   Serial2.println("Serial2 setup");
   std::string deviceId("MAMA0002");
   std::vector<byte> devId;
   devId.insert(devId.end(), deviceId.begin(), deviceId.end());
   duck.setupWithDefaults(devId);

   duck.onReceiveDuckData(handlLFTPacket);

   // Initialize the timer. The timer thread runs separately from the main loop
   // and will trigger sending a counter message.
   //timer.every(INTERVAL_MS, runSensor);
   Serial.println("[MAMA] Setup OK!");

  

}

void loop() {
   timer.tick();
   // Use the default run(). The Mama duck is designed to also forward data it receives
   // from other ducks, across the network. It has a basic routing mechanism built-in
   // to prevent messages from hoping endlessly.
   duck.run();

   if(Serial2.available() > 0) {
      int incoming = Serial2.available();
      while(Serial2.available() > 0) {
         byte piMsg = Serial2.read();
         switch(state) {
            case 'A':
            {
               
               if((char)piMsg == ':') {
                  std::string cmdString(piCmd.begin(), piCmd.end());
                  Serial.println((char)piMsg);
                  Serial.println(cmdString.c_str());
                  serialBytes.push_back(piMsg);
                  headerSize = headerSize + piCmd.size() + 1;
                  Serial.print("Header size ");
                  Serial.println(headerSize);
                  checkCommand(cmdString);
               } else {
                  char piChar = (char)piMsg;
                  piCmd.push_back(piChar);
                  Serial.println(piChar);
                  serialBytes.push_back(piMsg);
               }
               Serial.print("SerialBytes size A: ");
               Serial.println(serialBytes.size());

            }
               break;

            case 'B':
            {

               if((char)piMsg == ':') {
                  Serial.println(":");
                  serialBytes.push_back(piMsg);
                  counter++;
                  if(counter == 1) {
                  }
                  if(counter == 1) {
                     std::string temp;
                     headerSize = headerSize + piCmd.size() + 1;
                     Serial.print("Header size ");
                     Serial.println(headerSize);
                     for(int i = 0; i < piCmd.size(); i++) {
                        temp = temp + piCmd[i];
                     }
                     int cur = atoi(temp.c_str());
                     FileInfo.packetSize = cur;
                     Serial.println(cur);
                     piCmd.clear();
                     piCmd.shrink_to_fit();
                     Serial2.write("AK:");
                     Serial.println("Sent AK");
                     state = 'C';
                     Serial.println("Change state B to C");

                     counter = 0;
                  }
                  
               } else {
                  char piChar = (char)piMsg;
                  piCmd.push_back(piChar);
                  Serial.println("Add to piCmd");
                  Serial.println(piChar);
                  serialBytes.push_back(piMsg);
               }

            }
               break;

            case 'C':
            {

               char piChar = (char)piMsg;
               Serial.println(piChar);
               
               serialBytes.push_back(piMsg);
               Serial.print("SerialBytes size: ");
               Serial.println(serialBytes.size());

               if(serialBytes.size() == FileInfo.packetSize + headerSize) {
                  Serial.print("Before add header ");
                  Serial.println(serialBytes.size());
                  serialBytes.insert(serialBytes.begin(), (byte)headerSize);
                  headerSize = 0;
                  Serial.print("Before add header ");
                  Serial.println(serialBytes.size());
                  for(int i = 0; i < serialBytes.size(); i++) {
                     Serial.print((char)serialBytes[i]);
                  }
                  Serial.println("");
                  int err = duck.sendData(topics::lft, serialBytes);
                  if(err == DUCK_ERR_NONE) {
                     serialBytes.clear();
                     serialBytes.shrink_to_fit();
                     delay(200);
                     // Serial2.write("AK:");
                     // Serial.println("Sent AK");
                     state = 'A';
                     Serial.println("Change state C to A");
                  }

               }

            }
               break;

            default:
               Serial.println(state);
               Serial.println("Invalid state");
         }
      
      }
   }

   if(messageAcked) {

      if(bridgeOpenState) {
         Serial2.write("AK:");
         Serial.println("message acked bridge open");
         Serial2.write("BO:200:");
      }

      if(!bridgeOpenState) {
         Serial.println("message acked from next packet");
         Serial2.write("AK:");
         Serial.println("Sent AK");
         messageAcked = false;
         duck.setLFTMessageAck(false);
      } else {
         bridgeOpenState = false;
      }
      
   }

   if(sendAckState) {
      std::vector<byte> temp = {'A','K',':'}; 
      int err = duck.sendData(topics::lft, temp);
      if(err == DUCK_ERR_NONE) {
         bridgeOpenState = true;
      }
   }

   if(!sendState) {
      if(buffLoaded && headerBuffer.size() > 0) {
         Serial.println("Write cmd header to pi");
         for(int i = 0; i < headerBuffer.size(); i++) {
            Serial2.write(headerBuffer[i]);
         }
         headerBuffer.clear();
         headerBuffer.shrink_to_fit();
      } else if(buffLoaded && headerBuffer.size() < 1 && gotPiAck == true) {
         Serial.println("Write packet buffer to pi");
         for(int i = 0; i < ackBuffer.size(); i++) {
            Serial2.write(ackBuffer[i]);
         }
         ackBuffer.clear();
         ackBuffer.shrink_to_fit();
         gotPiAck = false;
         buffLoaded = false;
      }
   }
  
}

void handlLFTPacket(std::vector<byte> packetBuffer) {
   Serial.println("LFT Packet Received");
   std::string cmdCB = "CB:";
   std::string openBridge = "OB:";
   std::string packetIncoming = "PK:";
   CdpPacket packet = packetBuffer;
   std::string payload(packet.data.begin(), packet.data.end());

   std::string cmd;
   int len = packet.data[0];
   for(int i = 1; i < 4; i++) {
      cmd = cmd + (char)packet.data[i];
   }

   Serial.print("Message received: ");
   Serial.println(cmd.c_str());

   if(cmd == openBridge) {
      sendState = false;
      Serial.println("Got Open Bridge Request");
      Serial2.write("OB:");
   }

   if(cmd == packetIncoming) {
      Serial.println("Got Next Packet Request");
      Serial.println(packet.data.size());

      for(int i = 1; i < (int)packet.data[0]; i++) {
         headerBuffer.push_back(packet.data[i]);
      }

      for(int i = (int)packet.data[0]; i < packet.data.size(); i++) {
         ackBuffer.push_back(packet.data[i]);
      }

      buffLoaded = true;
      Serial.println("");

   }

   if(payload == cmdCB) {
      sendState = false;
      Serial.println("Received close bridge!!!!!!!!");
      Serial2.write("CB:");
   }
}

void checkCommand(std::string cmd) {
   std::string openBridge = "OB";
   std::string packetIncoming = "PK";
   std::string ack = "AK";

   if(cmd == openBridge) {
      Serial.println("Open bridge");
      //SEND OB to duck
      //Serial2.write("AK:");
      serialBytes.clear();
      serialBytes.shrink_to_fit();
      headerSize = 0;
      sendState = true;
      std::vector<byte> temp = {'O','B',':'};
      temp.insert(temp.begin(),(byte)3); 
      int err = duck.sendData(topics::lft, temp);
      if(err == DUCK_ERR_NONE) {
         bridgeOpenState = true;
      }
      
   } else {
      Serial.println("no Open bridge");
   }

   if(cmd == packetIncoming) {
      Serial.println("packetIncoming");
      state = 'B';
      Serial.println("Change state B");
      Serial.println(serialBytes.size());
      for(int i = 0; i < serialBytes.size(); i++) {
         Serial.println((char)serialBytes[i]);
      }
   } else {
      Serial.println("no packetIncoming");
   }

   if(cmd == ack) {
      Serial.println("ack");
      serialBytes.clear();
      serialBytes.shrink_to_fit();
      headerSize = 0;
      Serial.print("SerialBytes size: ");
      Serial.println(serialBytes.size());
      if(!sendState) {
         if(buffLoaded) {
            gotPiAck = true;
         } else {
            std::vector<byte> temp = {'A','K',':'}; 
            int err = duck.sendData(topics::lft, temp);
            if(err == DUCK_ERR_NONE) {
               Serial.println("Ack sent to sender");
            }
         }
      }
   } else {
      Serial.println("no ack");
   }

   piCmd.clear();
   piCmd.shrink_to_fit();
 
}

bool runSensor(void *) {
   bool result;
   const byte* buffer;
  
   String message = String("Counter:") + String(counter);
   int length = message.length();
   Serial.print("[MAMA] sensor data: ");
   Serial.println(message);
   buffer = (byte*) message.c_str(); 

   result = sendData(buffer, length);
   if (result) {
      Serial.println("[MAMA] runSensor ok.");
   } else {
      Serial.println("[MAMA] runSensor failed.");
   }
   return result;
}

bool sendData(const byte* buffer, int length) {
   bool sentOk = false;
  
   // Send Data can either take a byte buffer (unsigned char) or a vector
   int err = duck.sendData(topics::status, buffer, length);
   if (err == DUCK_ERR_NONE) {
      counter++;
      sentOk = true;
   }
   if (!sentOk) {
      Serial.println("[MAMA] Failed to send data. error = " + String(err));
   }
   return sentOk;
}
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
bool bridgeOpenState = false;

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
                  Serial.println(cmdString.c_str());
                  checkCommand(cmdString);
                  serialBytes.push_back(piMsg);
                  headerSize = headerSize + piCmd.size() + 1;
               } else {
                  char piChar = (char)piMsg;
                  piCmd.push_back(piChar);
                  Serial.println(piChar);
                  serialBytes.push_back(piMsg);
               }

            }
               break;

            case 'B':
            {

               if((char)piMsg == ':') {
                  Serial.println(":");
                  serialBytes.push_back(piMsg);
                  counter++;
                  if(counter == 1) {
                     std::string temp;
                     headerSize = headerSize + piCmd.size() + 1;
                     for(int i = 0; i < piCmd.size(); i++) {
                        temp = temp + piCmd[i];
                     }
                     int cur = atoi(temp.c_str());
                     FileInfo.currentPacket = cur;
                     Serial.println(cur);
                     piCmd.clear();
                     piCmd.shrink_to_fit();
                  } else if(counter == 2) {
                     std::string temp;
                     headerSize = headerSize + piCmd.size() + 1;
                     for(int i = 0; i < piCmd.size(); i++) {
                        temp = temp + piCmd[i];
                     }
                     int cur = atoi(temp.c_str());
                     FileInfo.packetSize = cur;
                     Serial.println(cur);
                     piCmd.clear();
                     piCmd.shrink_to_fit();
                  }
                  if(counter == 3) {
                     std::string temp;
                     headerSize = headerSize + piCmd.size() + 1;
                     for(int i = 0; i < piCmd.size(); i++) {
                        temp = temp + piCmd[i];
                     }
                     int cur = atoi(temp.c_str());
                     FileInfo.nPackets = cur;
                     Serial.println(cur);
                     piCmd.clear();
                     piCmd.shrink_to_fit();
                     Serial2.write("AK:");
                     Serial.println("Sent AK");
                     state = 'C';

                     counter = 0;
                  }
                  
               } else {
                  char piChar = (char)piMsg;
                  piCmd.push_back(piChar);
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
                  serialBytes.insert(serialBytes.begin(), (byte)headerSize);
                  headerSize = 0;
                  // for(int i = 0; i < serialBytes.size(); i++) {
                  //    Serial.println((char)serialBytes[i]);
                  // }
                  int err = duck.sendData(topics::lft, serialBytes);
                  if(err == DUCK_ERR_NONE) {
                     serialBytes.clear();
                     serialBytes.shrink_to_fit();
                     delay(200);
                     // Serial2.write("AK:");
                     // Serial.println("Sent AK");
                     state = 'A';
                  }

               }

            }
               break;

            case 'D':
            {
               if((char)piMsg == ':') {
                  Serial.println(":");
                  serialBytes.push_back(piMsg);
                  counter++;
                  if(counter == 1) {
                     std::string temp;
                     headerSize = headerSize + piCmd.size() + 1;
                     for(int i = 0; i < piCmd.size(); i++) {
                        temp = temp + piCmd[i];
                     }
                     int cur = atoi(temp.c_str());
                     FileInfo.currentPacket = cur;
                     Serial.println(cur);
                     piCmd.clear();
                     piCmd.shrink_to_fit();
                  } else if(counter == 2) {
                     std::string temp;
                     headerSize = headerSize + piCmd.size() + 1;
                     for(int i = 0; i < piCmd.size(); i++) {
                        temp = temp + piCmd[i];
                     }
                     int cur = atoi(temp.c_str());
                     FileInfo.packetSize = cur;
                     Serial.println(cur);
                     piCmd.clear();
                     piCmd.shrink_to_fit();
                     state = 'C';
                     Serial.println("Change State");

                     headerSize = headerSize + 2;

                     counter = 0;
                  }
                  
               } else {
                  char piChar = (char)piMsg;
                  piCmd.push_back(piChar);
                  Serial.println(piChar);
                  serialBytes.push_back(piMsg);
               }
            }
               break;
            default:
               Serial.println(state);
               Serial.println("Invalid state");
         }
      
      }
   }

   messageAcked = duck.getLFTMessageAck();
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
  
}

void checkCommand(std::string cmd) {
   std::string openBridge = "OB";
   std::string firstPacket = "FP";
   std::string nextPacket = "NP";
   std::string lastPacket = "LP";
   std::string ack = "AK";

   if(cmd == openBridge) {
      Serial.println("Open bridge");
      //SEND OB to duck
      //Serial2.write("AK:");
      serialBytes.clear();
      serialBytes.shrink_to_fit();
      std::vector<byte> temp = {'O','B',':'}; 
      int err = duck.sendData(topics::lft, temp);
      if(err == DUCK_ERR_NONE) {
         bridgeOpenState = true;
      }
      
   } else {
      Serial.println("no Open bridge");
   }

   if(cmd == firstPacket) {
      Serial.println("firstPacket");
      state = 'B';
      Serial.println("Change state");
      serialBytes.erase(serialBytes.begin());
      Serial.println(serialBytes.size());
      for(int i = 0; i < serialBytes.size(); i++) {
         Serial.println((char)serialBytes[i]);
      }
   } else {
      Serial.println("no firstPacket");
   }

   if(cmd == nextPacket) {
      Serial.println("nextPacket");
      state = 'D';
      Serial.println("Change state");
      // Serial2.write("AK:");
      // Serial.println("Sent AK");
   } else {
      Serial.println("no nextPacket");
   }

   if(cmd == lastPacket) {
      Serial.println("lastPacket");
      state = 'D';
      Serial.println("Change state");
      Serial2.write("AK:");
      Serial.println("Sent AK");
   } else {
      Serial.println("no lastPacket");
   }

   if(cmd == ack) {
      Serial.println("ack");
      serialBytes.clear();
      serialBytes.shrink_to_fit();
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
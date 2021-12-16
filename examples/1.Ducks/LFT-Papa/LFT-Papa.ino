/**
 * @file PapaDuck.ino
 * @author 
 * @brief Uses built-in PapaDuck from the SDK to create a WiFi enabled Papa Duck
 * 
 * This example will configure and run a Papa Duck that connects to the cloud
 * and forwards all messages (except  pings) to the cloud. When disconnected
 * it will add received packets to a queue. When it reconnects to MQTT it will
 * try to publish all messages in the queue. You can change the size of the queue
 * by changing `QUEUE_SIZE_MAX`.
 * 
 * @date 2021-06-17
 * 
 */

#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <arduino-timer.h>
#include <string>

/* CDP Headers */
#include <PapaDuck.h>
#include <CdpPacket.h>
#include <queue>

#define MQTT_RETRY_DELAY_MS 500
#define WIFI_RETRY_DELAY_MS 5000

//Uncomment CA_CERT if you want to use the certificate auth method
//#define CA_CERT
#ifdef CA_CERT
const char* example_root_ca = \
  "-----BEGIN CERTIFICATE-----\n" \
  "MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\n" \
  "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n" \
  "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\n" \
  "QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\n" \
  "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n" \
  "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG\n" \
  "9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB\n" \
  "CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97\n" \
  "nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n" \
  "43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P\n" \
  "T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4\n" \
  "gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO\n" \
  "BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR\n" \
  "TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw\n" \
  "DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr\n" \
  "hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n" \
  "06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF\n" \
  "PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls\n" \
  "YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\n" \
  "CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=\n" \
  "-----END CERTIFICATE-----\n";
  // This is the DigiCert Global Root CA, which is the root CA cert for
  // https://internetofthings.ibmcloud.com/
  // It expires November 9, 2031.
  // To connect to a different cloud provider or server, you may need to use a
  // different cert. For details, see
  // https://github.com/espressif/arduino-esp32/tree/master/libraries/WiFiClientSecure
#endif

#define SSID ""
#define PASSWORD ""


// Used for Mqtt client connection
// Provided when a Papa Duck device is created in DMS
#define ORG         ""
#define DEVICE_ID   ""
#define DEVICE_TYPE ""
#define TOKEN       ""
char server[] = ORG ".messaging.internetofthings.ibmcloud.com";
const int port = 8883;
char authMethod[] = "use-token-auth";
char token[] = TOKEN;
char clientId[] = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

// Use pre-built papa duck
PapaDuck duck;

#define RXD2 0
#define TXD2 4

std::vector<byte> ackBuffer;
bool buffLoaded = false;

DuckDisplay* display = NULL;

bool use_auth_method = true;

auto timer = timer_create_default();

int QUEUE_SIZE_MAX = 5;
std::queue<std::vector<byte>> packetQueue;

WiFiClientSecure wifiClient;
PubSubClient client(server, port, wifiClient);
// / DMS locator URL requires a topicString, so we need to convert the topic
// from the packet to a string based on the topics code
std::string toTopicString(byte topic) {

  std::string topicString;

  switch (topic) {
    case topics::status:
      topicString = "status";
      break;
    case topics::cpm:
      topicString = "portal";
      break;
    case topics::sensor:
      topicString = "sensor";
      break;
    case topics::alert:
      topicString = "alert";
      break;
    case topics::location:
      topicString = "gps";
      break;
    case topics::health:
      topicString ="health";
      break;
    default:
      topicString = "status";
  }

  return topicString;
}

String convertToHex(byte* data, int size) {
  String buf = "";
  buf.reserve(size * 2); // 2 digit hex
  const char* cs = "0123456789ABCDEF";
  for (int i = 0; i < size; i++) {
    byte val = data[i];
    buf += cs[(val >> 4) & 0x0F];
    buf += cs[val & 0x0F];
  }
  return buf;
}

// WiFi connection retry
bool retry = true;
int quackJson(std::vector<byte> packetBuffer) {

  CdpPacket packet = CdpPacket(packetBuffer);
  const int bufferSize = 4 * JSON_OBJECT_SIZE(4);
  StaticJsonDocument<bufferSize> doc;

  // Here we treat the internal payload of the CDP packet as a string
  // but this is mostly application dependent. 
  // The parsingf here is optional. The Papa duck could simply decide to
  // forward the CDP packet as a byte array and let the Network Server (or DMS) deal with
  // the parsing based on some business logic.

  std::string payload(packet.data.begin(), packet.data.end());
  std::string sduid(packet.sduid.begin(), packet.sduid.end());
  std::string dduid(packet.dduid.begin(), packet.dduid.end());

  std::string muid(packet.muid.begin(), packet.muid.end());
  std::string path(packet.path.begin(), packet.path.end());

  Serial.println("[PAPA] Packet Received:");
  Serial.println("[PAPA] sduid:   " + String(sduid.c_str()));
  Serial.println("[PAPA] dduid:   " + String(dduid.c_str()));

  Serial.println("[PAPA] muid:    " + String(muid.c_str()));
  Serial.println("[PAPA] path:    " + String(path.c_str()));
  Serial.println("[PAPA] data:    " + String(payload.c_str()));
  Serial.println("[PAPA] hops:    " + String(packet.hopCount));
  Serial.println("[PAPA] duck:    " + String(packet.duckType));

  doc["DeviceID"] = sduid;
  doc["MessageID"] = muid;
  doc["Payload"].set(payload);
  doc["path"].set(path);
  doc["hops"].set(packet.hopCount);
  doc["duckType"].set(packet.duckType);

  std::string cdpTopic = toTopicString(packet.topic);

  display->clear();
  display->drawString(0, 10, "New Message");
  display->drawString(0, 20, sduid.c_str());
  display->drawString(0, 30, muid.c_str());
  display->drawString(0, 40, cdpTopic.c_str());
  display->sendBuffer();
    
  std::string topic = "iot-2/evt/" + cdpTopic + "/fmt/json";

  String jsonstat;
  serializeJson(doc, jsonstat);

  if (client.publish(topic.c_str(), jsonstat.c_str())) {
    Serial.println("[PAPA] Packet forwarded:");
    serializeJsonPretty(doc, Serial);
    Serial.println("");
    Serial.println("[PAPA] Publish ok");
    display->drawString(0, 60, "Publish ok");
    display->sendBuffer();
    return 0;
  } else {
    Serial.println("[PAPA] Publish failed");
    display->drawString(0, 60, "Publish failed");
    display->sendBuffer();
    return -1;
  }
}

char state = 'A';

// The callback method simply takes the incoming packet and
// converts it to a JSON string, before sending it out over WiFi
void handleDuckData(std::vector<byte> packetBuffer) {
  Serial.println("[PAPA] got packet: " +
                 convertToHex(packetBuffer.data(), packetBuffer.size()));
   CdpPacket packet = CdpPacket(packetBuffer);
   if(packet.topic == topics::lft) {

      switch(state) {
         case 'A':
         {
            std::string openBridge = "OB:";
            std::string firstPacket = "FP:";
            std::string nextPacket = "NP:";
            std::string lastPacket = "LP:";

            std::string cmd;
            int len = packet.data[0];
            for(int i = 1; i < 4; i++) {
               cmd = cmd + (char)packet.data[i];
            }

            if(cmd == openBridge) {
               Serial.println("Got Open Bridge Request");
               Serial2.write("OB:");
            }

            if(cmd == firstPacket) {
               Serial.println("Got First Packet Request");
               for(int i = 1; i < 4; i++) {
                  Serial2.write(packet.data[i]);
                  Serial.println((char)packet.data[i]);
               }

               for(int i = (int)packet.data[0] - 1; i < packet.data.size(); i++) {
                  Serial.print((char)packet.data[i]);
                  //Serial2.write(packet.data[i]);
                  ackBuffer.insert(ackBuffer.end(), packet.data[i]);
               }
               buffLoaded = true;
               Serial.println("");

               
            }

            if(cmd == nextPacket) {
               Serial.println("Got Next Packet Request");
               for(int i = 1; i < packet.data.size(); i++) {
                  Serial.print((char)packet.data[i]);
                  Serial2.write(packet.data[i]);
               }
               Serial.println("");

            }

            if(cmd == lastPacket) {
               Serial.println("Got Last Packet Request");
               for(int i = 1; i < packet.data.size(); i++) {
                  Serial.print((char)packet.data[i]);
                  Serial2.write(packet.data[i]);
               }
               Serial.println("");

            }
         }

            break;
         
         case 'B':

            for(int i = 0; i < packet.data.size(); i++) {
               Serial2.write(packet.data[i]);
            }
            break;

         default:
            Serial.println("Invalid state");
      }

   } else {
      if(quackJson(packetBuffer) == -1) {
    if(packetQueue.size() > QUEUE_SIZE_MAX) {
      packetQueue.pop();
      packetQueue.push(packetBuffer);
    } else {
      packetQueue.push(packetBuffer);
    }
    Serial.print("New size of queue: ");
    Serial.println(packetQueue.size());
  }
   }
  
}

void setup() {
  // We are using a hardcoded device id here, but it should be retrieved or
  // given during the device provisioning then converted to a byte vector to
  // setup the duck NOTE: The Device ID must be exactly 8 bytes otherwise it
  // will get rejected
   Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
   Serial2.println("Serial2 setup");

  std::string deviceId("PAPADUCK");
  std::vector<byte> devId;
  devId.insert(devId.end(), deviceId.begin(), deviceId.end());

  // the default setup is equivalent to the above setup sequence
  duck.setupWithDefaults(devId, SSID, PASSWORD);

  display = DuckDisplay::getInstance();
  // DuckDisplay instance is returned unconditionally, if there is no physical
  // display the functions will not do anything
  display->setupDisplay(duck.getType(), devId);

  // register a callback to handle incoming data from duck in the network
  duck.onReceiveDuckData(handleDuckData);

  #ifdef CA_CERT
  Serial.println("[PAPA] Using root CA cert");
  wifiClient.setCACert(example_root_ca);
  #else
  Serial.println("[PAPA] Using insecure TLS");
  wifiClient.setInsecure();
  #endif

  duck.enableAcks(true);

  Serial.println("[PAPA] Setup OK! ");
  
  // we are done
  display->showDefaultScreen();
}

std::vector<char> piCmd;
std::string ackk = "AK";

void loop() {
  if (!duck.isWifiConnected() && retry) {
    String ssid = duck.getSsid();
    String password = duck.getPassword();

    Serial.println("[PAPA] WiFi disconnected, reconnecting to local network: " +
                   ssid);

    int err = duck.reconnectWifi(ssid, password);

    if (err != DUCK_ERR_NONE) {
      retry = false;
      timer.in(5000, enableRetry);
    }
  }
  if (duck.isWifiConnected() && retry) {
    setup_mqtt(use_auth_method);
  }
   
   while(Serial2.available() > 0) {
      byte piMsg = Serial2.read();
      if((char)piMsg == ':') {
         std::string cmdString(piCmd.begin(), piCmd.end());
         Serial.println(cmdString.c_str());

         if(cmdString == ackk) {
            Serial.println("Ack Received send buffer");
            if(buffLoaded) {
               for(int i = 0; i < ackBuffer.size(); i++) {
                  Serial2.write(ackBuffer[i]);
               }
            }
         }

         piCmd.clear();
         piCmd.shrink_to_fit();
      } else {
         char piChar = (char)piMsg;
         piCmd.push_back(piChar);
         Serial.println(piChar);
      }
   }

  duck.run();
  timer.tick();
}

void setup_mqtt(bool use_auth) {
  bool connected = client.connected();
  if (connected) {

    //Once reconnected check queue and publish all queued messages
    if(packetQueue.size() > 0) {
      publishQueue();
    }
    return;
  }

  
  if (use_auth) {
    connected = client.connect(clientId, authMethod, token);
  } else {
    connected = client.connect(clientId);
  }
  if (connected) {
    if(packetQueue.size() > 0) {
      publishQueue();
    }
    Serial.println("[PAPA] Mqtt client is connected!");
    return;
  }
  retry_mqtt_connection(1000);
  
}

bool enableRetry(void*) {
  retry = true;
  return retry;
}

void retry_mqtt_connection(int delay_ms) {
  Serial.println("[PAPA] Could not connect to MQTT...............................");
  retry = false;
  timer.in(delay_ms, enableRetry);
}

void publishQueue() {
  while(!packetQueue.empty()) {
    if(quackJson(packetQueue.front()) == 0) {
      packetQueue.pop();
      Serial.print("Queue size: ");
      Serial.println(packetQueue.size());
    } else {
      return;
    }
  }
}
#include <ClusterDuck.h>

#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "timer.h"

<<<<<<< HEAD
#define SSID        "Brave24"
#define PASSWORD    "tcrack003"
||||||| merged common ancestors
#define SSID        "HEINEKEN"
#define PASSWORD    "duckduckowl"
=======
#define SSID        ""
#define PASSWORD    ""
>>>>>>> 9a6d9ca3cf53976c7e3cc827566f551f60c6b63c

<<<<<<< HEAD
#define ORG         "9c6nfo"
#define DEVICE_ID   "tCrackhouse"
#define DEVICE_TYPE "PAPA"
#define TOKEN       "FcKVN2D+GH0SnYGynR"
||||||| merged common ancestors
#define ORG         "9c6nfo"
#define DEVICE_ID   "TIMO_DUCK"
#define DEVICE_TYPE "PAPA"
#define TOKEN       "qQTQ5q(4qvAVSlxdHu"
=======
#define ORG         ""
#define DEVICE_ID   ""
#define DEVICE_TYPE ""
#define TOKEN       ""
>>>>>>> 9a6d9ca3cf53976c7e3cc827566f551f60c6b63c

char server[]           = ORG ".messaging.internetofthings.ibmcloud.com";
char authMethod[]       = "use-token-auth";
char token[]            = TOKEN;
char clientId[]         = "d:" ORG ":" DEVICE_TYPE ":" DEVICE_ID;

ClusterDuck duck;

auto timer = timer_create_default(); // create a timer with default settings

WiFiClientSecure wifiClient;
PubSubClient client(server, 8883, wifiClient);

byte ping = 0xF4;
bool retry = true;

void setup() {
  // put your setup code here, to run once:

  duck.begin();
  duck.setDeviceId("Papa");

  duck.setupLoRa();
  duck.setupDisplay("Papa");

  const char * ap = "PapaDuck Setup";
  duck.setupWifiAp(ap);
	duck.setupDns();

	duck.setupInternet(SSID, PASSWORD);
  duck.setupWebServer();

  Serial.println("PAPA Online");
}

void loop() {
  // put your main code here, to run repeatedly:
  if(WiFi.status() != WL_CONNECTED && retry)
  {
    Serial.print("WiFi disconnected, reconnecting to local network: ");
<<<<<<< HEAD
    Serial.print(duck.getSSID());
    duck.setupInternet(duck.getSSID(), duck.getPassword());
		duck.setupDns();
||||||| merged common ancestors
    Serial.print(SSID);
    setupWiFi();

=======
    Serial.print(duck.getSSID());
    if(duck.ssidAvailable()) {
      duck.setupInternet(duck.getSSID(), duck.getPassword());
		  duck.setupDns();
    } else {
      retry = false;
      timer.in(5000, enableRetry);
    }
    
>>>>>>> 9a6d9ca3cf53976c7e3cc827566f551f60c6b63c
  }
  if(WiFi.status() == WL_CONNECTED) setupMQTT();

  if(duck.getFlag()) {  //If LoRa packet received
    duck.flipFlag();
    duck.flipInterrupt();
    int pSize = duck.handlePacket();
    if(pSize > 3) {
      duck.getPacketData(pSize);
      quackJson();

    }
    duck.flipInterrupt();
    duck.startReceive();
  }

<<<<<<< HEAD

  timer.tick();
||||||| merged common ancestors
  
  timer.tick();
}

void setupWiFi()
{
  Serial.println();
  Serial.print("Connecting to ");
  Serial.print(SSID);

  // Connect to Access Poink
  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED)
  {
    timer.tick(); //Advance timer to reboot after awhile
    //delay(500);
    Serial.print(".");
  }

  // Connected to Access Point
  Serial.println("");
  Serial.println("WiFi connected - PAPA ONLINE");
=======

  timer.tick();
>>>>>>> 9a6d9ca3cf53976c7e3cc827566f551f60c6b63c
}

void setupMQTT()
{
  if (!!!client.connected()) {
    Serial.print("Reconnecting client to "); Serial.println(server);
    while ( ! (ORG == "quickstart" ? client.connect(clientId) : client.connect(clientId, authMethod, token)))
    {
      timer.tick(); //Advance timer to reboot after awhile
      Serial.print("i");
      delay(500);
    }
  }
}

void quackJson() {
  const int bufferSize = 4*  JSON_OBJECT_SIZE(4);
  StaticJsonDocument<bufferSize> doc;

  JsonObject root = doc.as<JsonObject>();

  Packet lastPacket = duck.getLastPacket();

  doc["DeviceID"]        = lastPacket.senderId;
  doc["MessageID"]       = lastPacket.messageId;
  doc["Payload"]     .set(lastPacket.payload);
  doc["path"]         .set(lastPacket.path + "," + duck.getDeviceId());

  String loc = "iot-2/evt/"+ lastPacket.topic +"/fmt/json";
  Serial.print(loc);
  // add space for null char
  int len = loc.length() + 1;

  char topic[len];
  loc.toCharArray(topic, len);

  String jsonstat;
  serializeJson(doc, jsonstat);

  if (client.publish(topic, jsonstat.c_str())) {

    serializeJsonPretty(doc, Serial);
     Serial.println("");
    Serial.println("Publish ok");

  }
  else {
    Serial.println("Publish failed");
  }

}

bool enableRetry(void *) {
  retry = true;
}

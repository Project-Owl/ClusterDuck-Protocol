#pragma once
#include <WiFiClient.h>
class PubSubClient {
public:
    struct Publication { std::string topic, json; };
    std::vector<Publication> published;
    WiFiClient& transport;
    bool active = false, brokerAvailable = true, publishOK = true, bufferOK = true;
    uint16_t bufferSize = 256;
    unsigned connects = 0;
    explicit PubSubClient(WiFiClient& client) : transport(client) {}
    void setServer(IPAddress, uint16_t) {}
    void setSocketTimeout(uint16_t) {}
    void setKeepAlive(uint16_t) {}
    bool setBufferSize(uint16_t value) { bufferSize = value; return bufferOK; }
    bool connected() { return active && transport.active && brokerAvailable; }
    bool connect(const char*) { ++connects; active = brokerAvailable; return connected(); }
    void loop() {}
    void disconnect() { active = false; transport.stop(); }
    bool publish(const char* topic, const char* json) {
        if (!connected() || !publishOK || 7 + std::strlen(topic) + std::strlen(json) > bufferSize) return false;
        published.push_back({topic, json});
        return true;
    }
};

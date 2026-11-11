#pragma once
#include <WiFi.h>
class WiFiClient {
public:
    bool available = true, active = false;
    unsigned connects = 0;
    int timeout = 0;
    int connect(IPAddress, uint16_t, int32_t timeoutMs) {
        ++connects; timeout = timeoutMs; active = available; return active;
    }
    void stop() { active = false; }
};

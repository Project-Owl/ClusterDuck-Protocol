#pragma once
#include <Arduino.h>
constexpr int WIFI_STA = 1, WL_CONNECTED = 3;
struct IPAddress {
    std::array<uint8_t, 4> bytes;
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : bytes{a,b,c,d} {}
};
struct HostWiFi {
    int state = 0;
    unsigned reconnects = 0;
    void mode(int) {}
    void setAutoReconnect(bool) {}
    void begin(const char*, const char*) {}
    int status() { return state; }
    bool reconnect() { ++reconnects; return true; }
};
inline HostWiFi WiFi;

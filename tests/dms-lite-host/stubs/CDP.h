#pragma once
// Real packet types, serializers and error codes; only the device is simulated.
#include <ArduinoJson.h>
#include "CdpPacket.h"
inline int setupResult = DUCK_ERR_NONE;
template<class WifiCapability = DuckWifiNone> class PapaDuck {
public:
    explicit PapaDuck(const char*) {}
    void (*receive)(CdpPacket) = nullptr;
    unsigned runs = 0;
    int setupWithDefaults() { Serial.begin(115200); return setupResult; }
    void onReceiveDuckData(void (*cb)(CdpPacket)) { receive = cb; }
    void run() { ++runs; }
};

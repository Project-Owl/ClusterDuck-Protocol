#include <cassert>
#include <iostream>
#include <sstream>

#ifdef DMS_LITE_MQTT
#include "../../examples/Custom-Papa-Examples/PapiDuck-DMS-Lite-WiFi-Example/PapiDuck-DMS-Lite-WiFi-Example.ino"
#else
#include "../../examples/Custom-Papa-Examples/PapiDuck-DMS-Lite-Serial-Example/PapiDuck-DMS-Lite-Serial-Example.ino"
#endif

CdpPacket packet(const std::string& payload = "23.5", uint8_t topic = topics::sensor) {
    CdpPacket p;
    const std::string name = "MAMADUCK";
    std::copy(name.begin(), name.end(), p.sduid.begin());
    p.muid = {0x00, 0xFF, 0x0A, 0x4B};
    p.topic = topic;
    p.data.assign(payload.begin(), payload.end());
    p.hopCount = 2;
    p.duckType = DuckType::MAMA;
    return p;
}

JsonDocument parse(const std::string& json) {
    JsonDocument doc;
    assert(!deserializeJson(doc, json));
    return doc;
}

// Exercise the registered receive callback and the sketch's actual transport.
std::vector<std::string> send(CdpPacket p) {
    Serial.output.clear();
#ifdef DMS_LITE_MQTT
    mqttClient.published.clear();
#endif
    assert(duck.receive);
    duck.receive(p);
    loop();
    std::vector<std::string> output;
#ifdef DMS_LITE_MQTT
    for (const auto& publication : mqttClient.published) {
        assert(publication.topic == p.topicToString());
        output.push_back(publication.json);
    }
#else
    std::istringstream lines(Serial.output);
    for (std::string line; std::getline(lines, line);) {
        if (!line.empty() && line.front() == '{') output.push_back(line);
    }
#endif
    return output;
}

void checkPacket(CdpPacket p, const std::string& expectedPayload, bool hex = false) {
    const auto output = send(p);
    assert(output.size() == 1);
    const auto doc = parse(output.front());
    assert(doc["DeviceID"] == "MAMADUCK");
    assert(doc["MessageID"] == "00FF0A4B");
    assert(doc["Payload"].as<std::string>() == expectedPayload);
    assert(doc["PayloadEncoding"].is<const char*>() == hex);
    assert(doc["path"] == "");
    assert(doc["hops"].is<unsigned>() && doc["hops"] == 2);
    assert(doc["duckType"].is<unsigned>() && doc["duckType"].as<unsigned>() == unsigned(DuckType::MAMA));
    assert(doc["topic"].as<std::string>() == p.topicToString());
    assert(output.front().find('\n') == std::string::npos);
    std::cout << output.front() << '\n';  // also fed to the real Python adapters
}

int main() {
    setup();
    assert(setupOK && Serial.baud == 115200);
#ifdef DMS_LITE_MQTT
    // No network at boot: setup returns, and duck.run() still gets serviced.
    for (unsigned i = 0; i < 10; ++i) loop();
    assert(duck.runs == 10 && wifiClient.connects == 0);
    hostMillis = WIFI_RETRY_MS;
    loop();
    assert(WiFi.reconnects == 1);
    WiFi.state = WL_CONNECTED;
#endif

    checkPacket(packet(), "23.5");
    checkPacket(packet("-2.5"), "-2.5");
    checkPacket(packet(""), "");
    const std::string text = "quotes \" slash \\ line\n" + std::string("NUL\0end", 7) + " café 🌡";
    checkPacket(packet(text), text);
    // Largest valid radio payload with worst-case JSON control-byte escaping.
    checkPacket(packet(std::string(MAX_DATA_LENGTH, '\x01')), std::string(MAX_DATA_LENGTH, '\x01'));
    checkPacket(packet(std::string("\xFF\0\x80", 3)), "hex:FF0080", true);
    for (const auto& [bytes, expected] : std::vector<std::pair<std::string, std::string>>{
            {"\xC0\xAF", "hex:C0AF"}, {"\xED\xA0\x80", "hex:EDA080"},
            {"\xF4\x90\x80\x80", "hex:F4908080"}, {"\xE2\x82", "hex:E282"}}) {
        checkPacket(packet(bytes), expected, true);
    }
    for (const auto& bytes : {"\xC2\x80", "\xE0\xA0\x80", "\xF0\x90\x80\x80", "\xF4\x8F\xBF\xBF"}) {
        checkPacket(packet(bytes), bytes);
    }
    checkPacket(packet("{\"heap\":12345}", topics::health), "{\"heap\":12345}");
    for (uint8_t topic : {uint8_t(reservedTopic::ping), uint8_t(reservedTopic::rreq)}) {
        assert(send(packet("reserved", topic)).empty());
    }
    // Relay copies keep the same identity and are both forwarded.
    const auto first = send(packet()), second = send(packet());
    assert(first == second && first.size() == 1);
    auto binaryId = packet(); binaryId.sduid = {0, 0xFF, 2, 3, 4, 5, 6, 7};
    const auto encoded = send(binaryId);
    assert(parse(encoded.front())["DeviceID"] == "00FF020304050607");
    std::cout << encoded.front() << '\n';

#ifdef DMS_LITE_MQTT
    mqttClient.disconnect();
    wifiClient.available = false;
    const auto before = wifiClient.connects;
    hostMillis += MQTT_RETRY_MS;
    loop();  // a failed TCP attempt; loop() does not spin waiting for a broker
    assert(wifiClient.connects == before + 1);
    const auto retry = mqttRetryMs;
    for (unsigned i = 0; i < 100; ++i) loop();
    assert(wifiClient.connects == before + 1);
    for (unsigned i = 0; i < QUEUE_SIZE_MAX + 2; ++i) {
        duck.receive(packet(std::to_string(i)));
    }
    assert(pending.size() == QUEUE_SIZE_MAX);
    wifiClient.available = true;
    hostMillis += retry;
    mqttClient.published.clear();
    loop();  // connect and send only the first retained message
    assert(mqttClient.published.size() == 1);
    assert(parse(mqttClient.published.front().json)["Payload"] == "2");
    assert(pending.size() == QUEUE_SIZE_MAX - 1);
    assert(wifiClient.timeout == MQTT_TIMEOUT_S * 1000);
    // Publish failure retains the packet and defers the next connection.
    mqttClient.publishOK = false;
    loop();
    assert(pending.size() == QUEUE_SIZE_MAX - 1 && !mqttClient.connected());
    const auto attempts = wifiClient.connects;
    loop(); assert(wifiClient.connects == attempts);
    mqttClient.publishOK = true;
    hostMillis += MQTT_RETRY_MS;
    while (!pending.empty()) loop();
    // MQTT refusal also backs off; retry delay stays bounded through rollover.
    mqttClient.disconnect();
    mqttClient.brokerAvailable = false;
    for (unsigned i = 0; i < 12; ++i) {
        hostMillis += mqttRetryMs;
        loop();
        assert(mqttRetryMs <= MQTT_RETRY_MAX_MS);
    }
    hostMillis = UINT32_MAX - 100;
    lastMqttAttempt = hostMillis;
    hostMillis += mqttRetryMs;
    mqttClient.brokerAvailable = true;
    loop(); assert(mqttClient.connected());
    // An allocation failure leaves setup disabled.
    setupOK = false;
    mqttClient.bufferOK = false;
    setup(); assert(!setupOK);
#else
    // DMS writes commands without a newline. Consume only 64 bytes per loop,
    // warn once, and keep running the mesh even with a continuous input stream.
    Serial.output.clear();
    Serial.input.assign(200, 'x');
    const auto before = duck.runs;
    loop(); assert(Serial.input.size() == 136 && duck.runs == before + 1);
    const auto warning = Serial.output;
    assert(warning.find("unsupported") != std::string::npos);
    loop(); assert(Serial.output == warning);
#endif
    setupOK = false;
    setupResult = DUCK_ERR_EEPROM_INIT;
    setup(); assert(!setupOK);
    const auto beforeFailedLoop = duck.runs;
    loop(); assert(duck.runs == beforeFailedLoop);
    std::cerr << "PASS: DMS Lite sketch and transport checks\n";
}

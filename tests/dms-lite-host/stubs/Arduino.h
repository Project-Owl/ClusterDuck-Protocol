#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>
#include <vector>
#include <unordered_map>
using byte = uint8_t;
using boolean = bool;
inline uint32_t hostMillis = 0;
inline uint32_t hostRandom = 7;
inline unsigned long millis() { return hostMillis; }
inline unsigned long micros() { return hostMillis * 1000UL; }
inline uint32_t esp_random() { return ++hostRandom; }
inline long random(long n) { return esp_random() % n; }
inline void delay(unsigned long ms) { hostMillis += ms; }
struct HostSerial {
    int baud = 0;
    std::string output;
    std::deque<uint8_t> input;
    void begin(int value) { baud = value; }
    template<class... T> int printf(const char* format, T... args) {
        char buffer[512];
        const int size = std::snprintf(buffer, sizeof(buffer), format, args...);
        output += buffer;
        return size;
    }
    void println(const char* text) { output += text; output += '\n'; }
    void println() { output += '\n'; }
    int available() { return input.size(); }
    int read() { const int value = input.front(); input.pop_front(); return value; }
};
inline HostSerial Serial;

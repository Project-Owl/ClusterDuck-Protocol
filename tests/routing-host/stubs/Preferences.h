#pragma once
#include <cstddef>
#include <string>
struct Preferences {
    bool begin(const char*) { return true; }
    bool isKey(const char*) { return true; }
    int getInt(const char*, int value) { return value; }
    float getFloat(const char*, float value) { return value; }
    size_t putInt(const char*, int) { return sizeof(int); }
    size_t putFloat(const char*, float) { return sizeof(float); }
};

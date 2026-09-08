// Minimal host stand-in for Arduino.h, enough to compile lib/Settings.
#pragma once

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <string>

#define constrain(amt, low, high) ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
#define abs(x) ((x) > 0 ? (x) : -(x))

extern unsigned long fakeMillis;
extern uint32_t fakeAdcMillivolts;
extern int fakePinModeCalls;

#define INPUT 0x01
#define ADC_11db 3

inline unsigned long millis()
{
    return fakeMillis;
}

inline void pinMode(uint8_t pin, uint8_t mode)
{
    (void)pin;
    (void)mode;
    fakePinModeCalls++;
}

inline void analogSetPinAttenuation(uint8_t pin, int attenuation)
{
    (void)pin;
    (void)attenuation;
}

inline uint32_t analogReadMilliVolts(uint8_t pin)
{
    (void)pin;
    return fakeAdcMillivolts;
}

inline bool isAlphaNumeric(char value)
{
    return std::isalnum((unsigned char)value) != 0;
}

class String
{
public:

    String() {}
    String(const char* text) : value(text ? text : "") {}
    String(const std::string& text) : value(text) {}

    size_t length() const { return value.size(); }
    char charAt(size_t index) const { return index < value.size() ? value[index] : '\0'; }
    const char* c_str() const { return value.c_str(); }
    void reserve(size_t size) { value.reserve(size); }

    void toCharArray(char* buffer, size_t size) const
    {
        if(size == 0) return;
        std::snprintf(buffer, size, "%s", value.c_str());
    }

    void trim()
    {
        size_t start = 0;
        while(start < value.size() && std::isspace((unsigned char)value[start])) start++;
        size_t end = value.size();
        while(end > start && std::isspace((unsigned char)value[end - 1])) end--;
        value = value.substr(start, end - start);
    }

    bool equalsIgnoreCase(const String& other) const
    {
        if(value.size() != other.value.size()) return false;
        for(size_t i = 0; i < value.size(); i++)
        {
            if(std::tolower((unsigned char)value[i]) != std::tolower((unsigned char)other.value[i])) return false;
        }
        return true;
    }

    bool equals(const String& other) const { return value == other.value; }
    bool operator==(const String& other) const { return value == other.value; }
    bool operator!=(const String& other) const { return value != other.value; }
    String& operator+=(char c) { value += c; return *this; }
    String& operator+=(const char* text) { value += text; return *this; }
    String& operator+=(const String& other) { value += other.value; return *this; }

    std::string value;
};

// Host stand-in for the ESP32 Preferences (NVS) API backed by a map, so the
// real Settings code can be exercised against synthetic stored blobs.
#pragma once

#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

typedef std::map<std::string, std::vector<uint8_t>> FakePreferencesNamespace;

FakePreferencesNamespace& fakePreferencesStore(const char* name);

class Preferences
{
public:

    bool begin(const char* name, bool readOnly = false)
    {
        (void)readOnly;
        store = &fakePreferencesStore(name);
        return true;
    }

    bool isKey(const char* key) { return store->count(key) > 0; }
    bool remove(const char* key) { return store->erase(key) > 0; }

    size_t putBytes(const char* key, const void* data, size_t length)
    {
        std::vector<uint8_t> bytes((const uint8_t*)data, (const uint8_t*)data + length);
        (*store)[key] = bytes;
        return length;
    }

    size_t getBytesLength(const char* key)
    {
        return isKey(key) ? (*store)[key].size() : 0;
    }

    size_t getBytes(const char* key, void* buffer, size_t maxLength)
    {
        if(!isKey(key)) return 0;
        const std::vector<uint8_t>& bytes = (*store)[key];
        if(bytes.size() > maxLength) return 0;
        std::memcpy(buffer, bytes.data(), bytes.size());
        return bytes.size();
    }

    size_t putBool(const char* key, bool value) { return putTyped(key, (uint8_t)(value ? 1 : 0)); }
    size_t putChar(const char* key, int8_t value) { return putTyped(key, value); }
    size_t putUChar(const char* key, uint8_t value) { return putTyped(key, value); }
    size_t putUShort(const char* key, uint16_t value) { return putTyped(key, value); }
    size_t putInt(const char* key, int32_t value) { return putTyped(key, value); }
    size_t putULong(const char* key, uint32_t value) { return putTyped(key, value); }
    size_t putFloat(const char* key, float value) { return putTyped(key, value); }

    bool getBool(const char* key, bool fallback = false) { return getTyped<uint8_t>(key, fallback ? 1 : 0) != 0; }
    int8_t getChar(const char* key, int8_t fallback = 0) { return getTyped<int8_t>(key, fallback); }
    uint8_t getUChar(const char* key, uint8_t fallback = 0) { return getTyped<uint8_t>(key, fallback); }
    uint16_t getUShort(const char* key, uint16_t fallback = 0) { return getTyped<uint16_t>(key, fallback); }
    int32_t getInt(const char* key, int32_t fallback = 0) { return getTyped<int32_t>(key, fallback); }
    uint32_t getULong(const char* key, uint32_t fallback = 0) { return getTyped<uint32_t>(key, fallback); }
    float getFloat(const char* key, float fallback = 0.0f) { return getTyped<float>(key, fallback); }

private:

    FakePreferencesNamespace* store = nullptr;

    template<typename T>
    size_t putTyped(const char* key, T value)
    {
        return putBytes(key, &value, sizeof(T));
    }

    template<typename T>
    T getTyped(const char* key, T fallback)
    {
        if(!isKey(key)) return fallback;
        const std::vector<uint8_t>& bytes = (*store)[key];
        if(bytes.size() != sizeof(T)) return fallback;
        T value;
        std::memcpy(&value, bytes.data(), sizeof(T));
        return value;
    }
};

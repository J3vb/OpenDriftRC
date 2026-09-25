#pragma once

#include <Arduino.h>
#include <FS.h>
#include <FFat.h>

// Safe storage for browser-converted AMOLED backgrounds. Images are raw
// little-endian RGB565 so the control board never decodes JPG/PNG data.
class Backgrounds
{
public:
    static constexpr int WIDTH = 456;
    static constexpr int HEIGHT = 280;
    static constexpr size_t PIXEL_BYTES = (size_t)WIDTH * HEIGHT * 2;
    static constexpr uint8_t MAX_BACKGROUNDS = 16;
    static constexpr size_t NAME_LENGTH = 24;

    bool begin();
    bool isReady() const;
    bool wasFormatted() const;
    uint8_t refresh();
    uint8_t getCount() const;
    const char* getName(uint8_t index) const;
    bool exists(const char* name) const;
    uint32_t getRevision() const;
    bool load(const char* name, uint16_t* pixels);
    bool remove(const char* name);

    bool beginUpload(const char* name);
    bool writeUpload(const uint8_t* data, size_t length);
    bool endUpload();
    void abortUpload();
    const char* getUploadError() const;

    size_t getFreeBytes() const;
    size_t getTotalBytes() const;
    static String sanitizeName(const String& value);

private:
    static constexpr const char* DIRECTORY = "/bg";
    static constexpr const char* EXTENSION = ".rgb";
    static constexpr const char* BACKUP_EXTENSION = ".bak";
    static constexpr const char* UPLOAD_PATH = "/bg/upload.tmp";

    bool ready = false;
    bool formatted = false;
    char names[MAX_BACKGROUNDS][NAME_LENGTH] = {};
    uint8_t count = 0;
    uint32_t revision = 0;
    File uploadFile;
    size_t uploadBytes = 0;
    char uploadName[NAME_LENGTH] = {0};
    const char* uploadError = "";

    String pathFor(const char* name) const;
    String backupPathFor(const char* name) const;
    void recoverInterruptedReplacements();
};

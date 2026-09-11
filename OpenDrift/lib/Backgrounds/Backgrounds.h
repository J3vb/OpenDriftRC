#pragma once

#include <Arduino.h>
#include <FS.h>
#include <FFat.h>


// User-uploaded AMOLED backgrounds. Each one is a raw 456 x 280 RGB565
// image, 255,360 bytes, stored as /bg/<name>.rgb on the ffat partition
// that the firmware otherwise leaves unused. The web configurator writes
// them, the UI loads the selected one into PSRAM at boot and whenever the
// selection changes.
class Backgrounds
{
public:

    static constexpr int WIDTH = 456;
    static constexpr int HEIGHT = 280;
    static constexpr size_t PIXEL_BYTES = (size_t)WIDTH * HEIGHT * 2;
    static constexpr uint8_t MAX_BACKGROUNDS = 16;
    static constexpr size_t NAME_LENGTH = 24;   // 23 characters + NUL

    // Mounts the partition and formats it when it has never been used.
    // Call before the control task starts: the one-time format takes a
    // few seconds.
    bool begin();

    bool isReady();

    // True when begin() had to format the partition.
    bool wasFormatted();

    // Rescans the directory and returns the number of stored images.
    uint8_t refresh();

    uint8_t getCount();

    // Name at a list position, or "" when out of range.
    const char* getName(
        uint8_t index
    );

    bool exists(
        const char* name
    );

    // Increments on every upload or delete so the UI knows to reload.
    uint32_t getRevision();

    // Copies one image into pixels, which must hold PIXEL_BYTES bytes.
    bool load(
        const char* name,
        uint16_t* pixels
    );

    bool remove(
        const char* name
    );

    // Deletes every file in the background directory, including leftovers
    // of interrupted uploads. Used by the factory reset.
    void eraseAll();

    // Streaming upload of one raw image. The data must total PIXEL_BYTES.
    // An existing image with the same name is replaced on success.
    bool beginUpload(
        const char* name
    );

    bool writeUpload(
        const uint8_t* data,
        size_t length
    );

    bool endUpload();

    void abortUpload();

    // Human-readable reason for the last refused or failed upload.
    const char* getUploadError();

    size_t getFreeBytes();

    size_t getTotalBytes();

    // Letters, digits, - and _ only, at most 23 characters. Keeps names
    // safe as FAT filenames and inside the web page without escaping.
    static String sanitizeName(
        const String& value
    );


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

    String pathFor(
        const char* name
    );

    String backupPathFor(
        const char* name
    );

    // Finishes or undoes a replacement that lost power halfway.
    void recoverInterruptedReplacements();
};

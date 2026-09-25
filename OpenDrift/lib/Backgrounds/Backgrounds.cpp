#include "Backgrounds.h"

#include <stdlib.h>
#include <string.h>

namespace
{
int compareBackgroundNames(const void* left, const void* right)
{
    return strcasecmp(
        static_cast<const char*>(left),
        static_cast<const char*>(right)
    );
}
}

bool Backgrounds::begin()
{
    ready = FFat.begin(false);

    if(!ready)
    {
        formatted = true;
        ready = FFat.begin(true);
    }

    if(!ready)
    {
        return false;
    }

    if(!FFat.exists(DIRECTORY))
    {
        FFat.mkdir(DIRECTORY);
    }

    if(FFat.exists(UPLOAD_PATH))
    {
        FFat.remove(UPLOAD_PATH);
    }

    recoverInterruptedReplacements();
    refresh();
    return true;
}

bool Backgrounds::isReady() const { return ready; }
bool Backgrounds::wasFormatted() const { return formatted; }
uint8_t Backgrounds::getCount() const { return count; }
uint32_t Backgrounds::getRevision() const { return revision; }
size_t Backgrounds::getFreeBytes() const { return ready ? FFat.freeBytes() : 0; }
size_t Backgrounds::getTotalBytes() const { return ready ? FFat.totalBytes() : 0; }
const char* Backgrounds::getUploadError() const { return uploadError; }

uint8_t Backgrounds::refresh()
{
    count = 0;

    if(!ready)
    {
        return 0;
    }

    File directory = FFat.open(DIRECTORY);
    if(!directory || !directory.isDirectory())
    {
        return 0;
    }

    const size_t extensionLength = strlen(EXTENSION);

    while(count < MAX_BACKGROUNDS)
    {
        File entry = directory.openNextFile();
        if(!entry) break;

        if(!entry.isDirectory() && entry.size() == PIXEL_BYTES)
        {
            const char* fileName = strrchr(entry.name(), '/');
            fileName = fileName == nullptr ? entry.name() : fileName + 1;
            const size_t length = strlen(fileName);

            if(
                length > extensionLength &&
                length - extensionLength < NAME_LENGTH &&
                strcasecmp(fileName + length - extensionLength, EXTENSION) == 0
            )
            {
                const size_t baseLength = length - extensionLength;
                memcpy(names[count], fileName, baseLength);
                names[count][baseLength] = 0;
                count++;
            }
        }

        entry.close();
    }

    directory.close();
    qsort(names, count, NAME_LENGTH, compareBackgroundNames);
    return count;
}

const char* Backgrounds::getName(uint8_t index) const
{
    return index < count ? names[index] : "";
}

bool Backgrounds::exists(const char* name) const
{
    return ready && name != nullptr && name[0] != 0 && FFat.exists(pathFor(name));
}

bool Backgrounds::load(const char* name, uint16_t* pixels)
{
    if(!exists(name) || pixels == nullptr) return false;

    File file = FFat.open(pathFor(name), FILE_READ);
    if(!file || file.isDirectory() || file.size() != PIXEL_BYTES)
    {
        if(file) file.close();
        return false;
    }

    uint8_t* target = reinterpret_cast<uint8_t*>(pixels);
    size_t offset = 0;

    while(offset < PIXEL_BYTES)
    {
        const size_t amount = file.read(target + offset, PIXEL_BYTES - offset);
        if(amount == 0) break;
        offset += amount;
    }

    file.close();
    return offset == PIXEL_BYTES;
}

bool Backgrounds::remove(const char* name)
{
    if(!exists(name)) return false;
    const bool removed = FFat.remove(pathFor(name));
    revision++;
    refresh();
    return removed;
}

bool Backgrounds::beginUpload(const char* name)
{
    abortUpload();
    uploadError = "";

    if(!ready)
    {
        uploadError = "Background storage is unavailable";
        return false;
    }

    const String clean = sanitizeName(String(name));
    if(clean.length() == 0)
    {
        uploadError = "Use letters, numbers, - or _ for the name";
        return false;
    }

    if(!exists(clean.c_str()) && count >= MAX_BACKGROUNDS)
    {
        uploadError = "The 16-background limit has been reached";
        return false;
    }

    if(FFat.freeBytes() < PIXEL_BYTES + 8192)
    {
        uploadError = "Not enough free storage";
        return false;
    }

    snprintf(uploadName, sizeof(uploadName), "%s", clean.c_str());
    uploadFile = FFat.open(UPLOAD_PATH, FILE_WRITE);

    if(!uploadFile)
    {
        uploadError = "Could not create the temporary upload";
        return false;
    }

    uploadBytes = 0;
    return true;
}

bool Backgrounds::writeUpload(const uint8_t* data, size_t length)
{
    if(!uploadFile) return false;

    if(uploadBytes + length > PIXEL_BYTES)
    {
        uploadError = "Image data is larger than 456 x 280 RGB565";
        abortUpload();
        return false;
    }

    const size_t written = uploadFile.write(data, length);
    if(written != length)
    {
        uploadError = "Writing background storage failed";
        abortUpload();
        return false;
    }

    uploadBytes += written;
    return true;
}

bool Backgrounds::endUpload()
{
    if(!uploadFile) return false;
    uploadFile.close();

    if(uploadBytes != PIXEL_BYTES)
    {
        uploadError = "Converted image must be exactly 456 x 280 RGB565";
        FFat.remove(UPLOAD_PATH);
        return false;
    }

    const String target = pathFor(uploadName);
    const String backup = backupPathFor(uploadName);
    const bool replacing = FFat.exists(target);

    if(replacing)
    {
        if(FFat.exists(backup)) FFat.remove(backup);
        if(!FFat.rename(target, backup))
        {
            uploadError = "Could not safely replace the existing background";
            FFat.remove(UPLOAD_PATH);
            return false;
        }
    }

    if(!FFat.rename(String(UPLOAD_PATH), target))
    {
        if(replacing) FFat.rename(backup, target);
        uploadError = "Installing the uploaded background failed";
        FFat.remove(UPLOAD_PATH);
        return false;
    }

    if(replacing) FFat.remove(backup);
    revision++;
    refresh();
    return true;
}

void Backgrounds::abortUpload()
{
    if(uploadFile) uploadFile.close();
    if(ready && FFat.exists(UPLOAD_PATH)) FFat.remove(UPLOAD_PATH);
    uploadBytes = 0;
}

String Backgrounds::sanitizeName(const String& value)
{
    String input = value;
    input.trim();
    String clean;
    clean.reserve(NAME_LENGTH - 1);

    for(size_t i = 0; i < input.length() && clean.length() < NAME_LENGTH - 1; i++)
    {
        const char character = input.charAt(i);
        if(isAlphaNumeric(character) || character == '-' || character == '_')
        {
            clean += character;
        }
    }

    return clean;
}

String Backgrounds::pathFor(const char* name) const
{
    return String(DIRECTORY) + "/" + name + EXTENSION;
}

String Backgrounds::backupPathFor(const char* name) const
{
    return String(DIRECTORY) + "/" + name + BACKUP_EXTENSION;
}

void Backgrounds::recoverInterruptedReplacements()
{
    File directory = FFat.open(DIRECTORY);
    if(!directory || !directory.isDirectory()) return;

    char pending[MAX_BACKGROUNDS][NAME_LENGTH] = {};
    uint8_t pendingCount = 0;
    const size_t extensionLength = strlen(BACKUP_EXTENSION);

    while(pendingCount < MAX_BACKGROUNDS)
    {
        File entry = directory.openNextFile();
        if(!entry) break;

        if(!entry.isDirectory())
        {
            const char* fileName = strrchr(entry.name(), '/');
            fileName = fileName == nullptr ? entry.name() : fileName + 1;
            const size_t length = strlen(fileName);

            if(
                length > extensionLength &&
                length - extensionLength < NAME_LENGTH &&
                strcasecmp(fileName + length - extensionLength, BACKUP_EXTENSION) == 0
            )
            {
                const size_t baseLength = length - extensionLength;
                memcpy(pending[pendingCount], fileName, baseLength);
                pending[pendingCount][baseLength] = 0;
                pendingCount++;
            }
        }

        entry.close();
    }

    directory.close();

    for(uint8_t i = 0; i < pendingCount; i++)
    {
        const String backup = backupPathFor(pending[i]);
        const String image = pathFor(pending[i]);
        if(FFat.exists(image)) FFat.remove(backup);
        else FFat.rename(backup, image);
    }
}

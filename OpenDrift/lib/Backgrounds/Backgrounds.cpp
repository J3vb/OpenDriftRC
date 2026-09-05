#include "Backgrounds.h"

#include <string.h>
#include <stdlib.h>


namespace
{
    int compareNames(
        const void* a,
        const void* b
    )
    {
        return strcasecmp(
            static_cast<const char*>(a),
            static_cast<const char*>(b)
        );
    }
}


bool Backgrounds::begin()
{
    // Try a plain mount first so a first-boot format can be reported.
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

    // A power loss mid-upload leaves the temporary file behind.
    if(FFat.exists(UPLOAD_PATH))
    {
        FFat.remove(UPLOAD_PATH);
    }

    refresh();

    return true;
}



bool Backgrounds::isReady()
{
    return ready;
}



bool Backgrounds::wasFormatted()
{
    return formatted;
}



uint8_t Backgrounds::refresh()
{
    count = 0;

    if(!ready)
    {
        return 0;
    }

    File directory =
        FFat.open(DIRECTORY);

    if(!directory || !directory.isDirectory())
    {
        return 0;
    }

    size_t extensionLength =
        strlen(EXTENSION);

    while(count < MAX_BACKGROUNDS)
    {
        File entry =
            directory.openNextFile();

        if(!entry)
        {
            break;
        }

        if(entry.isDirectory())
        {
            entry.close();
            continue;
        }

        // Only complete images of the right size are listed; anything
        // else in the directory is ignored rather than trusted.
        if(entry.size() != PIXEL_BYTES)
        {
            entry.close();
            continue;
        }

        const char* fileName =
            entry.name();

        const char* slash =
            strrchr(fileName, '/');

        if(slash != nullptr)
        {
            fileName = slash + 1;
        }

        size_t length =
            strlen(fileName);

        if(
            length <= extensionLength ||
            length - extensionLength >= NAME_LENGTH ||
            strcasecmp(fileName + length - extensionLength, EXTENSION) != 0
        )
        {
            entry.close();
            continue;
        }

        size_t baseLength =
            length - extensionLength;

        memcpy(
            names[count],
            fileName,
            baseLength
        );

        names[count][baseLength] = 0;

        count++;

        entry.close();
    }

    directory.close();

    qsort(
        names,
        count,
        NAME_LENGTH,
        compareNames
    );

    return count;
}



uint8_t Backgrounds::getCount()
{
    return count;
}



const char* Backgrounds::getName(
    uint8_t index
)
{
    if(index >= count)
    {
        return "";
    }

    return names[index];
}



bool Backgrounds::exists(
    const char* name
)
{
    if(!ready || name == nullptr || name[0] == 0)
    {
        return false;
    }

    return FFat.exists(pathFor(name));
}



uint32_t Backgrounds::getRevision()
{
    return revision;
}



bool Backgrounds::load(
    const char* name,
    uint16_t* pixels
)
{
    if(!ready || pixels == nullptr || name == nullptr || name[0] == 0)
    {
        return false;
    }

    File file =
        FFat.open(
            pathFor(name),
            FILE_READ
        );

    if(!file || file.isDirectory() || file.size() != PIXEL_BYTES)
    {
        if(file)
        {
            file.close();
        }

        return false;
    }

    uint8_t* target =
        reinterpret_cast<uint8_t*>(pixels);

    size_t offset = 0;

    while(offset < PIXEL_BYTES)
    {
        size_t read =
            file.read(
                target + offset,
                PIXEL_BYTES - offset
            );

        if(read == 0)
        {
            break;
        }

        offset += read;
    }

    file.close();

    return offset == PIXEL_BYTES;
}



bool Backgrounds::remove(
    const char* name
)
{
    if(!exists(name))
    {
        return false;
    }

    bool removed =
        FFat.remove(pathFor(name));

    revision++;

    refresh();

    return removed;
}



bool Backgrounds::beginUpload(
    const char* name
)
{
    abortUpload();

    uploadError = "";

    if(!ready)
    {
        uploadError = "Background storage is not available";
        return false;
    }

    String clean =
        sanitizeName(String(name));

    if(clean.length() == 0)
    {
        uploadError = "Name must use letters, digits, - or _";
        return false;
    }

    bool replacing =
        exists(clean.c_str());

    if(!replacing && count >= MAX_BACKGROUNDS)
    {
        uploadError = "Limit of 16 backgrounds reached; delete one first";
        return false;
    }

    // Keep one spare cluster's worth of room for the directory entry.
    if(!replacing && FFat.freeBytes() < PIXEL_BYTES + 8192)
    {
        uploadError = "Not enough free space; delete a background first";
        return false;
    }

    snprintf(
        uploadName,
        sizeof(uploadName),
        "%s",
        clean.c_str()
    );

    uploadFile =
        FFat.open(
            UPLOAD_PATH,
            FILE_WRITE
        );

    if(!uploadFile)
    {
        uploadError = "Could not create the upload file";
        return false;
    }

    uploadBytes = 0;

    return true;
}



bool Backgrounds::writeUpload(
    const uint8_t* data,
    size_t length
)
{
    if(!uploadFile)
    {
        return false;
    }

    if(uploadBytes + length > PIXEL_BYTES)
    {
        uploadError = "Image is larger than 456 x 280 RGB565";
        abortUpload();
        return false;
    }

    size_t written =
        uploadFile.write(
            data,
            length
        );

    if(written != length)
    {
        uploadError = "Writing to background storage failed";
        abortUpload();
        return false;
    }

    uploadBytes += written;

    return true;
}



bool Backgrounds::endUpload()
{
    if(!uploadFile)
    {
        return false;
    }

    uploadFile.close();

    if(uploadBytes != PIXEL_BYTES)
    {
        uploadError = "Image must be exactly 456 x 280 RGB565";
        FFat.remove(UPLOAD_PATH);
        return false;
    }

    String path =
        pathFor(uploadName);

    if(FFat.exists(path))
    {
        FFat.remove(path);
    }

    if(!FFat.rename(String(UPLOAD_PATH), path))
    {
        uploadError = "Storing the background failed";
        FFat.remove(UPLOAD_PATH);
        return false;
    }

    revision++;

    refresh();

    return true;
}



void Backgrounds::abortUpload()
{
    if(uploadFile)
    {
        uploadFile.close();
    }

    if(ready && FFat.exists(UPLOAD_PATH))
    {
        FFat.remove(UPLOAD_PATH);
    }

    uploadBytes = 0;
}



const char* Backgrounds::getUploadError()
{
    return uploadError;
}



size_t Backgrounds::getFreeBytes()
{
    return ready ? FFat.freeBytes() : 0;
}



size_t Backgrounds::getTotalBytes()
{
    return ready ? FFat.totalBytes() : 0;
}



String Backgrounds::sanitizeName(
    const String& value
)
{
    String name = value;
    name.trim();

    String clean;
    clean.reserve(NAME_LENGTH - 1);

    for(
        size_t i = 0;
        i < name.length() &&
        clean.length() < NAME_LENGTH - 1;
        i++
    )
    {
        char character = name.charAt(i);

        if(
            isAlphaNumeric(character) ||
            character == '-' ||
            character == '_'
        )
        {
            clean += character;
        }
    }

    return clean;
}



String Backgrounds::pathFor(
    const char* name
)
{
    String path = DIRECTORY;
    path += '/';
    path += name;
    path += EXTENSION;

    return path;
}

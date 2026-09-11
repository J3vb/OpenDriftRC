// Host stand-in for the ESP-IDF heap capability allocator.
#pragma once

#include <cstdint>
#include <cstdlib>

#define MALLOC_CAP_SPIRAM 0x400
#define MALLOC_CAP_8BIT 0x004

inline void* heap_caps_malloc(size_t size, uint32_t caps)
{
    (void)caps;
    return std::malloc(size);
}

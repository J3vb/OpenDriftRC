#pragma once

// Single source of truth for the firmware version.
// Update OPENDRIFT_VERSION on release; every interface reads it from here.

#define OPENDRIFT_VERSION "1.0.8"

#if defined(OPENDRIFT_BOARD_AMOLED_164)
    #if defined(OPENDRIFT_AMOLED_V2)
        #define OPENDRIFT_BOARD_NAME "V2"
    #else
        #define OPENDRIFT_BOARD_NAME "V1"
    #endif
#else
    #define OPENDRIFT_BOARD_NAME "ROUND"
#endif

#if defined(OPENDRIFT_CRSF_OOPS_SWAPPED_PINS)
    #define OPENDRIFT_INPUT_NAME "OOPS"
#elif defined(OPENDRIFT_ROUND_LOG51_TUNE)
    #define OPENDRIFT_INPUT_NAME "LOG51"
#elif defined(OPENDRIFT_INPUT_CRSF)
    #define OPENDRIFT_INPUT_NAME "CRSF"
#else
    #define OPENDRIFT_INPUT_NAME "PWM"
#endif

#define OPENDRIFT_BUILD_NAME OPENDRIFT_BOARD_NAME " " OPENDRIFT_INPUT_NAME

#define OPENDRIFT_VERSION_STRING "v" OPENDRIFT_VERSION " " OPENDRIFT_BUILD_NAME

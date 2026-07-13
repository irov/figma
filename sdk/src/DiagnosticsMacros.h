#pragma once

#include "Figma/Types.h"

#if FIGMA_ENABLE_DIAGNOSTICS
#   define FIGMA_DIAGNOSTICS_ADD(_diagnostics, ...) (_diagnostics).add(__VA_ARGS__)
#   define FIGMA_DIAGNOSTICS_ADD_POINTER(_diagnostics, ...) (_diagnostics)->add(__VA_ARGS__)
#else
#   define FIGMA_DIAGNOSTICS_ADD(_diagnostics, ...) do { if(false) { (_diagnostics).add(__VA_ARGS__); } } while(false)
#   define FIGMA_DIAGNOSTICS_ADD_POINTER(_diagnostics, ...) do { if(false) { (_diagnostics)->add(__VA_ARGS__); } } while(false)
#endif

#include "figma/figma.h"

_Static_assert(
    sizeof(figma_result_t) == sizeof(int32_t),
    "figma_result_t must be fixed-width");
_Static_assert(
    sizeof(figma_render_batch_type_t) == sizeof(int32_t),
    "figma_render_batch_type_t must be fixed-width");
_Static_assert(
    sizeof(figma_bool_t) == sizeof(uint8_t),
    "figma_bool_t must be fixed-width");

//////////////////////////////////////////////////////////////////////////
uint32_t figma_c11_header_probe(void)
{
    return FIGMA_SDK_VERSION;
}

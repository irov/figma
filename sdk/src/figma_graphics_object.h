#pragma once

/*
 * Creates the irov/graphics object a host has to own and pass through
 * figma_runtime_desc_t. Used by the tools and tests of this repository; a host
 * with its own graphics object does not need it.
 */

#if defined(__cplusplus)
#include "graphics/graphics.hpp"
#else
#include "graphics/graphics.h"
#endif

#include <stdlib.h>

static void * figma_graphics_object_malloc(gp_size_t size, void * user_data)
{
    (void)user_data;

    return malloc((size_t)size);
}

static void * figma_graphics_object_realloc(void * ptr, gp_size_t size, void * user_data)
{
    (void)user_data;

    return realloc(ptr, (size_t)size);
}

static void figma_graphics_object_free(void * ptr, void * user_data)
{
    (void)user_data;

    free(ptr);
}

static gp_graphics_t * figma_graphics_object_create(void)
{
    gp_graphics_t * graphics = NULL;

    if(gp_graphics_create(
           &graphics,
           &figma_graphics_object_malloc,
           &figma_graphics_object_realloc,
           &figma_graphics_object_free,
           NULL,
           GP_CAPACITY_DEFAULT,
           GP_CAPACITY_DEFAULT,
           GP_CAPACITY_DEFAULT) == GP_FAILURE)
    {
        return NULL;
    }

    return graphics;
}

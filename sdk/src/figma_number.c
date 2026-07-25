#include "figma_internal.h"

#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_double_equal_bits(double left, double right)
{
    uint64_t left_bits;
    uint64_t right_bits;
    memcpy(&left_bits, &left, sizeof(left_bits));
    memcpy(&right_bits, &right, sizeof(right_bits));
    return left_bits == right_bits ? FIGMA_TRUE : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_normalize_decimal_point(char * buffer, size_t capacity, size_t * length)
{
    const struct lconv * locale = localeconv();
    const char * decimal_point =
        locale != NULL && locale->decimal_point != NULL
        ? locale->decimal_point
        : ".";
    const size_t decimal_size = strlen(decimal_point);
    char * position;

    if(decimal_size == 0u || strcmp(decimal_point, ".") == 0)
    {
        return FIGMA_TRUE;
    }

    position = strstr(buffer, decimal_point);
    if(position == NULL)
    {
        return FIGMA_TRUE;
    }

    if(decimal_size == 1u)
    {
        *position = '.';
        return FIGMA_TRUE;
    }

    if(*length < decimal_size - 1u)
    {
        return FIGMA_FALSE;
    }
    memmove(
        position + 1u,
        position + decimal_size,
        *length - (size_t)(position - buffer) - decimal_size + 1u);
    *position = '.';
    *length -= decimal_size - 1u;
    return *length < capacity ? FIGMA_TRUE : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_format_double_shortest(double value, char * buffer, size_t capacity, size_t * length)
{
    char candidate[64];
    size_t candidate_length = 0u;
    int precision;

    if(buffer == NULL || capacity == 0u || length == NULL)
    {
        return FIGMA_FALSE;
    }

    for(precision = 1; precision <= 17; ++precision)
    {
        char * parse_end;
        double parsed;
        const int written = snprintf(
            candidate, sizeof(candidate), "%.*g", precision, value);
        if(written < 0 || (size_t)written >= sizeof(candidate))
        {
            return FIGMA_FALSE;
        }

        candidate_length = (size_t)written;
        if(isfinite(value) == 0)
        {
            break;
        }

        parse_end = NULL;
        parsed = strtod(candidate, &parse_end);
        if(parse_end != candidate + candidate_length)
        {
            return FIGMA_FALSE;
        }
        if(__figma_double_equal_bits(value, parsed) == FIGMA_TRUE)
        {
            break;
        }
    }

    if(__figma_normalize_decimal_point(
           candidate, sizeof(candidate), &candidate_length) == FIGMA_FALSE ||
        candidate_length + 1u > capacity)
    {
        return FIGMA_FALSE;
    }

    memcpy(buffer, candidate, candidate_length + 1u);
    *length = candidate_length;
    return FIGMA_TRUE;
}

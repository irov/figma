#pragma once

#include "figma_inspection.h"

#include <stdint.h>

typedef struct figma_memory
{
    figma_allocator_desc_t allocator;
} figma_memory_t;

typedef void (*figma_element_destroy_fn)(figma_memory_t * memory, void * element);

typedef struct figma_array
{
    void * data;
    size_t size;
    size_t capacity;
    size_t element_size;
} figma_array_t;

typedef struct figma_string
{
    char * data;
    size_t size;
    size_t capacity;
} figma_string_t;

typedef struct figma_diagnostic_owned
{
    figma_diagnostic_severity_t severity;
    figma_string_t code;
    figma_string_t message;
    figma_string_t node_id;
} figma_diagnostic_owned_t;

struct figma_diagnostics
{
    figma_memory_t * memory;
    figma_array_t items;
};

struct figma_runtime
{
    figma_runtime_desc_t desc;
    figma_memory_t memory;
};

figma_bool_t figma_size_add(size_t left, size_t right, size_t * result);
figma_bool_t figma_size_mul(size_t left, size_t right, size_t * result);

void * figma_memory_allocate(figma_memory_t * memory, size_t size);
void * figma_memory_reallocate(figma_memory_t * memory, void * ptr, size_t size);
void figma_memory_deallocate(figma_memory_t * memory, void * ptr);

void figma_array_init(figma_array_t * array, size_t element_size);
void figma_array_destroy(figma_memory_t * memory, figma_array_t * array, figma_element_destroy_fn destroy);
void figma_array_clear(figma_memory_t * memory, figma_array_t * array, figma_element_destroy_fn destroy);
figma_bool_t figma_array_reserve(figma_memory_t * memory, figma_array_t * array, size_t capacity);
figma_bool_t figma_array_resize(figma_memory_t * memory, figma_array_t * array, size_t size, figma_element_destroy_fn destroy);
void * figma_array_push_uninitialized(figma_memory_t * memory, figma_array_t * array);
figma_bool_t figma_array_push_copy(figma_memory_t * memory, figma_array_t * array, const void * value);
void figma_array_pop(figma_memory_t * memory, figma_array_t * array, figma_element_destroy_fn destroy);
void figma_array_remove(figma_memory_t * memory, figma_array_t * array, size_t index, figma_element_destroy_fn destroy);

#define FIGMA_ARRAY_AT(type, array, index) (((type *)(array)->data)[(index)])
#define FIGMA_ARRAY_PTR(type, array, index) (&((type *)(array)->data)[(index)])
#define FIGMA_ARRAY_CONST_PTR(type, array, index) (&((const type *)(array)->data)[(index)])

void figma_string_init(figma_string_t * string);
void figma_string_destroy(figma_memory_t * memory, figma_string_t * string);
void figma_string_clear(figma_string_t * string);
figma_bool_t figma_string_reserve(figma_memory_t * memory, figma_string_t * string, size_t capacity);
figma_bool_t figma_string_assign(figma_memory_t * memory, figma_string_t * string, figma_string_view_t value);
figma_bool_t figma_string_assign_cstr(figma_memory_t * memory, figma_string_t * string, const char * value);
figma_bool_t figma_string_append(figma_memory_t * memory, figma_string_t * string, figma_string_view_t value);
figma_bool_t figma_string_append_cstr(figma_memory_t * memory, figma_string_t * string, const char * value);
figma_bool_t figma_string_copy(figma_memory_t * memory, figma_string_t * target, const figma_string_t * source);
figma_string_view_t figma_string_view(const figma_string_t * string);
figma_string_view_t figma_string_view_cstr(const char * value);
figma_bool_t figma_string_equal_view(const figma_string_t * string, figma_string_view_t value);
figma_bool_t figma_string_equal(const figma_string_t * left, const figma_string_t * right);
figma_bool_t figma_string_view_equal(figma_string_view_t left, figma_string_view_t right);
figma_bool_t figma_string_view_equal_cstr(figma_string_view_t left, const char * right);
figma_bool_t figma_string_contains_view(const figma_string_t * string, figma_string_view_t value);

figma_bool_t figma_format_double_shortest(double value, char * buffer, size_t capacity, size_t * length);

void figma_diagnostics_init(figma_diagnostics_t * diagnostics, figma_memory_t * memory);
void figma_diagnostics_destroy(figma_diagnostics_t * diagnostics);
void figma_diagnostics_clear(figma_diagnostics_t * diagnostics);
figma_bool_t figma_diagnostics_add_owned(figma_diagnostics_t * diagnostics, figma_diagnostic_severity_t severity, const char * code, const char * message, const char * node_id);
figma_bool_t figma_diagnostics_add_view(figma_diagnostics_t * diagnostics, figma_diagnostic_severity_t severity, figma_string_view_t code, figma_string_view_t message, figma_string_view_t node_id);
figma_bool_t figma_diagnostics_copy(figma_diagnostics_t * target, const figma_diagnostics_t * source);

#if !defined(FIGMA_ENABLE_DIAGNOSTICS)
#   if defined(NDEBUG)
#       define FIGMA_ENABLE_DIAGNOSTICS 0
#   else
#       define FIGMA_ENABLE_DIAGNOSTICS 1
#   endif
#endif

#if FIGMA_ENABLE_DIAGNOSTICS
#   define FIGMA_DIAGNOSTIC_ADD(diagnostics, severity, code, message, node_id) \
        figma_diagnostics_add_owned((diagnostics), (severity), (code), (message), (node_id))
#else
#   define FIGMA_DIAGNOSTIC_ADD(diagnostics, severity, code, message, node_id) \
        ((void)(diagnostics), (void)(severity), (void)(code), (void)(message), \
            (void)(node_id), FIGMA_TRUE)
#endif

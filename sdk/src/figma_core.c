#include "figma_internal.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef union figma_memory_alignment
{
#if defined(_MSC_VER)
    double alignment;
#else
    max_align_t alignment;
#endif
    size_t size;
} figma_memory_alignment_t;

typedef struct figma_memory_header
{
    figma_memory_alignment_t storage;
} figma_memory_header_t;

//////////////////////////////////////////////////////////////////////////
static void __figma_diagnostic_owned_destroy(figma_memory_t * memory, void * element)
{
    figma_diagnostic_owned_t * diagnostic = (figma_diagnostic_owned_t *)element;

    figma_string_destroy(memory, &diagnostic->code);
    figma_string_destroy(memory, &diagnostic->message);
    figma_string_destroy(memory, &diagnostic->node_id);
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_size_add(size_t left, size_t right, size_t * result)
{
    if(result == NULL || left > SIZE_MAX - right)
    {
        return FIGMA_FALSE;
    }

    *result = left + right;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_size_mul(size_t left, size_t right, size_t * result)
{
    if(result == NULL || (right != 0u && left > SIZE_MAX / right))
    {
        return FIGMA_FALSE;
    }

    *result = left * right;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
void * figma_memory_allocate(figma_memory_t * memory, size_t size)
{
    figma_memory_header_t * header;
    size_t allocation_size;

    if(memory == NULL || figma_size_add(sizeof(figma_memory_header_t), size, &allocation_size) == FIGMA_FALSE)
    {
        return NULL;
    }

    if(memory->allocator.alloc != NULL)
    {
        header = (figma_memory_header_t *)memory->allocator.alloc(
            allocation_size, memory->allocator.user_data);
    }
    else
    {
        header = (figma_memory_header_t *)malloc(allocation_size);
    }

    if(header == NULL)
    {
        return NULL;
    }

    header->storage.size = size;

    return (void *)(header + 1);
}

//////////////////////////////////////////////////////////////////////////
void * figma_memory_reallocate(figma_memory_t * memory, void * ptr, size_t size)
{
    figma_memory_header_t * header;
    figma_memory_header_t * new_header;
    size_t allocation_size;
    size_t old_size;

    if(ptr == NULL)
    {
        return figma_memory_allocate(memory, size);
    }

    if(size == 0u)
    {
        figma_memory_deallocate(memory, ptr);
        return NULL;
    }

    if(memory == NULL || figma_size_add(sizeof(figma_memory_header_t), size, &allocation_size) == FIGMA_FALSE)
    {
        return NULL;
    }

    header = ((figma_memory_header_t *)ptr) - 1;
    old_size = header->storage.size;

    if(memory->allocator.realloc != NULL)
    {
        new_header = (figma_memory_header_t *)memory->allocator.realloc(
            header, allocation_size, memory->allocator.user_data);

        if(new_header == NULL)
        {
            return NULL;
        }

        new_header->storage.size = size;

        return (void *)(new_header + 1);
    }

    if(memory->allocator.alloc == NULL)
    {
        new_header = (figma_memory_header_t *)realloc(header, allocation_size);

        if(new_header == NULL)
        {
            return NULL;
        }

        new_header->storage.size = size;

        return (void *)(new_header + 1);
    }

    {
        void * new_ptr = figma_memory_allocate(memory, size);

        if(new_ptr == NULL)
        {
            return NULL;
        }

        memcpy(new_ptr, ptr, old_size < size ? old_size : size);
        figma_memory_deallocate(memory, ptr);

        return new_ptr;
    }
}

//////////////////////////////////////////////////////////////////////////
void figma_memory_deallocate(figma_memory_t * memory, void * ptr)
{
    figma_memory_header_t * header;

    if(memory == NULL || ptr == NULL)
    {
        return;
    }

    header = ((figma_memory_header_t *)ptr) - 1;

    if(memory->allocator.free != NULL)
    {
        memory->allocator.free(header, memory->allocator.user_data);
    }
    else
    {
        free(header);
    }
}

//////////////////////////////////////////////////////////////////////////
void figma_array_init(figma_array_t * array, size_t element_size)
{
    if(array == NULL)
    {
        return;
    }

    array->data = NULL;
    array->size = 0u;
    array->capacity = 0u;
    array->element_size = element_size;
}

//////////////////////////////////////////////////////////////////////////
void figma_array_destroy(figma_memory_t * memory, figma_array_t * array, figma_element_destroy_fn destroy)
{
    if(array == NULL)
    {
        return;
    }

    figma_array_clear(memory, array, destroy);
    figma_memory_deallocate(memory, array->data);
    figma_array_init(array, array->element_size);
}

//////////////////////////////////////////////////////////////////////////
void figma_array_clear(figma_memory_t * memory, figma_array_t * array, figma_element_destroy_fn destroy)
{
    size_t index;

    if(array == NULL)
    {
        return;
    }

    if(destroy != NULL)
    {
        for(index = array->size; index != 0u; --index)
        {
            destroy(memory, (uint8_t *)array->data + (index - 1u) * array->element_size);
        }
    }

    array->size = 0u;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_array_reserve(figma_memory_t * memory, figma_array_t * array, size_t capacity)
{
    void * data;
    size_t bytes;
    size_t new_capacity;

    if(memory == NULL || array == NULL || array->element_size == 0u)
    {
        return FIGMA_FALSE;
    }

    if(capacity <= array->capacity)
    {
        return FIGMA_TRUE;
    }

    new_capacity = array->capacity != 0u ? array->capacity : 4u;

    while(new_capacity < capacity)
    {
        if(new_capacity > SIZE_MAX / 2u)
        {
            new_capacity = capacity;
            break;
        }

        new_capacity *= 2u;
    }

    if(figma_size_mul(new_capacity, array->element_size, &bytes) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    data = figma_memory_reallocate(memory, array->data, bytes);

    if(data == NULL)
    {
        return FIGMA_FALSE;
    }

    array->data = data;
    array->capacity = new_capacity;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_array_resize(figma_memory_t * memory, figma_array_t * array, size_t size, figma_element_destroy_fn destroy)
{
    size_t old_size;

    if(array == NULL)
    {
        return FIGMA_FALSE;
    }

    old_size = array->size;

    if(size < old_size)
    {
        if(destroy != NULL)
        {
            size_t index;

            for(index = old_size; index != size; --index)
            {
                destroy(memory, (uint8_t *)array->data + (index - 1u) * array->element_size);
            }
        }

        array->size = size;

        return FIGMA_TRUE;
    }

    if(size == old_size)
    {
        return FIGMA_TRUE;
    }

    if(figma_array_reserve(memory, array, size) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    memset(
        (uint8_t *)array->data + old_size * array->element_size,
        0,
        (size - old_size) * array->element_size);
    array->size = size;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
void * figma_array_push_uninitialized(figma_memory_t * memory, figma_array_t * array)
{
    void * value;

    if(array == NULL || figma_array_reserve(memory, array, array->size + 1u) == FIGMA_FALSE)
    {
        return NULL;
    }

    value = (uint8_t *)array->data + array->size * array->element_size;
    memset(value, 0, array->element_size);
    ++array->size;

    return value;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_array_push_copy(figma_memory_t * memory, figma_array_t * array, const void * value)
{
    void * target;

    if(value == NULL)
    {
        return FIGMA_FALSE;
    }

    target = figma_array_push_uninitialized(memory, array);

    if(target == NULL)
    {
        return FIGMA_FALSE;
    }

    memcpy(target, value, array->element_size);

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
void figma_array_pop(figma_memory_t * memory, figma_array_t * array, figma_element_destroy_fn destroy)
{
    if(array == NULL || array->size == 0u)
    {
        return;
    }

    if(destroy != NULL)
    {
        destroy(memory, (uint8_t *)array->data + (array->size - 1u) * array->element_size);
    }

    --array->size;
}

//////////////////////////////////////////////////////////////////////////
void figma_array_remove(figma_memory_t * memory, figma_array_t * array, size_t index, figma_element_destroy_fn destroy)
{
    if(array == NULL || index >= array->size)
    {
        return;
    }

    if(destroy != NULL)
    {
        destroy(memory, (uint8_t *)array->data + index * array->element_size);
    }

    if(index + 1u != array->size)
    {
        memmove(
            (uint8_t *)array->data + index * array->element_size,
            (uint8_t *)array->data + (index + 1u) * array->element_size,
            (array->size - index - 1u) * array->element_size);
    }

    --array->size;
}

//////////////////////////////////////////////////////////////////////////
void figma_string_init(figma_string_t * string)
{
    if(string == NULL)
    {
        return;
    }

    string->data = NULL;
    string->size = 0u;
    string->capacity = 0u;
}

//////////////////////////////////////////////////////////////////////////
void figma_string_destroy(figma_memory_t * memory, figma_string_t * string)
{
    if(string == NULL)
    {
        return;
    }

    figma_memory_deallocate(memory, string->data);
    figma_string_init(string);
}

//////////////////////////////////////////////////////////////////////////
void figma_string_clear(figma_string_t * string)
{
    if(string == NULL)
    {
        return;
    }

    string->size = 0u;

    if(string->data != NULL)
    {
        string->data[0] = '\0';
    }
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_string_reserve(figma_memory_t * memory, figma_string_t * string, size_t capacity)
{
    char * data;
    size_t required;
    size_t new_capacity;

    if(memory == NULL || string == NULL || figma_size_add(capacity, 1u, &required) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    if(required <= string->capacity)
    {
        return FIGMA_TRUE;
    }

    new_capacity = string->capacity != 0u ? string->capacity : 16u;

    while(new_capacity < required)
    {
        if(new_capacity > SIZE_MAX / 2u)
        {
            new_capacity = required;
            break;
        }

        new_capacity *= 2u;
    }

    data = (char *)figma_memory_reallocate(memory, string->data, new_capacity);

    if(data == NULL)
    {
        return FIGMA_FALSE;
    }

    string->data = data;
    string->capacity = new_capacity;

    if(string->size == 0u)
    {
        string->data[0] = '\0';
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_string_assign(figma_memory_t * memory, figma_string_t * string, figma_string_view_t value)
{
    if(string == NULL || (value.size != 0u && value.data == NULL))
    {
        return FIGMA_FALSE;
    }

    if(figma_string_reserve(memory, string, value.size) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    if(value.size != 0u)
    {
        memmove(string->data, value.data, value.size);
    }

    string->data[value.size] = '\0';
    string->size = value.size;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_string_assign_cstr(figma_memory_t * memory, figma_string_t * string, const char * value)
{
    return figma_string_assign(memory, string, figma_string_view_cstr(value));
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_string_append(figma_memory_t * memory, figma_string_t * string, figma_string_view_t value)
{
    size_t size;

    if(string == NULL || (value.size != 0u && value.data == NULL) ||
        figma_size_add(string->size, value.size, &size) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    if(figma_string_reserve(memory, string, size) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    if(value.size != 0u)
    {
        memmove(string->data + string->size, value.data, value.size);
    }

    string->data[size] = '\0';
    string->size = size;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_string_append_cstr(figma_memory_t * memory, figma_string_t * string, const char * value)
{
    return figma_string_append(memory, string, figma_string_view_cstr(value));
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_string_copy(figma_memory_t * memory, figma_string_t * target, const figma_string_t * source)
{
    if(source == NULL)
    {
        return FIGMA_FALSE;
    }

    return figma_string_assign(memory, target, figma_string_view(source));
}

//////////////////////////////////////////////////////////////////////////
figma_string_view_t figma_string_view(const figma_string_t * string)
{
    figma_string_view_t value;

    value.data = string != NULL ? string->data : NULL;
    value.size = string != NULL ? string->size : 0u;

    return value;
}

//////////////////////////////////////////////////////////////////////////
figma_string_view_t figma_string_view_cstr(const char * value)
{
    figma_string_view_t view;

    view.data = value;
    view.size = value != NULL ? strlen(value) : 0u;

    return view;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_string_equal_view(const figma_string_t * string, figma_string_view_t value)
{
    return figma_string_view_equal(figma_string_view(string), value);
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_string_equal(const figma_string_t * left, const figma_string_t * right)
{
    return figma_string_view_equal(figma_string_view(left), figma_string_view(right));
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_string_view_equal(figma_string_view_t left, figma_string_view_t right)
{
    if(left.size != right.size)
    {
        return FIGMA_FALSE;
    }

    if(left.size == 0u)
    {
        return FIGMA_TRUE;
    }

    if(left.data == NULL || right.data == NULL)
    {
        return FIGMA_FALSE;
    }

    return memcmp(left.data, right.data, left.size) == 0 ? FIGMA_TRUE : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_string_view_equal_cstr(figma_string_view_t left, const char * right)
{
    return figma_string_view_equal(left, figma_string_view_cstr(right));
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_string_contains_view(const figma_string_t * string, figma_string_view_t value)
{
    size_t index;

    if(string == NULL || value.size == 0u)
    {
        return FIGMA_TRUE;
    }

    if(value.data == NULL || value.size > string->size)
    {
        return FIGMA_FALSE;
    }

    for(index = 0u; index + value.size <= string->size; ++index)
    {
        if(memcmp(string->data + index, value.data, value.size) == 0)
        {
            return FIGMA_TRUE;
        }
    }

    return FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
void figma_diagnostics_init(figma_diagnostics_t * diagnostics, figma_memory_t * memory)
{
    if(diagnostics == NULL)
    {
        return;
    }

    diagnostics->memory = memory;
    figma_array_init(&diagnostics->items, sizeof(figma_diagnostic_owned_t));
}

//////////////////////////////////////////////////////////////////////////
void figma_diagnostics_destroy(figma_diagnostics_t * diagnostics)
{
    if(diagnostics == NULL)
    {
        return;
    }

    figma_array_destroy(
        diagnostics->memory, &diagnostics->items, &__figma_diagnostic_owned_destroy);
    diagnostics->memory = NULL;
}

//////////////////////////////////////////////////////////////////////////
void figma_diagnostics_clear(figma_diagnostics_t * diagnostics)
{
    if(diagnostics == NULL)
    {
        return;
    }

    figma_array_clear(
        diagnostics->memory, &diagnostics->items, &__figma_diagnostic_owned_destroy);
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_diagnostics_add_owned(figma_diagnostics_t * diagnostics, figma_diagnostic_severity_t severity, const char * code, const char * message, const char * node_id)
{
    return figma_diagnostics_add_view(
        diagnostics,
        severity,
        figma_string_view_cstr(code),
        figma_string_view_cstr(message),
        figma_string_view_cstr(node_id));
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_diagnostics_add_view(figma_diagnostics_t * diagnostics, figma_diagnostic_severity_t severity, figma_string_view_t code, figma_string_view_t message, figma_string_view_t node_id)
{
    figma_diagnostic_owned_t diagnostic;

    if(diagnostics == NULL || diagnostics->memory == NULL)
    {
        return FIGMA_FALSE;
    }

    memset(&diagnostic, 0, sizeof(diagnostic));
    diagnostic.severity = severity;
    figma_string_init(&diagnostic.code);
    figma_string_init(&diagnostic.message);
    figma_string_init(&diagnostic.node_id);

    if(figma_string_assign(diagnostics->memory, &diagnostic.code, code) == FIGMA_FALSE ||
        figma_string_assign(diagnostics->memory, &diagnostic.message, message) == FIGMA_FALSE ||
        figma_string_assign(diagnostics->memory, &diagnostic.node_id, node_id) == FIGMA_FALSE ||
        figma_array_push_copy(
            diagnostics->memory, &diagnostics->items, &diagnostic) == FIGMA_FALSE)
    {
        __figma_diagnostic_owned_destroy(diagnostics->memory, &diagnostic);
        return FIGMA_FALSE;
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_diagnostics_copy(figma_diagnostics_t * target, const figma_diagnostics_t * source)
{
    size_t index;

    if(target == NULL || source == NULL)
    {
        return FIGMA_FALSE;
    }

    for(index = 0u; index != source->items.size; ++index)
    {
        const figma_diagnostic_owned_t * item =
            FIGMA_ARRAY_CONST_PTR(figma_diagnostic_owned_t, &source->items, index);

        if(figma_diagnostics_add_view(
               target,
               item->severity,
               figma_string_view(&item->code),
               figma_string_view(&item->message),
               figma_string_view(&item->node_id)) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_runtime_create(uint32_t version, const figma_runtime_desc_t * desc, figma_runtime_t ** runtime)
{
    figma_runtime_desc_t value;
    figma_memory_t memory;
    figma_runtime_t * instance;

    if(runtime == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    *runtime = NULL;

    if(version != FIGMA_SDK_VERSION)
    {
        return FIGMA_RESULT_VERSION_MISMATCH;
    }

    memset(&value, 0, sizeof(value));

    if(desc != NULL)
    {
        value = *desc;
    }

    if((value.allocator.alloc == NULL) != (value.allocator.free == NULL) ||
        (value.allocator.realloc != NULL && value.allocator.alloc == NULL))
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    memset(&memory, 0, sizeof(memory));
    memory.allocator = value.allocator;
    instance = (figma_runtime_t *)figma_memory_allocate(&memory, sizeof(figma_runtime_t));

    if(instance == NULL)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    memset(instance, 0, sizeof(*instance));
    instance->desc = value;
    instance->memory = memory;
    *runtime = instance;

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
void FIGMA_CALL figma_runtime_destroy(figma_runtime_t * runtime)
{
    figma_memory_t memory;

    if(runtime == NULL)
    {
        return;
    }

    memory = runtime->memory;
    figma_memory_deallocate(&memory, runtime);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_runtime_get_desc(const figma_runtime_t * runtime, figma_runtime_desc_t * desc)
{
    if(runtime == NULL || desc == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    *desc = runtime->desc;

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
uint32_t FIGMA_CALL figma_diagnostics_get_count(const figma_diagnostics_t * diagnostics)
{
    if(diagnostics == NULL || diagnostics->items.size > UINT32_MAX)
    {
        return 0u;
    }

    return (uint32_t)diagnostics->items.size;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t FIGMA_CALL figma_diagnostics_has_errors(const figma_diagnostics_t * diagnostics)
{
    size_t index;

    if(diagnostics == NULL)
    {
        return FIGMA_FALSE;
    }

    for(index = 0u; index != diagnostics->items.size; ++index)
    {
        const figma_diagnostic_owned_t * item =
            FIGMA_ARRAY_CONST_PTR(figma_diagnostic_owned_t, &diagnostics->items, index);

        if(item->severity == FIGMA_DIAGNOSTIC_ERROR)
        {
            return FIGMA_TRUE;
        }
    }

    return FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_diagnostics_get(const figma_diagnostics_t * diagnostics, uint32_t index, figma_diagnostic_t * diagnostic)
{
    const figma_diagnostic_owned_t * item;

    if(diagnostics == NULL || diagnostic == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    if((size_t)index >= diagnostics->items.size)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }

    item = FIGMA_ARRAY_CONST_PTR(figma_diagnostic_owned_t, &diagnostics->items, index);
    diagnostic->severity = item->severity;
    diagnostic->code = figma_string_view(&item->code);
    diagnostic->message = figma_string_view(&item->message);
    diagnostic->node_id = figma_string_view(&item->node_id);

    return FIGMA_RESULT_OK;
}

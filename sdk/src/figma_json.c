#include "figma_json.h"

#include <stddef.h>

typedef struct figma_json_parse_context
{
    figma_diagnostics_t * diagnostics;
} figma_json_parse_context_t;

//////////////////////////////////////////////////////////////////////////
static void * __figma_json_alloc(size_t size, void * user_data)
{
    return figma_memory_allocate((figma_memory_t *)user_data, size);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_json_free(void * ptr, void * user_data)
{
    figma_memory_deallocate((figma_memory_t *)user_data, ptr);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_json_failed(const char * pointer, const char * end, const char * message, void * user_data)
{
    figma_json_parse_context_t * context =
        (figma_json_parse_context_t *)user_data;

    (void)pointer;
    (void)end;

    if(context != NULL && context->diagnostics != NULL)
    {
        FIGMA_DIAGNOSTIC_ADD(
            context->diagnostics,
            FIGMA_DIAGNOSTIC_ERROR,
            "json_parse_failed",
            message != NULL ? message : "JSON parse failed",
            "");
    }
}

//////////////////////////////////////////////////////////////////////////
void figma_json_document_init(figma_json_document_t * document)
{
    if(document != NULL)
    {
        document->root = NULL;
    }
}

//////////////////////////////////////////////////////////////////////////
void figma_json_document_destroy(figma_json_document_t * document)
{
    if(document != NULL && document->root != NULL)
    {
        js_free(document->root);
        document->root = NULL;
    }
}

//////////////////////////////////////////////////////////////////////////
figma_result_t figma_json_parse(figma_memory_t * memory, figma_string_view_t data, figma_diagnostics_t * diagnostics, figma_json_document_t * document)
{
    js_allocator_t allocator;
    figma_json_parse_context_t context;
    js_element_t * root;
    js_result_t result;

    if(memory == NULL || document == NULL || (data.size != 0u && data.data == NULL))
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    js_make_allocator_default(
        &__figma_json_alloc, &__figma_json_free, memory, &allocator);
    context.diagnostics = diagnostics;
    root = NULL;
    result = js_parse(
        allocator,
        js_flag_none,
        data.data,
        data.size,
        &__figma_json_failed,
        &context,
        &root);

    if(result != JS_SUCCESSFUL || root == NULL)
    {
        if(diagnostics != NULL)
        {
            FIGMA_DIAGNOSTIC_ADD(
                diagnostics,
                FIGMA_DIAGNOSTIC_ERROR,
                "json_parse_failed",
                "Unable to parse JSON",
                "");
        }

        return FIGMA_RESULT_PARSE_FAILED;
    }

    figma_json_document_destroy(document);
    document->root = root;

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
const js_element_t * figma_json_member(const js_element_t * value, figma_string_view_t key)
{
    js_string_t json_key;

    if(value == NULL || js_is_object(value) != JS_TRUE)
    {
        return NULL;
    }

    json_key.value = key.data;
    json_key.size = key.size;

    return js_object_getn(value, json_key);
}

//////////////////////////////////////////////////////////////////////////
const js_element_t * figma_json_index(const js_element_t * value, size_t index)
{
    size_t size;

    if(value == NULL || js_is_array(value) != JS_TRUE)
    {
        return NULL;
    }

    size = js_array_size(value);

    if(index >= size)
    {
        return NULL;
    }

    return js_array_get(value, size - index - 1u);
}

//////////////////////////////////////////////////////////////////////////
figma_string_view_t figma_json_string(const js_element_t * value, figma_string_view_t default_value)
{
    js_string_t string;
    figma_string_view_t result;

    if(value == NULL || js_is_string(value) != JS_TRUE)
    {
        return default_value;
    }

    js_get_string(value, &string);
    result.data = string.value;
    result.size = string.size;

    return result;
}

//////////////////////////////////////////////////////////////////////////
double figma_json_number(const js_element_t * value, double default_value)
{
    if(value == NULL)
    {
        return default_value;
    }

    if(js_is_integer(value) == JS_TRUE)
    {
        return (double)js_get_integer(value);
    }

    if(js_is_real(value) == JS_TRUE)
    {
        return js_get_real(value);
    }

    return default_value;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_json_bool(const js_element_t * value, figma_bool_t default_value)
{
    if(value == NULL || js_is_boolean(value) != JS_TRUE)
    {
        return default_value;
    }

    return js_get_boolean(value) == JS_TRUE ? FIGMA_TRUE : FIGMA_FALSE;
}

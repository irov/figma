#include "figma_canvas.h"

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct figma_canvas_matrix
{
    float m00;
    float m01;
    float m02;
    float m10;
    float m11;
    float m12;
} figma_canvas_matrix_t;

typedef struct figma_canvas_parent_index
{
    figma_string_t id;
    figma_string_t position;
} figma_canvas_parent_index_t;

typedef struct figma_canvas_font_name
{
    figma_string_t family;
    figma_string_t style;
    figma_string_t postscript;
} figma_canvas_font_name_t;

typedef struct figma_canvas_number
{
    float value;
    figma_bool_t percent;
} figma_canvas_number_t;

typedef struct figma_canvas_text_baseline
{
    figma_vec2f_t position;
    float width;
    float line_y;
    float line_height;
    float line_ascent;
    uint32_t first_character;
    uint32_t end_character;
} figma_canvas_text_baseline_t;

typedef struct figma_canvas_node_record
{
    figma_canvas_node_t node;
    figma_string_t parent_id;
    figma_string_t position;
    figma_string_t symbol_id;
    uint32_t style_id;
    figma_bool_t fill_style;
    figma_bool_t stroke_style;
    figma_canvas_matrix_t transform;
    figma_vec2f_t size;
    figma_array_t children;
} figma_canvas_node_record_t;

typedef struct figma_canvas_decoder
{
    figma_memory_t * memory;
    const figma_kiwi_schema_t * schema;
    figma_array_t records;
    figma_array_t blobs;
    uint32_t blob_base_index;
    figma_bool_t failed;
} figma_canvas_decoder_t;

//////////////////////////////////////////////////////////////////////////
static figma_canvas_matrix_t __figma_canvas_identity_matrix(void)
{
    figma_canvas_matrix_t matrix;
    matrix.m00 = 1.0f;
    matrix.m01 = 0.0f;
    matrix.m02 = 0.0f;
    matrix.m10 = 0.0f;
    matrix.m11 = 1.0f;
    matrix.m12 = 0.0f;
    return matrix;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_node_record_init(figma_canvas_node_record_t * record)
{
    memset(record, 0, sizeof(*record));
    figma_canvas_node_init(&record->node);
    figma_string_init(&record->parent_id);
    figma_string_init(&record->position);
    figma_string_init(&record->symbol_id);
    record->transform = __figma_canvas_identity_matrix();
    figma_array_init(&record->children, sizeof(size_t));
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_node_record_destroy(figma_memory_t * memory, void * value)
{
    figma_canvas_node_record_t * record =
        (figma_canvas_node_record_t *)value;
    figma_canvas_node_destroy(memory, &record->node);
    figma_string_destroy(memory, &record->parent_id);
    figma_string_destroy(memory, &record->position);
    figma_string_destroy(memory, &record->symbol_id);
    figma_array_destroy(memory, &record->children, NULL);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_blob_destroy(figma_memory_t * memory, void * value)
{
    figma_array_destroy(memory, (figma_array_t *)value, NULL);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_decoder_init(figma_canvas_decoder_t * decoder, figma_memory_t * memory, const figma_kiwi_schema_t * schema)
{
    memset(decoder, 0, sizeof(*decoder));
    decoder->memory = memory;
    decoder->schema = schema;
    figma_array_init(&decoder->records, sizeof(figma_canvas_node_record_t));
    figma_array_init(&decoder->blobs, sizeof(figma_array_t));
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_decoder_destroy(figma_canvas_decoder_t * decoder)
{
    figma_array_destroy(
        decoder->memory,
        &decoder->records,
        &__figma_canvas_node_record_destroy);
    figma_array_destroy(
        decoder->memory, &decoder->blobs, &__figma_canvas_blob_destroy);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_field_is(const figma_kiwi_field_t * field, const char * name)
{
    return figma_string_equal_view(
        &field->name, figma_string_view_cstr(name));
}

//////////////////////////////////////////////////////////////////////////
static const figma_kiwi_definition_t * __figma_canvas_definition(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, const char * name)
{
    const figma_kiwi_definition_t * definition =
        figma_kiwi_find_definition(
            decoder->schema, figma_string_view_cstr(name));

    if(definition == NULL)
    {
        decoder->failed = FIGMA_TRUE;
        reader->failed = FIGMA_TRUE;
    }

    return definition;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_skip(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, const figma_kiwi_field_t * field)
{
    if(figma_kiwi_skip_value(
           decoder->memory,
           decoder->schema,
           reader,
           figma_string_view(&field->type),
           field->array) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_read_enum_value(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, const figma_kiwi_field_t * field, figma_string_t * value)
{
    if(figma_kiwi_read_enum(
           decoder->memory,
           decoder->schema,
           reader,
           figma_string_view(&field->type),
           value) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_string_set(figma_canvas_decoder_t * decoder, figma_string_t * target, const figma_string_t * source)
{
    if(figma_string_copy(decoder->memory, target, source) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_guid(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_string_t * value)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "GUID");
    uint32_t session_id = 0u;
    uint32_t local_id = 0u;
    size_t index;
    char buffer[64];
    int size;
    figma_string_view_t string;

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    for(index = 0u; index != definition->fields.size; ++index)
    {
        const figma_kiwi_field_t * field =
            FIGMA_ARRAY_CONST_PTR(
                figma_kiwi_field_t, &definition->fields, index);

        if(__figma_canvas_field_is(field, "sessionID") == FIGMA_TRUE)
        {
            session_id = figma_kiwi_read_var_uint(reader);
        }
        else if(__figma_canvas_field_is(field, "localID") == FIGMA_TRUE)
        {
            local_id = figma_kiwi_read_var_uint(reader);
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }

    if(reader->failed == FIGMA_TRUE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    size = snprintf(buffer, sizeof(buffer), "%u:%u", session_id, local_id);

    if(size < 0 || (size_t)size >= sizeof(buffer))
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    string.data = buffer;
    string.size = (size_t)size;

    if(figma_string_assign(decoder->memory, value, string) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_style_id(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_string_t * value)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "StyleId");

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        if(tag == 0u)
        {
            return FIGMA_TRUE;
        }

        field = figma_kiwi_find_field(definition, tag);

        if(field == NULL)
        {
            decoder->failed = FIGMA_TRUE;
            reader->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        if(__figma_canvas_field_is(field, "guid") == FIGMA_TRUE)
        {
            if(__figma_canvas_decode_guid(decoder, reader, value) == FIGMA_FALSE)
            {
                return FIGMA_FALSE;
            }
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_vec2f_t __figma_canvas_decode_vector(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "Vector");
    figma_vec2f_t value = {0.0f, 0.0f};
    size_t index;

    if(definition == NULL)
    {
        return value;
    }

    for(index = 0u; index != definition->fields.size; ++index)
    {
        const figma_kiwi_field_t * field =
            FIGMA_ARRAY_CONST_PTR(
                figma_kiwi_field_t, &definition->fields, index);

        if(__figma_canvas_field_is(field, "x") == FIGMA_TRUE)
        {
            value.x = figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(field, "y") == FIGMA_TRUE)
        {
            value.y = figma_kiwi_read_var_float(reader);
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }

    return value;
}

//////////////////////////////////////////////////////////////////////////
static figma_canvas_matrix_t __figma_canvas_decode_matrix(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "Matrix");
    figma_canvas_matrix_t value = __figma_canvas_identity_matrix();
    size_t index;

    if(definition == NULL)
    {
        return value;
    }

    for(index = 0u; index != definition->fields.size; ++index)
    {
        const figma_kiwi_field_t * field =
            FIGMA_ARRAY_CONST_PTR(
                figma_kiwi_field_t, &definition->fields, index);
        float * target = NULL;

        if(__figma_canvas_field_is(field, "m00") == FIGMA_TRUE)
        {
            target = &value.m00;
        }
        else if(__figma_canvas_field_is(field, "m01") == FIGMA_TRUE)
        {
            target = &value.m01;
        }
        else if(__figma_canvas_field_is(field, "m02") == FIGMA_TRUE)
        {
            target = &value.m02;
        }
        else if(__figma_canvas_field_is(field, "m10") == FIGMA_TRUE)
        {
            target = &value.m10;
        }
        else if(__figma_canvas_field_is(field, "m11") == FIGMA_TRUE)
        {
            target = &value.m11;
        }
        else if(__figma_canvas_field_is(field, "m12") == FIGMA_TRUE)
        {
            target = &value.m12;
        }

        if(target != NULL)
        {
            *target = figma_kiwi_read_var_float(reader);
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }

    return value;
}

//////////////////////////////////////////////////////////////////////////
static figma_colorf_t __figma_canvas_decode_color(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "Color");
    figma_colorf_t value = {1.0f, 1.0f, 1.0f, 1.0f};
    size_t index;

    if(definition == NULL)
    {
        return value;
    }

    for(index = 0u; index != definition->fields.size; ++index)
    {
        const figma_kiwi_field_t * field =
            FIGMA_ARRAY_CONST_PTR(
                figma_kiwi_field_t, &definition->fields, index);
        float * target = NULL;

        if(__figma_canvas_field_is(field, "r") == FIGMA_TRUE)
        {
            target = &value.r;
        }
        else if(__figma_canvas_field_is(field, "g") == FIGMA_TRUE)
        {
            target = &value.g;
        }
        else if(__figma_canvas_field_is(field, "b") == FIGMA_TRUE)
        {
            target = &value.b;
        }
        else if(__figma_canvas_field_is(field, "a") == FIGMA_TRUE)
        {
            target = &value.a;
        }

        if(target != NULL)
        {
            *target = figma_kiwi_read_var_float(reader);
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }

    return value;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_parent_index_init(figma_canvas_parent_index_t * value)
{
    memset(value, 0, sizeof(*value));
    figma_string_init(&value->id);
    figma_string_init(&value->position);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_parent_index_destroy(figma_memory_t * memory, figma_canvas_parent_index_t * value)
{
    figma_string_destroy(memory, &value->id);
    figma_string_destroy(memory, &value->position);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_parent_index(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_canvas_parent_index_t * value)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "ParentIndex");
    size_t index;

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    for(index = 0u; index != definition->fields.size; ++index)
    {
        const figma_kiwi_field_t * field =
            FIGMA_ARRAY_CONST_PTR(
                figma_kiwi_field_t, &definition->fields, index);

        if(__figma_canvas_field_is(field, "guid") == FIGMA_TRUE)
        {
            __figma_canvas_decode_guid(decoder, reader, &value->id);
        }
        else if(__figma_canvas_field_is(field, "position") == FIGMA_TRUE)
        {
            if(figma_kiwi_read_string(
                   decoder->memory, reader, &value->position) == FIGMA_FALSE)
            {
                decoder->failed = FIGMA_TRUE;
            }
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }

    return decoder->failed == FIGMA_FALSE && reader->failed == FIGMA_FALSE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_font_name_init(figma_canvas_font_name_t * value)
{
    memset(value, 0, sizeof(*value));
    figma_string_init(&value->family);
    figma_string_init(&value->style);
    figma_string_init(&value->postscript);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_font_name_destroy(figma_memory_t * memory, figma_canvas_font_name_t * value)
{
    figma_string_destroy(memory, &value->family);
    figma_string_destroy(memory, &value->style);
    figma_string_destroy(memory, &value->postscript);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_font_name(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_canvas_font_name_t * value)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "FontName");
    size_t index;

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    for(index = 0u; index != definition->fields.size; ++index)
    {
        const figma_kiwi_field_t * field =
            FIGMA_ARRAY_CONST_PTR(
                figma_kiwi_field_t, &definition->fields, index);
        figma_string_t * target = NULL;

        if(__figma_canvas_field_is(field, "family") == FIGMA_TRUE)
        {
            target = &value->family;
        }
        else if(__figma_canvas_field_is(field, "style") == FIGMA_TRUE)
        {
            target = &value->style;
        }
        else if(__figma_canvas_field_is(field, "postscript") == FIGMA_TRUE)
        {
            target = &value->postscript;
        }

        if(target != NULL)
        {
            if(figma_kiwi_read_string(
                   decoder->memory, reader, target) == FIGMA_FALSE)
            {
                decoder->failed = FIGMA_TRUE;
            }
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }

    return decoder->failed == FIGMA_FALSE && reader->failed == FIGMA_FALSE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_canvas_number_t __figma_canvas_decode_number(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "Number");
    figma_canvas_number_t value = {0.0f, FIGMA_FALSE};
    size_t index;

    if(definition == NULL)
    {
        return value;
    }

    for(index = 0u; index != definition->fields.size; ++index)
    {
        const figma_kiwi_field_t * field =
            FIGMA_ARRAY_CONST_PTR(
                figma_kiwi_field_t, &definition->fields, index);

        if(__figma_canvas_field_is(field, "value") == FIGMA_TRUE)
        {
            value.value = figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(field, "units") == FIGMA_TRUE)
        {
            figma_string_t units;
            figma_string_init(&units);
            __figma_canvas_read_enum_value(decoder, reader, field, &units);
            value.percent =
                figma_string_equal_view(
                    &units, figma_string_view_cstr("PERCENT"));
            figma_string_destroy(decoder->memory, &units);
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }

    return value;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_float_array(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_array_t * values)
{
    const uint32_t count = figma_kiwi_read_var_uint(reader);
    size_t required_size;
    uint32_t index;

    if(reader->failed == FIGMA_TRUE ||
        figma_size_add(values->size, count, &required_size) == FIGMA_FALSE ||
        figma_array_reserve(
            decoder->memory, values, required_size) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    for(index = 0u; index != count; ++index)
    {
        const float value = figma_kiwi_read_var_float(reader);

        if(reader->failed == FIGMA_TRUE ||
            figma_array_push_copy(
                decoder->memory, values, &value) == FIGMA_FALSE)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_blob_array(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "Blob");
    const uint32_t count = figma_kiwi_read_var_uint(reader);
    uint32_t item_index;

    if(definition == NULL || reader->failed == FIGMA_TRUE ||
        figma_array_reserve(
            decoder->memory, &decoder->blobs, count) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    for(item_index = 0u; item_index != count; ++item_index)
    {
        figma_array_t * bytes =
            (figma_array_t *)figma_array_push_uninitialized(
                decoder->memory, &decoder->blobs);
        size_t field_index;

        if(bytes == NULL)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        figma_array_init(bytes, sizeof(uint8_t));

        for(field_index = 0u;
            field_index != definition->fields.size;
            ++field_index)
        {
            const figma_kiwi_field_t * field =
                FIGMA_ARRAY_CONST_PTR(
                    figma_kiwi_field_t,
                    &definition->fields,
                    field_index);

            if(__figma_canvas_field_is(field, "bytes") == FIGMA_TRUE)
            {
                if(figma_kiwi_read_byte_array(
                       decoder->memory, reader, bytes) == FIGMA_FALSE)
                {
                    decoder->failed = FIGMA_TRUE;
                    return FIGMA_FALSE;
                }
            }
            else
            {
                __figma_canvas_skip(decoder, reader, field);
            }
        }
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_path(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_canvas_path_t * path)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "Path");

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        if(tag == 0u)
        {
            return FIGMA_TRUE;
        }

        field = figma_kiwi_find_field(definition, tag);

        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        if(__figma_canvas_field_is(field, "windingRule") == FIGMA_TRUE)
        {
            figma_string_t value;
            figma_string_init(&value);
            __figma_canvas_read_enum_value(decoder, reader, field, &value);
            path->winding_rule =
                figma_string_equal_view(
                    &value, figma_string_view_cstr("ODD")) == FIGMA_TRUE
                ? FIGMA_CANVAS_WINDING_ODD
                : FIGMA_CANVAS_WINDING_NON_ZERO;
            figma_string_destroy(decoder->memory, &value);
        }
        else if(__figma_canvas_field_is(field, "commandsBlob") == FIGMA_TRUE)
        {
            path->commands_blob = figma_kiwi_read_var_uint(reader);
        }
        else if(__figma_canvas_field_is(field, "styleID") == FIGMA_TRUE)
        {
            path->style_id = figma_kiwi_read_var_uint(reader);
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_path_array(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_array_t * paths)
{
    const uint32_t count = figma_kiwi_read_var_uint(reader);
    size_t required_size;
    uint32_t index;

    if(reader->failed == FIGMA_TRUE ||
        figma_size_add(paths->size, count, &required_size) == FIGMA_FALSE ||
        figma_array_reserve(
            decoder->memory, paths, required_size) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    for(index = 0u; index != count; ++index)
    {
        figma_canvas_path_t * path =
            (figma_canvas_path_t *)figma_array_push_uninitialized(
                decoder->memory, paths);

        if(path == NULL)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        figma_canvas_path_init(path);

        if(__figma_canvas_decode_path(decoder, reader, path) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static uint32_t __figma_canvas_read_u32_le(const uint8_t * bytes)
{
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8u) |
        ((uint32_t)bytes[2] << 16u) |
        ((uint32_t)bytes[3] << 24u);
}

//////////////////////////////////////////////////////////////////////////
static float __figma_canvas_read_float_le(const uint8_t * bytes)
{
    const uint32_t encoded = __figma_canvas_read_u32_le(bytes);
    float value;
    memcpy(&value, &encoded, sizeof(value));
    return value;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_read_path_point(const figma_array_t * blob, size_t * offset, figma_vec2f_t * point)
{
    const uint8_t * data;

    if(*offset > blob->size || blob->size - *offset < 8u)
    {
        return FIGMA_FALSE;
    }

    data = (const uint8_t *)blob->data;
    point->x = __figma_canvas_read_float_le(data + *offset);
    point->y = __figma_canvas_read_float_le(data + *offset + 4u);
    *offset += 8u;
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_decode_path_commands(figma_canvas_decoder_t * decoder, const figma_array_t * blob, figma_canvas_path_t * path)
{
    size_t offset = 0u;

    figma_array_clear(decoder->memory, &path->commands, NULL);

    while(offset < blob->size)
    {
        const uint8_t opcode = ((const uint8_t *)blob->data)[offset++];
        figma_canvas_path_command_t command;
        figma_bool_t valid = FIGMA_TRUE;

        memset(&command, 0, sizeof(command));

        switch(opcode)
        {
        case 0u:
            command.type = FIGMA_CANVAS_PATH_CLOSE;
            break;
        case 1u:
            command.type = FIGMA_CANVAS_PATH_MOVE_TO;
            valid = __figma_canvas_read_path_point(
                blob, &offset, &command.p0);
            break;
        case 2u:
            command.type = FIGMA_CANVAS_PATH_LINE_TO;
            valid = __figma_canvas_read_path_point(
                blob, &offset, &command.p0);
            break;
        case 3u:
            command.type = FIGMA_CANVAS_PATH_QUADRATIC_TO;
            valid =
                __figma_canvas_read_path_point(
                    blob, &offset, &command.p0) &&
                __figma_canvas_read_path_point(
                    blob, &offset, &command.p1);
            break;
        case 4u:
            command.type = FIGMA_CANVAS_PATH_CUBIC_TO;
            valid =
                __figma_canvas_read_path_point(
                    blob, &offset, &command.p0) &&
                __figma_canvas_read_path_point(
                    blob, &offset, &command.p1) &&
                __figma_canvas_read_path_point(
                    blob, &offset, &command.p2);
            break;
        default:
            valid = FIGMA_FALSE;
            break;
        }

        if(valid == FIGMA_FALSE ||
            figma_array_push_copy(
                decoder->memory, &path->commands, &command) == FIGMA_FALSE)
        {
            figma_array_clear(decoder->memory, &path->commands, NULL);
            return;
        }
    }

    path->commands_decoded =
        path->commands.size != 0u ? FIGMA_TRUE : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_resolve_path_commands(figma_canvas_decoder_t * decoder, figma_canvas_path_t * path)
{
    uint32_t blob_index;

    if(path->commands_blob < decoder->blob_base_index)
    {
        return;
    }

    blob_index = path->commands_blob - decoder->blob_base_index;

    if((size_t)blob_index >= decoder->blobs.size)
    {
        return;
    }

    __figma_canvas_decode_path_commands(
        decoder,
        FIGMA_ARRAY_CONST_PTR(
            figma_array_t, &decoder->blobs, blob_index),
        path);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_resolve_geometry_blobs(figma_canvas_decoder_t * decoder)
{
    size_t record_index;

    for(record_index = 0u;
        record_index != decoder->records.size;
        ++record_index)
    {
        figma_canvas_node_record_t * record =
            FIGMA_ARRAY_PTR(
                figma_canvas_node_record_t,
                &decoder->records,
                record_index);
        size_t path_index;

        for(path_index = 0u;
            path_index != record->node.fill_geometry.size;
            ++path_index)
        {
            __figma_canvas_resolve_path_commands(
                decoder,
                FIGMA_ARRAY_PTR(
                    figma_canvas_path_t,
                    &record->node.fill_geometry,
                    path_index));
        }

        for(path_index = 0u;
            path_index != record->node.stroke_geometry.size;
            ++path_index)
        {
            __figma_canvas_resolve_path_commands(
                decoder,
                FIGMA_ARRAY_PTR(
                    figma_canvas_path_t,
                    &record->node.stroke_geometry,
                    path_index));
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_paint_array_assign(figma_canvas_decoder_t * decoder, figma_array_t * target, const figma_array_t * source)
{
    figma_array_t copy;
    size_t index;

    figma_array_init(&copy, sizeof(figma_canvas_paint_t));

    if(figma_array_reserve(
           decoder->memory, &copy, source->size) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    for(index = 0u; index != source->size; ++index)
    {
        figma_canvas_paint_t * target_paint =
            (figma_canvas_paint_t *)figma_array_push_uninitialized(
                decoder->memory, &copy);

        if(target_paint == NULL)
        {
            decoder->failed = FIGMA_TRUE;
            figma_array_destroy(
                decoder->memory, &copy, &figma_canvas_paint_destroy);
            return FIGMA_FALSE;
        }

        figma_canvas_paint_init(target_paint);

        if(figma_canvas_paint_copy(
               decoder->memory,
               target_paint,
               FIGMA_ARRAY_CONST_PTR(
                   figma_canvas_paint_t, source, index)) == FIGMA_FALSE)
        {
            decoder->failed = FIGMA_TRUE;
            figma_array_destroy(
                decoder->memory, &copy, &figma_canvas_paint_destroy);
            return FIGMA_FALSE;
        }
    }

    figma_array_destroy(
        decoder->memory, target, &figma_canvas_paint_destroy);
    *target = copy;
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_resolve_paint_style_references(figma_canvas_decoder_t * decoder)
{
    size_t record_index;

    for(record_index = 0u;
        record_index != decoder->records.size;
        ++record_index)
    {
        figma_canvas_node_record_t * record =
            FIGMA_ARRAY_PTR(
                figma_canvas_node_record_t,
                &decoder->records,
                record_index);
        size_t style_index;

        for(style_index = 0u;
            style_index != decoder->records.size;
            ++style_index)
        {
            const figma_canvas_node_record_t * style =
                FIGMA_ARRAY_CONST_PTR(
                    figma_canvas_node_record_t,
                    &decoder->records,
                    style_index);

            if(record->node.fill_style_node_id.size != 0u &&
                style->node.fills.size != 0u &&
                figma_string_equal(
                    &record->node.fill_style_node_id,
                    &style->node.id) == FIGMA_TRUE)
            {
                __figma_canvas_paint_array_assign(
                    decoder, &record->node.fills, &style->node.fills);
            }

            if(record->node.stroke_fill_style_node_id.size != 0u &&
                style->node.fills.size != 0u &&
                figma_string_equal(
                    &record->node.stroke_fill_style_node_id,
                    &style->node.id) == FIGMA_TRUE)
            {
                __figma_canvas_paint_array_assign(
                    decoder, &record->node.strokes, &style->node.fills);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static const figma_array_t * __figma_canvas_find_path_override_paints(const figma_canvas_node_t * node, uint32_t style_id, figma_bool_t fill)
{
    size_t index;

    for(index = 0u; index != node->path_style_overrides.size; ++index)
    {
        const figma_canvas_path_style_override_t * override =
            FIGMA_ARRAY_CONST_PTR(
                figma_canvas_path_style_override_t,
                &node->path_style_overrides,
                index);
        const figma_array_t * paints =
            fill == FIGMA_TRUE ? &override->fills : &override->strokes;

        if(override->style_id == style_id && paints->size != 0u)
        {
            return paints;
        }
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////
static const figma_array_t * __figma_canvas_find_global_style_paints(const figma_canvas_decoder_t * decoder, uint32_t style_id, figma_bool_t fill)
{
    size_t index;

    for(index = 0u; index != decoder->records.size; ++index)
    {
        const figma_canvas_node_record_t * record =
            FIGMA_ARRAY_CONST_PTR(
                figma_canvas_node_record_t, &decoder->records, index);

        if(record->style_id != style_id)
        {
            continue;
        }

        if(fill == FIGMA_TRUE &&
            record->fill_style == FIGMA_TRUE &&
            record->node.fills.size != 0u)
        {
            return &record->node.fills;
        }

        if(fill == FIGMA_FALSE &&
            record->stroke_style == FIGMA_TRUE &&
            record->node.strokes.size != 0u)
        {
            return &record->node.strokes;
        }
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_resolve_path_style_paints(figma_canvas_decoder_t * decoder)
{
    size_t record_index;

    for(record_index = 0u;
        record_index != decoder->records.size;
        ++record_index)
    {
        figma_canvas_node_record_t * record =
            FIGMA_ARRAY_PTR(
                figma_canvas_node_record_t,
                &decoder->records,
                record_index);
        figma_array_t * path_arrays[2] = {
            &record->node.fill_geometry,
            &record->node.stroke_geometry};
        uint32_t array_index;

        for(array_index = 0u; array_index != 2u; ++array_index)
        {
            figma_array_t * paths = path_arrays[array_index];
            figma_bool_t fill =
                array_index == 0u ? FIGMA_TRUE : FIGMA_FALSE;
            size_t path_index;

            for(path_index = 0u; path_index != paths->size; ++path_index)
            {
                figma_canvas_path_t * path =
                    FIGMA_ARRAY_PTR(
                        figma_canvas_path_t, paths, path_index);
                const figma_array_t * paints;

                if(path->style_id == 0u)
                {
                    continue;
                }

                paints = __figma_canvas_find_path_override_paints(
                    &record->node, path->style_id, fill);

                if(paints == NULL)
                {
                    paints = __figma_canvas_find_global_style_paints(
                        decoder, path->style_id, fill);
                }

                if(paints != NULL)
                {
                    __figma_canvas_paint_array_assign(
                        decoder, &path->paints, paints);
                }
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_canvas_blend_mode_t __figma_canvas_blend_mode_from_string(figma_string_view_t value)
{
    static const struct
    {
        const char * name;
        figma_canvas_blend_mode_t value;
    } values[] = {
        {"PASS_THROUGH", FIGMA_CANVAS_BLEND_PASS_THROUGH},
        {"NORMAL", FIGMA_CANVAS_BLEND_NORMAL},
        {"MULTIPLY", FIGMA_CANVAS_BLEND_MULTIPLY},
        {"SCREEN", FIGMA_CANVAS_BLEND_SCREEN},
        {"OVERLAY", FIGMA_CANVAS_BLEND_OVERLAY},
        {"DARKEN", FIGMA_CANVAS_BLEND_DARKEN},
        {"LIGHTEN", FIGMA_CANVAS_BLEND_LIGHTEN},
        {"COLOR_DODGE", FIGMA_CANVAS_BLEND_COLOR_DODGE},
        {"COLOR_BURN", FIGMA_CANVAS_BLEND_COLOR_BURN},
        {"SOFT_LIGHT", FIGMA_CANVAS_BLEND_SOFT_LIGHT},
        {"HARD_LIGHT", FIGMA_CANVAS_BLEND_HARD_LIGHT},
        {"DIFFERENCE", FIGMA_CANVAS_BLEND_DIFFERENCE},
        {"EXCLUSION", FIGMA_CANVAS_BLEND_EXCLUSION},
        {"HUE", FIGMA_CANVAS_BLEND_HUE},
        {"SATURATION", FIGMA_CANVAS_BLEND_SATURATION},
        {"COLOR", FIGMA_CANVAS_BLEND_COLOR},
        {"LUMINOSITY", FIGMA_CANVAS_BLEND_LUMINOSITY}};
    size_t index;

    for(index = 0u; index != sizeof(values) / sizeof(values[0]); ++index)
    {
        if(figma_string_view_equal_cstr(value, values[index].name) ==
            FIGMA_TRUE)
        {
            return values[index].value;
        }
    }

    return FIGMA_CANVAS_BLEND_UNSUPPORTED;
}

//////////////////////////////////////////////////////////////////////////
static figma_prototype_event_type_t __figma_canvas_event_from_string(figma_string_view_t value)
{
    if(figma_string_view_equal_cstr(value, "ON_CLICK") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_CLICK;
    }
    if(figma_string_view_equal_cstr(value, "ON_HOVER") == FIGMA_TRUE ||
        figma_string_view_equal_cstr(value, "MOUSE_ENTER") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_HOVER_ENTER;
    }
    if(figma_string_view_equal_cstr(value, "MOUSE_LEAVE") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_HOVER_LEAVE;
    }
    if(figma_string_view_equal_cstr(value, "ON_PRESS") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_PRESS;
    }
    if(figma_string_view_equal_cstr(value, "MOUSE_DOWN") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_POINTER_DOWN;
    }
    if(figma_string_view_equal_cstr(value, "MOUSE_UP") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_POINTER_UP;
    }
    if(figma_string_view_equal_cstr(value, "AFTER_TIMEOUT") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_AFTER_TIMEOUT;
    }
    if(figma_string_view_equal_cstr(value, "ON_KEY_DOWN") == FIGMA_TRUE ||
        figma_string_view_equal_cstr(value, "KEY_DOWN") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_KEY_DOWN;
    }
    return FIGMA_PROTOTYPE_EVENT_UNSUPPORTED;
}

//////////////////////////////////////////////////////////////////////////
static figma_prototype_connection_type_t __figma_canvas_connection_from_string(figma_string_view_t value)
{
    if(figma_string_view_equal_cstr(value, "NONE") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_CONNECTION_NONE;
    }
    if(figma_string_view_equal_cstr(value, "INTERNAL_NODE") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_CONNECTION_INTERNAL_NODE;
    }
    if(figma_string_view_equal_cstr(value, "BACK") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_CONNECTION_BACK;
    }
    if(figma_string_view_equal_cstr(value, "CLOSE") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_CONNECTION_CLOSE;
    }
    return FIGMA_PROTOTYPE_CONNECTION_UNSUPPORTED;
}

//////////////////////////////////////////////////////////////////////////
static figma_prototype_navigation_type_t __figma_canvas_navigation_from_string(figma_string_view_t value)
{
    if(figma_string_view_equal_cstr(value, "NAVIGATE") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_NAVIGATION_NAVIGATE;
    }
    if(figma_string_view_equal_cstr(value, "OVERLAY") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_NAVIGATION_OVERLAY;
    }
    if(figma_string_view_equal_cstr(value, "SWAP") == FIGMA_TRUE ||
        figma_string_view_equal_cstr(value, "SWAP_STATE") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_NAVIGATION_SWAP;
    }
    if(figma_string_view_equal_cstr(value, "SCROLL_TO") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_NAVIGATION_SCROLL_TO;
    }
    return FIGMA_PROTOTYPE_NAVIGATION_UNSUPPORTED;
}

//////////////////////////////////////////////////////////////////////////
static figma_prototype_transition_type_t __figma_canvas_transition_from_string(figma_string_view_t value)
{
    if(figma_string_view_equal_cstr(value, "NONE") == FIGMA_TRUE ||
        figma_string_view_equal_cstr(value, "INSTANT") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_TRANSITION_INSTANT;
    }
    if(figma_string_view_equal_cstr(value, "DISSOLVE") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_TRANSITION_DISSOLVE;
    }
    if(figma_string_view_equal_cstr(value, "SMART_ANIMATE") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_TRANSITION_SMART_ANIMATE;
    }
    if(figma_string_view_equal_cstr(value, "MOVE_IN") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_TRANSITION_MOVE_IN;
    }
    if(figma_string_view_equal_cstr(value, "MOVE_OUT") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_TRANSITION_MOVE_OUT;
    }
    if(figma_string_view_equal_cstr(value, "PUSH") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_TRANSITION_PUSH;
    }
    if(figma_string_view_equal_cstr(value, "SLIDE_IN") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_TRANSITION_SLIDE_IN;
    }
    if(figma_string_view_equal_cstr(value, "SLIDE_OUT") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_TRANSITION_SLIDE_OUT;
    }
    return FIGMA_PROTOTYPE_TRANSITION_UNSUPPORTED;
}

//////////////////////////////////////////////////////////////////////////
static figma_prototype_transition_direction_t __figma_canvas_transition_direction_from_string(figma_string_view_t value)
{
    if(value.size == 0u ||
        figma_string_view_equal_cstr(value, "NONE") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_TRANSITION_DIRECTION_NONE;
    }
    if(figma_string_view_equal_cstr(value, "LEFT") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_TRANSITION_DIRECTION_LEFT;
    }
    if(figma_string_view_equal_cstr(value, "RIGHT") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_TRANSITION_DIRECTION_RIGHT;
    }
    if(figma_string_view_equal_cstr(value, "UP") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_TRANSITION_DIRECTION_UP;
    }
    if(figma_string_view_equal_cstr(value, "DOWN") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_TRANSITION_DIRECTION_DOWN;
    }
    return FIGMA_PROTOTYPE_TRANSITION_DIRECTION_UNSUPPORTED;
}

//////////////////////////////////////////////////////////////////////////
static figma_animation_easing_t __figma_canvas_easing_from_string(figma_string_view_t value)
{
    if(figma_string_view_equal_cstr(value, "LINEAR") == FIGMA_TRUE)
    {
        return FIGMA_ANIMATION_EASING_LINEAR;
    }
    if(figma_string_view_equal_cstr(value, "EASE_IN") == FIGMA_TRUE)
    {
        return FIGMA_ANIMATION_EASING_EASE_IN;
    }
    if(figma_string_view_equal_cstr(value, "EASE_OUT") == FIGMA_TRUE)
    {
        return FIGMA_ANIMATION_EASING_EASE_OUT;
    }
    if(figma_string_view_equal_cstr(value, "EASE_IN_AND_OUT") == FIGMA_TRUE ||
        figma_string_view_equal_cstr(value, "EASE_IN_OUT") == FIGMA_TRUE)
    {
        return FIGMA_ANIMATION_EASING_EASE_IN_OUT;
    }
    if(figma_string_view_equal_cstr(value, "IN_CUBIC") == FIGMA_TRUE)
    {
        return FIGMA_ANIMATION_EASING_IN_CUBIC;
    }
    if(figma_string_view_equal_cstr(value, "OUT_CUBIC") == FIGMA_TRUE)
    {
        return FIGMA_ANIMATION_EASING_OUT_CUBIC;
    }
    if(figma_string_view_equal_cstr(value, "IN_OUT_CUBIC") == FIGMA_TRUE)
    {
        return FIGMA_ANIMATION_EASING_IN_OUT_CUBIC;
    }
    return FIGMA_ANIMATION_EASING_UNSUPPORTED;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_append_unsupported_field(figma_canvas_decoder_t * decoder, figma_array_t * fields, figma_string_view_t name)
{
    size_t index;
    figma_string_t * field;

    for(index = 0u; index != fields->size; ++index)
    {
        if(figma_string_equal_view(
               FIGMA_ARRAY_CONST_PTR(figma_string_t, fields, index),
               name) == FIGMA_TRUE)
        {
            return FIGMA_TRUE;
        }
    }

    field = (figma_string_t *)figma_array_push_uninitialized(
        decoder->memory, fields);

    if(field == NULL)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    figma_string_init(field);

    if(figma_string_assign(decoder->memory, field, name) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_filter_color_adjust(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_canvas_paint_t * paint)
{
    static const char * names[8] = {
        "tint",
        "shadows",
        "highlights",
        "detail",
        "exposure",
        "vignette",
        "temperature",
        "vibrance"};
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "FilterColorAdjust");
    size_t field_index;

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    paint->has_filter_color_adjust = FIGMA_TRUE;

    for(field_index = 0u;
        field_index != definition->fields.size;
        ++field_index)
    {
        const figma_kiwi_field_t * field =
            FIGMA_ARRAY_CONST_PTR(
                figma_kiwi_field_t,
                &definition->fields,
                field_index);
        size_t value_index;
        figma_bool_t found = FIGMA_FALSE;

        for(value_index = 0u; value_index != 8u; ++value_index)
        {
            if(__figma_canvas_field_is(field, names[value_index]) ==
                FIGMA_TRUE)
            {
                paint->filter_color_adjust[value_index] =
                    figma_kiwi_read_var_float(reader);
                found = FIGMA_TRUE;
                break;
            }
        }

        if(found == FIGMA_FALSE)
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }

    return decoder->failed == FIGMA_FALSE && reader->failed == FIGMA_FALSE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_paint_filter(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_canvas_paint_t * paint)
{
    static const char * names[10] = {
        "tint",
        "shadows",
        "highlights",
        "detail",
        "exposure",
        "vignette",
        "temperature",
        "vibrance",
        "contrast",
        "brightness"};
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "PaintFilterMessage");

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    paint->has_paint_filter = FIGMA_TRUE;

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;
        size_t value_index;
        figma_bool_t found = FIGMA_FALSE;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }
        if(tag == 0u)
        {
            return FIGMA_TRUE;
        }

        field = figma_kiwi_find_field(definition, tag);
        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        for(value_index = 0u; value_index != 10u; ++value_index)
        {
            if(__figma_canvas_field_is(field, names[value_index]) ==
                FIGMA_TRUE)
            {
                paint->paint_filter[value_index] =
                    figma_kiwi_read_var_float(reader);
                found = FIGMA_TRUE;
                break;
            }
        }

        if(found == FIGMA_FALSE)
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_canvas_arc_data_t __figma_canvas_decode_arc_data(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "ArcData");
    figma_canvas_arc_data_t value;
    memset(&value, 0, sizeof(value));

    if(definition == NULL)
    {
        return value;
    }

    value.valid = FIGMA_TRUE;

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            return value;
        }
        if(tag == 0u)
        {
            return value;
        }

        field = figma_kiwi_find_field(definition, tag);
        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            return value;
        }

        if(__figma_canvas_field_is(field, "startingAngle") == FIGMA_TRUE)
        {
            value.starting_angle = figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(field, "endingAngle") == FIGMA_TRUE)
        {
            value.ending_angle = figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(field, "innerRadius") == FIGMA_TRUE)
        {
            value.inner_radius = figma_kiwi_read_var_float(reader);
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_prototype_event(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_prototype_interaction_t * interaction)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "PrototypeEvent");

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }
        if(tag == 0u)
        {
            return FIGMA_TRUE;
        }

        field = figma_kiwi_find_field(definition, tag);
        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        if(__figma_canvas_field_is(field, "interactionType") == FIGMA_TRUE)
        {
            __figma_canvas_read_enum_value(
                decoder, reader, field, &interaction->raw_event_type);
            interaction->event_type = __figma_canvas_event_from_string(
                figma_string_view(&interaction->raw_event_type));
        }
        else if(__figma_canvas_field_is(field, "transitionTimeout") == FIGMA_TRUE)
        {
            interaction->transition_timeout =
                figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(field, "keyCode") == FIGMA_TRUE)
        {
            interaction->key_code = figma_kiwi_read_var_uint(reader);
        }
        else
        {
            if(__figma_canvas_field_is(field, "isDeleted") == FIGMA_FALSE)
            {
                __figma_canvas_append_unsupported_field(
                    decoder,
                    &interaction->unsupported_fields,
                    figma_string_view(&field->name));
            }
            __figma_canvas_skip(decoder, reader, field);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_prototype_action(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_prototype_action_t * action)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "PrototypeAction");

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }
        if(tag == 0u)
        {
            return FIGMA_TRUE;
        }

        field = figma_kiwi_find_field(definition, tag);
        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        if(__figma_canvas_field_is(field, "transitionNodeID") == FIGMA_TRUE)
        {
            __figma_canvas_decode_guid(
                decoder, reader, &action->target_node_id);
        }
        else if(__figma_canvas_field_is(field, "connectionType") == FIGMA_TRUE)
        {
            __figma_canvas_read_enum_value(
                decoder, reader, field, &action->raw_connection_type);
            action->connection_type = __figma_canvas_connection_from_string(
                figma_string_view(&action->raw_connection_type));
        }
        else if(__figma_canvas_field_is(field, "navigationType") == FIGMA_TRUE)
        {
            __figma_canvas_read_enum_value(
                decoder, reader, field, &action->raw_navigation_type);
            action->navigation_type = __figma_canvas_navigation_from_string(
                figma_string_view(&action->raw_navigation_type));
        }
        else if(__figma_canvas_field_is(field, "transitionType") == FIGMA_TRUE &&
            figma_kiwi_is_enum(
                decoder->schema, figma_string_view(&field->type)) == FIGMA_TRUE)
        {
            __figma_canvas_read_enum_value(
                decoder, reader, field, &action->raw_transition_type);
            action->transition_type = __figma_canvas_transition_from_string(
                figma_string_view(&action->raw_transition_type));
        }
        else if(__figma_canvas_field_is(field, "transitionDirection") ==
                FIGMA_TRUE &&
            figma_kiwi_is_enum(
                decoder->schema, figma_string_view(&field->type)) == FIGMA_TRUE)
        {
            __figma_canvas_read_enum_value(
                decoder,
                reader,
                field,
                &action->raw_transition_direction);
            action->transition_direction =
                __figma_canvas_transition_direction_from_string(
                    figma_string_view(&action->raw_transition_direction));
        }
        else if((__figma_canvas_field_is(field, "transitionEasing") ==
                     FIGMA_TRUE ||
                    __figma_canvas_field_is(field, "easing") == FIGMA_TRUE ||
                    __figma_canvas_field_is(field, "easingType") == FIGMA_TRUE) &&
            figma_kiwi_is_enum(
                decoder->schema, figma_string_view(&field->type)) == FIGMA_TRUE)
        {
            __figma_canvas_read_enum_value(
                decoder, reader, field, &action->raw_transition_easing);
            action->transition_easing = __figma_canvas_easing_from_string(
                figma_string_view(&action->raw_transition_easing));
        }
        else if(__figma_canvas_field_is(field, "transitionDuration") == FIGMA_TRUE)
        {
            action->transition_duration =
                figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(
                    field, "transitionShouldSmartAnimate") == FIGMA_TRUE)
        {
            action->smart_animate =
                figma_kiwi_read_byte(reader) != 0u ? FIGMA_TRUE : FIGMA_FALSE;
        }
        else if(__figma_canvas_field_is(
                    field, "transitionPreserveScroll") == FIGMA_TRUE)
        {
            action->transition_preserve_scroll =
                figma_kiwi_read_byte(reader) != 0u ? FIGMA_TRUE : FIGMA_FALSE;
        }
        else if(__figma_canvas_field_is(
                    field, "transitionResetVideoPosition") == FIGMA_TRUE)
        {
            action->transition_reset_video_position =
                figma_kiwi_read_byte(reader) != 0u ? FIGMA_TRUE : FIGMA_FALSE;
        }
        else if(__figma_canvas_field_is(field, "easingFunction") == FIGMA_TRUE)
        {
            action->has_easing_function = FIGMA_TRUE;
            __figma_canvas_skip(decoder, reader, field);
        }
        else
        {
            if(__figma_canvas_field_is(field, "isDeleted") == FIGMA_FALSE)
            {
                __figma_canvas_append_unsupported_field(
                    decoder,
                    &action->unsupported_fields,
                    figma_string_view(&field->name));
            }
            __figma_canvas_skip(decoder, reader, field);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_prototype_interaction(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_prototype_interaction_t * interaction)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "PrototypeInteraction");

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }
        if(tag == 0u)
        {
            return FIGMA_TRUE;
        }

        field = figma_kiwi_find_field(definition, tag);
        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        if(__figma_canvas_field_is(field, "id") == FIGMA_TRUE)
        {
            __figma_canvas_decode_guid(decoder, reader, &interaction->id);
        }
        else if(__figma_canvas_field_is(field, "event") == FIGMA_TRUE)
        {
            __figma_canvas_decode_prototype_event(
                decoder, reader, interaction);
        }
        else if(__figma_canvas_field_is(field, "actions") == FIGMA_TRUE)
        {
            const uint32_t count = figma_kiwi_read_var_uint(reader);
            uint32_t index;

            if(reader->failed == FIGMA_TRUE ||
                figma_array_reserve(
                    decoder->memory, &interaction->actions, count) ==
                    FIGMA_FALSE)
            {
                decoder->failed = FIGMA_TRUE;
                return FIGMA_FALSE;
            }

            for(index = 0u; index != count; ++index)
            {
                figma_prototype_action_t * action =
                    (figma_prototype_action_t *)
                        figma_array_push_uninitialized(
                            decoder->memory, &interaction->actions);

                if(action == NULL)
                {
                    decoder->failed = FIGMA_TRUE;
                    return FIGMA_FALSE;
                }

                figma_prototype_action_init(action);

                if(__figma_canvas_decode_prototype_action(
                       decoder, reader, action) == FIGMA_FALSE)
                {
                    return FIGMA_FALSE;
                }
            }
        }
        else
        {
            if(__figma_canvas_field_is(field, "isDeleted") == FIGMA_FALSE)
            {
                __figma_canvas_append_unsupported_field(
                    decoder,
                    &interaction->unsupported_fields,
                    figma_string_view(&field->name));
            }
            __figma_canvas_skip(decoder, reader, field);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_canvas_text_baseline_t __figma_canvas_decode_baseline(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "Baseline");
    figma_canvas_text_baseline_t value;
    memset(&value, 0, sizeof(value));

    if(definition == NULL)
    {
        return value;
    }

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            return value;
        }
        if(tag == 0u)
        {
            return value;
        }

        field = figma_kiwi_find_field(definition, tag);
        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            return value;
        }

        if(__figma_canvas_field_is(field, "position") == FIGMA_TRUE)
        {
            value.position = __figma_canvas_decode_vector(decoder, reader);
        }
        else if(__figma_canvas_field_is(field, "width") == FIGMA_TRUE)
        {
            value.width = figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(field, "lineY") == FIGMA_TRUE)
        {
            value.line_y = figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(field, "lineHeight") == FIGMA_TRUE)
        {
            value.line_height = figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(field, "lineAscent") == FIGMA_TRUE)
        {
            value.line_ascent = figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(field, "firstCharacter") == FIGMA_TRUE)
        {
            value.first_character = figma_kiwi_read_var_uint(reader);
        }
        else if(__figma_canvas_field_is(field, "endCharacter") == FIGMA_TRUE)
        {
            value.end_character = figma_kiwi_read_var_uint(reader);
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_baseline_array(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_array_t * baselines)
{
    const uint32_t count = figma_kiwi_read_var_uint(reader);
    size_t required_size;
    uint32_t index;

    if(reader->failed == FIGMA_TRUE ||
        figma_size_add(baselines->size, count, &required_size) == FIGMA_FALSE ||
        figma_array_reserve(
            decoder->memory, baselines, required_size) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    for(index = 0u; index != count; ++index)
    {
        const figma_canvas_text_baseline_t baseline =
            __figma_canvas_decode_baseline(decoder, reader);

        if(decoder->failed == FIGMA_TRUE ||
            figma_array_push_copy(
                decoder->memory, baselines, &baseline) == FIGMA_FALSE)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_utf8_substring(figma_canvas_decoder_t * decoder, const figma_string_t * value, uint32_t first, uint32_t end, figma_string_t * result)
{
    size_t byte_begin = value->size;
    size_t byte_end = value->size;
    uint32_t character_index = 0u;
    size_t index;
    figma_string_view_t substring;

    for(index = 0u; index < value->size;)
    {
        unsigned char byte;
        size_t advance = 1u;

        if(character_index == first)
        {
            byte_begin = index;
        }

        if(character_index == end)
        {
            byte_end = index;
            break;
        }

        byte = (unsigned char)value->data[index];

        if((byte & 0xe0u) == 0xc0u)
        {
            advance = 2u;
        }
        else if((byte & 0xf0u) == 0xe0u)
        {
            advance = 3u;
        }
        else if((byte & 0xf8u) == 0xf0u)
        {
            advance = 4u;
        }

        index =
            advance > value->size - index ? value->size : index + advance;
        ++character_index;
    }

    if(character_index == first)
    {
        byte_begin = value->size;
    }
    if(character_index == end)
    {
        byte_end = value->size;
    }
    if(byte_begin > byte_end)
    {
        byte_begin = byte_end;
    }

    substring.data = value->data + byte_begin;
    substring.size = byte_end - byte_begin;

    if(figma_string_assign(decoder->memory, result, substring) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_text_line_destroy_local(figma_memory_t * memory, void * value)
{
    figma_canvas_text_line_t * line = (figma_canvas_text_line_t *)value;
    figma_string_destroy(memory, &line->text);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_apply_baselines(figma_canvas_decoder_t * decoder, const figma_array_t * baselines, figma_canvas_node_t * node)
{
    size_t index;

    if(node->text.size == 0u)
    {
        return FIGMA_TRUE;
    }

    figma_array_clear(
        decoder->memory,
        &node->text_lines,
        &__figma_canvas_text_line_destroy_local);

    if(figma_array_reserve(
           decoder->memory, &node->text_lines, baselines->size) ==
        FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    for(index = 0u; index != baselines->size; ++index)
    {
        const figma_canvas_text_baseline_t * baseline =
            FIGMA_ARRAY_CONST_PTR(
                figma_canvas_text_baseline_t, baselines, index);
        figma_canvas_text_line_t * line;

        if(baseline->end_character <= baseline->first_character)
        {
            continue;
        }

        line = (figma_canvas_text_line_t *)figma_array_push_uninitialized(
            decoder->memory, &node->text_lines);

        if(line == NULL)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        memset(line, 0, sizeof(*line));
        figma_string_init(&line->text);

        if(__figma_canvas_utf8_substring(
               decoder,
               &node->text,
               baseline->first_character,
               baseline->end_character,
               &line->text) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }

        while(line->text.size != 0u &&
            (line->text.data[line->text.size - 1u] == '\n' ||
                line->text.data[line->text.size - 1u] == '\r'))
        {
            --line->text.size;
            line->text.data[line->text.size] = '\0';
        }

        line->x = baseline->position.x;
        line->y = baseline->line_y != 0.0f
            ? baseline->line_y
            : baseline->position.y - baseline->line_ascent;
        line->width = baseline->width;
        line->line_height = baseline->line_height;
        line->line_ascent = baseline->line_ascent;
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_text_data(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_canvas_node_t * node)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "TextData");
    figma_string_t characters;
    figma_array_t baselines;
    figma_bool_t result = FIGMA_FALSE;

    figma_string_init(&characters);
    figma_array_init(&baselines, sizeof(figma_canvas_text_baseline_t));

    if(definition == NULL)
    {
        goto cleanup;
    }

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            goto cleanup;
        }
        if(tag == 0u)
        {
            break;
        }

        field = figma_kiwi_find_field(definition, tag);
        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            goto cleanup;
        }

        if(__figma_canvas_field_is(field, "characters") == FIGMA_TRUE)
        {
            if(figma_kiwi_read_string(
                   decoder->memory, reader, &characters) == FIGMA_FALSE)
            {
                decoder->failed = FIGMA_TRUE;
                goto cleanup;
            }
        }
        else if(__figma_canvas_field_is(field, "baselines") == FIGMA_TRUE)
        {
            if(__figma_canvas_decode_baseline_array(
                   decoder, reader, &baselines) == FIGMA_FALSE)
            {
                goto cleanup;
            }
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }

    figma_string_destroy(decoder->memory, &node->text);
    node->text = characters;
    figma_string_init(&characters);
    result = __figma_canvas_apply_baselines(decoder, &baselines, node);

cleanup:
    figma_array_destroy(decoder->memory, &baselines, NULL);
    figma_string_destroy(decoder->memory, &characters);
    return result;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_derived_text_data(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_canvas_node_t * node)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "DerivedTextData");
    figma_array_t baselines;
    figma_bool_t result = FIGMA_FALSE;

    figma_array_init(&baselines, sizeof(figma_canvas_text_baseline_t));

    if(definition == NULL)
    {
        goto cleanup;
    }

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            goto cleanup;
        }
        if(tag == 0u)
        {
            break;
        }

        field = figma_kiwi_find_field(definition, tag);
        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            goto cleanup;
        }

        if(__figma_canvas_field_is(field, "baselines") == FIGMA_TRUE)
        {
            if(__figma_canvas_decode_baseline_array(
                   decoder, reader, &baselines) == FIGMA_FALSE)
            {
                goto cleanup;
            }
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }

    result = __figma_canvas_apply_baselines(decoder, &baselines, node);

cleanup:
    figma_array_destroy(decoder->memory, &baselines, NULL);
    return result;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_bytes_to_hex(figma_canvas_decoder_t * decoder, const figma_array_t * bytes, figma_string_t * result)
{
    static const char hex[] = "0123456789abcdef";
    size_t result_size;
    size_t index;

    if(figma_size_mul(bytes->size, 2u, &result_size) == FIGMA_FALSE ||
        figma_string_reserve(
            decoder->memory, result, result_size + 1u) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    for(index = 0u; index != bytes->size; ++index)
    {
        const uint8_t value = ((const uint8_t *)bytes->data)[index];
        result->data[index * 2u] = hex[(value >> 4u) & 0x0fu];
        result->data[index * 2u + 1u] = hex[value & 0x0fu];
    }

    result->size = result_size;
    result->data[result_size] = '\0';
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_image_hash(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_string_t * hash)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "Image");

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }
        if(tag == 0u)
        {
            return FIGMA_TRUE;
        }

        field = figma_kiwi_find_field(definition, tag);
        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        if(__figma_canvas_field_is(field, "hash") == FIGMA_TRUE)
        {
            figma_array_t bytes;
            figma_bool_t success;
            figma_array_init(&bytes, sizeof(uint8_t));
            success = figma_kiwi_read_byte_array(
                decoder->memory, reader, &bytes);

            if(success == FIGMA_TRUE)
            {
                success = __figma_canvas_bytes_to_hex(
                    decoder, &bytes, hash);
            }

            figma_array_destroy(decoder->memory, &bytes, NULL);

            if(success == FIGMA_FALSE)
            {
                decoder->failed = FIGMA_TRUE;
                return FIGMA_FALSE;
            }
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_symbol_data(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_string_t * symbol_id)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "SymbolData");

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }
        if(tag == 0u)
        {
            return FIGMA_TRUE;
        }

        field = figma_kiwi_find_field(definition, tag);
        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        if(__figma_canvas_field_is(field, "symbolID") == FIGMA_TRUE)
        {
            __figma_canvas_decode_guid(decoder, reader, symbol_id);
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_path_override_destroy_local(figma_memory_t * memory, figma_canvas_path_style_override_t * override)
{
    figma_array_destroy(
        memory, &override->fills, &figma_canvas_paint_destroy);
    figma_array_destroy(
        memory, &override->strokes, &figma_canvas_paint_destroy);
}

static figma_bool_t __figma_canvas_decode_node_change(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_canvas_node_record_t * record);

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_vector_data(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_canvas_node_t * node)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "VectorData");

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    node->has_vector_data = FIGMA_TRUE;

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }
        if(tag == 0u)
        {
            return FIGMA_TRUE;
        }

        field = figma_kiwi_find_field(definition, tag);
        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        if(__figma_canvas_field_is(field, "vectorNetworkBlob") == FIGMA_TRUE)
        {
            node->vector_network_blob =
                figma_kiwi_read_var_uint(reader);
            node->has_vector_network_blob = FIGMA_TRUE;
        }
        else if(__figma_canvas_field_is(field, "normalizedSize") == FIGMA_TRUE)
        {
            node->vector_normalized_size =
                __figma_canvas_decode_vector(decoder, reader);
        }
        else if(__figma_canvas_field_is(field, "styleOverrideTable") == FIGMA_TRUE)
        {
            const uint32_t count = figma_kiwi_read_var_uint(reader);
            uint32_t index;

            if(reader->failed == FIGMA_TRUE ||
                figma_array_reserve(
                    decoder->memory,
                    &node->path_style_overrides,
                    node->path_style_overrides.size + count) == FIGMA_FALSE)
            {
                decoder->failed = FIGMA_TRUE;
                return FIGMA_FALSE;
            }

            for(index = 0u; index != count; ++index)
            {
                figma_canvas_node_record_t override_record;

                __figma_canvas_node_record_init(&override_record);

                if(__figma_canvas_decode_node_change(
                       decoder, reader, &override_record) == FIGMA_FALSE)
                {
                    __figma_canvas_node_record_destroy(
                        decoder->memory, &override_record);
                    return FIGMA_FALSE;
                }

                if(override_record.style_id != 0u)
                {
                    figma_canvas_path_style_override_t * override =
                        (figma_canvas_path_style_override_t *)
                            figma_array_push_uninitialized(
                                decoder->memory,
                                &node->path_style_overrides);

                    if(override == NULL)
                    {
                        __figma_canvas_node_record_destroy(
                            decoder->memory, &override_record);
                        decoder->failed = FIGMA_TRUE;
                        return FIGMA_FALSE;
                    }

                    memset(override, 0, sizeof(*override));
                    override->style_id = override_record.style_id;
                    figma_array_init(
                        &override->fills, sizeof(figma_canvas_paint_t));
                    figma_array_init(
                        &override->strokes, sizeof(figma_canvas_paint_t));

                    if(__figma_canvas_paint_array_assign(
                           decoder,
                           &override->fills,
                           &override_record.node.fills) == FIGMA_FALSE ||
                        __figma_canvas_paint_array_assign(
                           decoder,
                           &override->strokes,
                           &override_record.node.strokes) == FIGMA_FALSE)
                    {
                        __figma_canvas_path_override_destroy_local(
                            decoder->memory, override);
                        figma_array_init(
                            &override->fills, sizeof(figma_canvas_paint_t));
                        figma_array_init(
                            &override->strokes, sizeof(figma_canvas_paint_t));
                        __figma_canvas_node_record_destroy(
                            decoder->memory, &override_record);
                        return FIGMA_FALSE;
                    }
                }

                __figma_canvas_node_record_destroy(
                    decoder->memory, &override_record);
            }
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_paint(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_canvas_paint_t * paint)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "Paint");

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }
        if(tag == 0u)
        {
            return FIGMA_TRUE;
        }

        field = figma_kiwi_find_field(definition, tag);
        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        if(__figma_canvas_field_is(field, "type") == FIGMA_TRUE)
        {
            __figma_canvas_read_enum_value(
                decoder, reader, field, &paint->raw_type);
            if(figma_string_equal_view(
                   &paint->raw_type,
                   figma_string_view_cstr("IMAGE")) == FIGMA_TRUE)
            {
                paint->type = FIGMA_CANVAS_PAINT_IMAGE;
            }
            else if(figma_string_equal_view(
                        &paint->raw_type,
                        figma_string_view_cstr("SOLID")) == FIGMA_TRUE)
            {
                paint->type = FIGMA_CANVAS_PAINT_SOLID;
            }
            else
            {
                paint->type = FIGMA_CANVAS_PAINT_UNSUPPORTED;
            }
        }
        else if(__figma_canvas_field_is(field, "color") == FIGMA_TRUE)
        {
            paint->color = __figma_canvas_decode_color(decoder, reader);
        }
        else if(__figma_canvas_field_is(field, "opacity") == FIGMA_TRUE)
        {
            paint->opacity = figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(field, "visible") == FIGMA_TRUE)
        {
            paint->visible =
                figma_kiwi_read_byte(reader) != 0u ? FIGMA_TRUE : FIGMA_FALSE;
        }
        else if(__figma_canvas_field_is(field, "blendMode") == FIGMA_TRUE)
        {
            __figma_canvas_read_enum_value(
                decoder, reader, field, &paint->raw_blend_mode);
            paint->blend_mode = __figma_canvas_blend_mode_from_string(
                figma_string_view(&paint->raw_blend_mode));
        }
        else if(__figma_canvas_field_is(field, "transform") == FIGMA_TRUE)
        {
            const figma_canvas_matrix_t matrix =
                __figma_canvas_decode_matrix(decoder, reader);
            paint->transform[0] = matrix.m00;
            paint->transform[1] = matrix.m01;
            paint->transform[2] = matrix.m02;
            paint->transform[3] = matrix.m10;
            paint->transform[4] = matrix.m11;
            paint->transform[5] = matrix.m12;
            paint->has_transform = FIGMA_TRUE;
        }
        else if(__figma_canvas_field_is(field, "image") == FIGMA_TRUE)
        {
            __figma_canvas_decode_image_hash(
                decoder, reader, &paint->asset_id);
        }
        else if(__figma_canvas_field_is(field, "imageScaleMode") == FIGMA_TRUE)
        {
            figma_string_t value;
            figma_string_init(&value);
            __figma_canvas_read_enum_value(decoder, reader, field, &value);

            if(figma_string_equal_view(
                   &value, figma_string_view_cstr("STRETCH")) == FIGMA_TRUE)
            {
                paint->image_scale_mode = FIGMA_CANVAS_IMAGE_SCALE_STRETCH;
            }
            else if(figma_string_equal_view(
                        &value, figma_string_view_cstr("FIT")) == FIGMA_TRUE)
            {
                paint->image_scale_mode = FIGMA_CANVAS_IMAGE_SCALE_FIT;
            }
            else if(figma_string_equal_view(
                        &value, figma_string_view_cstr("FILL")) == FIGMA_TRUE)
            {
                paint->image_scale_mode = FIGMA_CANVAS_IMAGE_SCALE_FILL;
            }
            else if(figma_string_equal_view(
                        &value, figma_string_view_cstr("TILE")) == FIGMA_TRUE)
            {
                paint->image_scale_mode = FIGMA_CANVAS_IMAGE_SCALE_TILE;
            }
            else
            {
                paint->image_scale_mode = FIGMA_CANVAS_IMAGE_SCALE_UNKNOWN;
            }

            figma_string_destroy(decoder->memory, &value);
        }
        else if(__figma_canvas_field_is(field, "filterColorAdjust") == FIGMA_TRUE)
        {
            __figma_canvas_decode_filter_color_adjust(
                decoder, reader, paint);
        }
        else if(__figma_canvas_field_is(field, "paintFilter") == FIGMA_TRUE)
        {
            __figma_canvas_decode_paint_filter(decoder, reader, paint);
        }
        else if(__figma_canvas_field_is(field, "originalImageWidth") == FIGMA_TRUE)
        {
            paint->original_image_width =
                figma_kiwi_read_var_uint(reader);
        }
        else if(__figma_canvas_field_is(
                    field, "originalImageHeight") == FIGMA_TRUE)
        {
            paint->original_image_height =
                figma_kiwi_read_var_uint(reader);
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_paint_array(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_array_t * paints)
{
    const uint32_t count = figma_kiwi_read_var_uint(reader);
    size_t required_size;
    uint32_t index;

    if(reader->failed == FIGMA_TRUE ||
        figma_size_add(paints->size, count, &required_size) == FIGMA_FALSE ||
        figma_array_reserve(
            decoder->memory, paints, required_size) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    for(index = 0u; index != count; ++index)
    {
        figma_canvas_paint_t * paint =
            (figma_canvas_paint_t *)figma_array_push_uninitialized(
                decoder->memory, paints);

        if(paint == NULL)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        figma_canvas_paint_init(paint);

        if(__figma_canvas_decode_paint(decoder, reader, paint) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_canvas_matrix_t __figma_canvas_multiply_matrix(figma_canvas_matrix_t left, figma_canvas_matrix_t right)
{
    figma_canvas_matrix_t result;
    result.m00 = left.m00 * right.m00 + left.m01 * right.m10;
    result.m01 = left.m00 * right.m01 + left.m01 * right.m11;
    result.m02 =
        left.m00 * right.m02 + left.m01 * right.m12 + left.m02;
    result.m10 = left.m10 * right.m00 + left.m11 * right.m10;
    result.m11 = left.m10 * right.m01 + left.m11 * right.m11;
    result.m12 =
        left.m10 * right.m02 + left.m11 * right.m12 + left.m12;
    return result;
}

//////////////////////////////////////////////////////////////////////////
static figma_vec2f_t __figma_canvas_transform_point(figma_canvas_matrix_t matrix, float x, float y)
{
    figma_vec2f_t result;
    result.x = matrix.m00 * x + matrix.m01 * y + matrix.m02;
    result.y = matrix.m10 * x + matrix.m11 * y + matrix.m12;
    return result;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_assign_transformed_geometry(figma_canvas_node_t * node, figma_canvas_matrix_t transform, figma_vec2f_t size)
{
    float min_x;
    float min_y;
    float max_x;
    float max_y;
    size_t index;

    node->quad[0] = __figma_canvas_transform_point(transform, 0.0f, 0.0f);
    node->quad[1] =
        __figma_canvas_transform_point(transform, size.x, 0.0f);
    node->quad[2] =
        __figma_canvas_transform_point(transform, size.x, size.y);
    node->quad[3] =
        __figma_canvas_transform_point(transform, 0.0f, size.y);

    min_x = max_x = node->quad[0].x;
    min_y = max_y = node->quad[0].y;

    for(index = 1u; index != 4u; ++index)
    {
        if(node->quad[index].x < min_x)
        {
            min_x = node->quad[index].x;
        }
        if(node->quad[index].x > max_x)
        {
            max_x = node->quad[index].x;
        }
        if(node->quad[index].y < min_y)
        {
            min_y = node->quad[index].y;
        }
        if(node->quad[index].y > max_y)
        {
            max_y = node->quad[index].y;
        }
    }

    node->rect.x = min_x;
    node->rect.y = min_y;
    node->rect.w = max_x - min_x;
    node->rect.h = max_y - min_y;
}

//////////////////////////////////////////////////////////////////////////
static size_t __figma_canvas_find_record(const figma_canvas_decoder_t * decoder, const figma_string_t * id)
{
    size_t index;

    if(id->size == 0u)
    {
        return SIZE_MAX;
    }

    for(index = 0u; index != decoder->records.size; ++index)
    {
        const figma_canvas_node_record_t * record =
            FIGMA_ARRAY_CONST_PTR(
                figma_canvas_node_record_t, &decoder->records, index);

        if(figma_string_equal(&record->node.id, id) == FIGMA_TRUE)
        {
            return index;
        }
    }

    return SIZE_MAX;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_copy_node_recursive(figma_canvas_decoder_t * decoder, size_t record_index, figma_canvas_matrix_t parent_transform, figma_bool_t root_frame, size_t depth, figma_canvas_node_t * node)
{
    const figma_canvas_node_record_t * record;
    const figma_canvas_node_record_t * child_source;
    figma_canvas_matrix_t transform;
    size_t child_index;

    if(record_index >= decoder->records.size ||
        depth > decoder->records.size)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    record = FIGMA_ARRAY_CONST_PTR(
        figma_canvas_node_record_t, &decoder->records, record_index);

    if(figma_canvas_node_copy(
           decoder->memory, node, &record->node) == FIGMA_FALSE ||
        __figma_canvas_string_set(
           decoder, &node->symbol_id, &record->symbol_id) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    node->size = record->size;
    transform = root_frame == FIGMA_TRUE
        ? __figma_canvas_identity_matrix()
        : __figma_canvas_multiply_matrix(parent_transform, record->transform);
    __figma_canvas_assign_transformed_geometry(node, transform, record->size);

    child_source = record;

    if(record->children.size == 0u && record->symbol_id.size != 0u)
    {
        const size_t symbol_index =
            __figma_canvas_find_record(decoder, &record->symbol_id);

        if(symbol_index != SIZE_MAX)
        {
            child_source = FIGMA_ARRAY_CONST_PTR(
                figma_canvas_node_record_t,
                &decoder->records,
                symbol_index);
        }
    }

    if(figma_array_reserve(
           decoder->memory,
           &node->children,
           child_source->children.size) == FIGMA_FALSE)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    for(child_index = 0u;
        child_index != child_source->children.size;
        ++child_index)
    {
        const size_t source_index =
            FIGMA_ARRAY_AT(size_t, &child_source->children, child_index);
        figma_canvas_node_t * child =
            (figma_canvas_node_t *)figma_array_push_uninitialized(
                decoder->memory, &node->children);

        if(child == NULL)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        figma_canvas_node_init(child);

        if(__figma_canvas_copy_node_recursive(
               decoder,
               source_index,
               transform,
               FIGMA_FALSE,
               depth + 1u,
               child) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static int __figma_canvas_compare_position_descending(const figma_string_t * left, const figma_string_t * right)
{
    const size_t common =
        left->size < right->size ? left->size : right->size;
    int comparison = common != 0u
        ? memcmp(left->data, right->data, common)
        : 0;

    if(comparison != 0)
    {
        return -comparison;
    }
    if(left->size < right->size)
    {
        return 1;
    }
    if(left->size > right->size)
    {
        return -1;
    }
    return 0;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_sort_children(figma_canvas_decoder_t * decoder, figma_array_t * children)
{
    size_t index;

    for(index = 1u; index < children->size; ++index)
    {
        size_t position = index;
        const size_t value = FIGMA_ARRAY_AT(size_t, children, index);
        const figma_canvas_node_record_t * value_record =
            FIGMA_ARRAY_CONST_PTR(
                figma_canvas_node_record_t, &decoder->records, value);

        while(position != 0u)
        {
            const size_t previous =
                FIGMA_ARRAY_AT(size_t, children, position - 1u);
            const figma_canvas_node_record_t * previous_record =
                FIGMA_ARRAY_CONST_PTR(
                    figma_canvas_node_record_t,
                    &decoder->records,
                    previous);

            if(__figma_canvas_compare_position_descending(
                   &previous_record->position,
                   &value_record->position) <= 0)
            {
                break;
            }

            FIGMA_ARRAY_AT(size_t, children, position) = previous;
            --position;
        }

        FIGMA_ARRAY_AT(size_t, children, position) = value;
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_collect_prototype_start(figma_canvas_decoder_t * decoder, const figma_canvas_node_t * node, figma_string_t * prototype_start_node_id, size_t depth)
{
    size_t index;

    if(depth > decoder->records.size)
    {
        decoder->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    if(prototype_start_node_id->size == 0u &&
        node->prototype_start_node_id.size != 0u)
    {
        if(__figma_canvas_string_set(
               decoder,
               prototype_start_node_id,
               &node->prototype_start_node_id) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }
    else if(prototype_start_node_id->size == 0u &&
        node->has_prototype_starting_point == FIGMA_TRUE)
    {
        if(__figma_canvas_string_set(
               decoder,
               prototype_start_node_id,
               &node->id) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    for(index = 0u; index != node->children.size; ++index)
    {
        if(__figma_canvas_collect_prototype_start(
               decoder,
               FIGMA_ARRAY_CONST_PTR(
                   figma_canvas_node_t, &node->children, index),
               prototype_start_node_id,
               depth + 1u) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_build_document_tree(figma_canvas_decoder_t * decoder, figma_canvas_node_t * root, figma_string_t * prototype_start_node_id)
{
    size_t root_index = SIZE_MAX;
    size_t parentless_index = SIZE_MAX;
    size_t root_candidate_count = 0u;
    size_t parentless_count = 0u;
    size_t index;

    if(decoder->records.size == 0u)
    {
        return FIGMA_FALSE;
    }

    for(index = 0u; index != decoder->records.size; ++index)
    {
        figma_canvas_node_record_t * record =
            FIGMA_ARRAY_PTR(
                figma_canvas_node_record_t, &decoder->records, index);

        if(record->parent_id.size == 0u)
        {
            ++parentless_count;
            parentless_index = index;

            if(record->node.type == FIGMA_CANVAS_NODE_DOCUMENT)
            {
                ++root_candidate_count;
                root_index = index;
            }
        }
        else
        {
            const size_t parent_index =
                __figma_canvas_find_record(decoder, &record->parent_id);

            if(parent_index != SIZE_MAX)
            {
                figma_canvas_node_record_t * parent =
                    FIGMA_ARRAY_PTR(
                        figma_canvas_node_record_t,
                        &decoder->records,
                        parent_index);

                if(figma_array_push_copy(
                       decoder->memory,
                       &parent->children,
                       &index) == FIGMA_FALSE)
                {
                    decoder->failed = FIGMA_TRUE;
                    return FIGMA_FALSE;
                }
            }
        }
    }

    if(root_candidate_count == 0u && parentless_count == 1u)
    {
        root_index = parentless_index;
        root_candidate_count = 1u;
    }

    if(root_candidate_count != 1u || root_index == SIZE_MAX)
    {
        return FIGMA_FALSE;
    }

    for(index = 0u; index != decoder->records.size; ++index)
    {
        __figma_canvas_sort_children(
            decoder,
            &FIGMA_ARRAY_PTR(
                 figma_canvas_node_record_t,
                 &decoder->records,
                 index)
                 ->children);
    }

    if(__figma_canvas_copy_node_recursive(
           decoder,
           root_index,
           __figma_canvas_identity_matrix(),
           FIGMA_FALSE,
           0u,
           root) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    return __figma_canvas_collect_prototype_start(
        decoder, root, prototype_start_node_id, 0u);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_message(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_canvas_node_t * root, figma_string_t * prototype_start_node_id)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "Message");

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }
        if(tag == 0u)
        {
            break;
        }

        field = figma_kiwi_find_field(definition, tag);

        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        if(__figma_canvas_field_is(field, "nodeChanges") == FIGMA_TRUE &&
            field->array == FIGMA_TRUE)
        {
            const uint32_t count = figma_kiwi_read_var_uint(reader);
            uint32_t index;

            if(reader->failed == FIGMA_TRUE ||
                figma_array_reserve(
                    decoder->memory, &decoder->records, count) == FIGMA_FALSE)
            {
                decoder->failed = FIGMA_TRUE;
                return FIGMA_FALSE;
            }

            for(index = 0u; index != count; ++index)
            {
                figma_canvas_node_record_t * record =
                    (figma_canvas_node_record_t *)
                        figma_array_push_uninitialized(
                            decoder->memory, &decoder->records);

                if(record == NULL)
                {
                    decoder->failed = FIGMA_TRUE;
                    return FIGMA_FALSE;
                }

                __figma_canvas_node_record_init(record);

                if(__figma_canvas_decode_node_change(
                       decoder, reader, record) == FIGMA_FALSE)
                {
                    return FIGMA_FALSE;
                }
            }
        }
        else if(__figma_canvas_field_is(field, "blobs") == FIGMA_TRUE &&
            field->array == FIGMA_TRUE)
        {
            if(__figma_canvas_decode_blob_array(decoder, reader) == FIGMA_FALSE)
            {
                return FIGMA_FALSE;
            }
        }
        else if(__figma_canvas_field_is(field, "blobBaseIndex") == FIGMA_TRUE)
        {
            decoder->blob_base_index = figma_kiwi_read_var_uint(reader);
        }
        else
        {
            if(__figma_canvas_skip(decoder, reader, field) == FIGMA_FALSE)
            {
                return FIGMA_FALSE;
            }
        }
    }

    __figma_canvas_resolve_geometry_blobs(decoder);
    __figma_canvas_resolve_paint_style_references(decoder);
    __figma_canvas_resolve_path_style_paints(decoder);

    if(decoder->failed == FIGMA_TRUE)
    {
        return FIGMA_FALSE;
    }

    return __figma_canvas_build_document_tree(
        decoder, root, prototype_start_node_id);
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_canvas_decode_document_internal(figma_memory_t * memory, const figma_kiwi_schema_t * schema, const figma_array_t * encoded_data, figma_document_t * document)
{
    figma_canvas_decoder_t decoder;
    figma_kiwi_reader_t reader;
    figma_canvas_node_t root;
    figma_string_t prototype_start_node_id;
    figma_bool_t success;

    if(memory == NULL || schema == NULL || encoded_data == NULL ||
        document == NULL)
    {
        return FIGMA_FALSE;
    }

    __figma_canvas_decoder_init(&decoder, memory, schema);
    figma_kiwi_reader_init(
        &reader,
        (const uint8_t *)encoded_data->data,
        encoded_data->size);
    figma_canvas_node_init(&root);
    figma_string_init(&prototype_start_node_id);

    success = __figma_canvas_decode_message(
        &decoder, &reader, &root, &prototype_start_node_id);

    if(success == FIGMA_TRUE &&
        decoder.failed == FIGMA_FALSE &&
        reader.failed == FIGMA_FALSE)
    {
        figma_canvas_node_destroy(memory, &document->canvas_root);
        document->canvas_root = root;
        figma_canvas_node_init(&root);
        figma_string_destroy(memory, &document->prototype_start_node_id);
        document->prototype_start_node_id = prototype_start_node_id;
        figma_string_init(&prototype_start_node_id);
        document->has_canvas_root = FIGMA_TRUE;
    }
    else
    {
        success = FIGMA_FALSE;
    }

    figma_string_destroy(memory, &prototype_start_node_id);
    figma_canvas_node_destroy(memory, &root);
    __figma_canvas_decoder_destroy(&decoder);
    return success;
}

//////////////////////////////////////////////////////////////////////////
static figma_canvas_node_type_t __figma_canvas_node_type_from_string(figma_string_view_t value)
{
    if(figma_string_view_equal_cstr(value, "DOCUMENT") == FIGMA_TRUE)
    {
        return FIGMA_CANVAS_NODE_DOCUMENT;
    }
    if(figma_string_view_equal_cstr(value, "CANVAS") == FIGMA_TRUE)
    {
        return FIGMA_CANVAS_NODE_CANVAS;
    }
    if(figma_string_view_equal_cstr(value, "FRAME") == FIGMA_TRUE ||
        figma_string_view_equal_cstr(value, "INSTANCE") == FIGMA_TRUE ||
        figma_string_view_equal_cstr(value, "SYMBOL") == FIGMA_TRUE)
    {
        return FIGMA_CANVAS_NODE_FRAME;
    }
    if(figma_string_view_equal_cstr(value, "GROUP") == FIGMA_TRUE)
    {
        return FIGMA_CANVAS_NODE_GROUP;
    }
    if(figma_string_view_equal_cstr(value, "RECTANGLE") == FIGMA_TRUE)
    {
        return FIGMA_CANVAS_NODE_RECTANGLE;
    }
    if(figma_string_view_equal_cstr(value, "ROUNDED_RECTANGLE") == FIGMA_TRUE)
    {
        return FIGMA_CANVAS_NODE_ROUNDED_RECTANGLE;
    }
    if(figma_string_view_equal_cstr(value, "ELLIPSE") == FIGMA_TRUE)
    {
        return FIGMA_CANVAS_NODE_ELLIPSE;
    }
    if(figma_string_view_equal_cstr(value, "TEXT") == FIGMA_TRUE)
    {
        return FIGMA_CANVAS_NODE_TEXT;
    }
    if(figma_string_view_equal_cstr(value, "VECTOR") == FIGMA_TRUE)
    {
        return FIGMA_CANVAS_NODE_VECTOR;
    }
    return FIGMA_CANVAS_NODE_UNKNOWN;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_node_change(figma_canvas_decoder_t * decoder, figma_kiwi_reader_t * reader, figma_canvas_node_record_t * record)
{
    const figma_kiwi_definition_t * definition =
        __figma_canvas_definition(decoder, reader, "NodeChange");

    if(definition == NULL)
    {
        return FIGMA_FALSE;
    }

    for(;;)
    {
        const uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }
        if(tag == 0u)
        {
            break;
        }

        field = figma_kiwi_find_field(definition, tag);

        if(field == NULL)
        {
            reader->failed = FIGMA_TRUE;
            decoder->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }

        if(__figma_canvas_field_is(field, "guid") == FIGMA_TRUE)
        {
            __figma_canvas_decode_guid(decoder, reader, &record->node.id);
        }
        else if(__figma_canvas_field_is(field, "parentIndex") == FIGMA_TRUE)
        {
            figma_canvas_parent_index_t parent;
            __figma_canvas_parent_index_init(&parent);

            if(__figma_canvas_decode_parent_index(
                   decoder, reader, &parent) == FIGMA_TRUE)
            {
                __figma_canvas_string_set(
                    decoder, &record->parent_id, &parent.id);
                __figma_canvas_string_set(
                    decoder, &record->position, &parent.position);
            }

            __figma_canvas_parent_index_destroy(
                decoder->memory, &parent);
        }
        else if(__figma_canvas_field_is(field, "type") == FIGMA_TRUE)
        {
            figma_string_t value;
            figma_string_init(&value);
            __figma_canvas_read_enum_value(decoder, reader, field, &value);
            record->node.type = __figma_canvas_node_type_from_string(
                figma_string_view(&value));
            figma_string_destroy(decoder->memory, &value);
        }
        else if(__figma_canvas_field_is(field, "name") == FIGMA_TRUE)
        {
            if(figma_kiwi_read_string(
                   decoder->memory, reader, &record->node.name) == FIGMA_FALSE)
            {
                decoder->failed = FIGMA_TRUE;
            }
        }
        else if(__figma_canvas_field_is(field, "styleID") == FIGMA_TRUE)
        {
            record->style_id = figma_kiwi_read_var_uint(reader);
        }
        else if(__figma_canvas_field_is(field, "isFillStyle") == FIGMA_TRUE)
        {
            record->fill_style =
                figma_kiwi_read_byte(reader) != 0u ? FIGMA_TRUE : FIGMA_FALSE;
        }
        else if(__figma_canvas_field_is(field, "isStrokeStyle") == FIGMA_TRUE)
        {
            record->stroke_style =
                figma_kiwi_read_byte(reader) != 0u ? FIGMA_TRUE : FIGMA_FALSE;
        }
        else if(__figma_canvas_field_is(field, "styleType") == FIGMA_TRUE)
        {
            figma_string_t value;
            figma_string_init(&value);
            __figma_canvas_read_enum_value(decoder, reader, field, &value);
            if(figma_string_equal_view(
                   &value, figma_string_view_cstr("FILL")) == FIGMA_TRUE)
            {
                record->fill_style = FIGMA_TRUE;
            }
            if(figma_string_equal_view(
                   &value, figma_string_view_cstr("STROKE")) == FIGMA_TRUE)
            {
                record->stroke_style = FIGMA_TRUE;
            }
            figma_string_destroy(decoder->memory, &value);
        }
        else if(__figma_canvas_field_is(field, "styleIdForFill") == FIGMA_TRUE)
        {
            __figma_canvas_decode_style_id(
                decoder, reader, &record->node.fill_style_node_id);
        }
        else if(__figma_canvas_field_is(
                    field, "styleIdForStrokeFill") == FIGMA_TRUE)
        {
            __figma_canvas_decode_style_id(
                decoder,
                reader,
                &record->node.stroke_fill_style_node_id);
        }
        else if(__figma_canvas_field_is(field, "visible") == FIGMA_TRUE)
        {
            record->node.visible =
                figma_kiwi_read_byte(reader) != 0u ? FIGMA_TRUE : FIGMA_FALSE;
        }
        else if(__figma_canvas_field_is(field, "opacity") == FIGMA_TRUE)
        {
            record->node.opacity = figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(field, "blendMode") == FIGMA_TRUE)
        {
            __figma_canvas_read_enum_value(
                decoder,
                reader,
                field,
                &record->node.raw_blend_mode);
            record->node.blend_mode = __figma_canvas_blend_mode_from_string(
                figma_string_view(&record->node.raw_blend_mode));
        }
        else if(__figma_canvas_field_is(field, "size") == FIGMA_TRUE)
        {
            record->size = __figma_canvas_decode_vector(decoder, reader);
        }
        else if(__figma_canvas_field_is(field, "transform") == FIGMA_TRUE)
        {
            record->transform = __figma_canvas_decode_matrix(decoder, reader);
        }
        else if(__figma_canvas_field_is(field, "cornerRadius") == FIGMA_TRUE)
        {
            record->node.corner_radius =
                figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(field, "strokeWeight") == FIGMA_TRUE)
        {
            record->node.stroke_weight =
                figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(field, "strokeAlign") == FIGMA_TRUE)
        {
            figma_string_t value;
            figma_string_init(&value);
            __figma_canvas_read_enum_value(decoder, reader, field, &value);

            if(figma_string_equal_view(
                   &value, figma_string_view_cstr("CENTER")) == FIGMA_TRUE)
            {
                record->node.stroke_align =
                    FIGMA_CANVAS_STROKE_ALIGN_CENTER;
            }
            else if(figma_string_equal_view(
                        &value,
                        figma_string_view_cstr("INSIDE")) == FIGMA_TRUE)
            {
                record->node.stroke_align =
                    FIGMA_CANVAS_STROKE_ALIGN_INSIDE;
            }
            else if(figma_string_equal_view(
                        &value,
                        figma_string_view_cstr("OUTSIDE")) == FIGMA_TRUE)
            {
                record->node.stroke_align =
                    FIGMA_CANVAS_STROKE_ALIGN_OUTSIDE;
            }
            else
            {
                record->node.stroke_align =
                    FIGMA_CANVAS_STROKE_ALIGN_UNSUPPORTED;
            }

            figma_string_destroy(decoder->memory, &value);
        }
        else if(__figma_canvas_field_is(field, "strokeCap") == FIGMA_TRUE)
        {
            figma_string_t value;
            figma_string_init(&value);
            __figma_canvas_read_enum_value(decoder, reader, field, &value);

            if(figma_string_equal_view(
                   &value, figma_string_view_cstr("NONE")) == FIGMA_TRUE)
            {
                record->node.stroke_cap = FIGMA_CANVAS_STROKE_CAP_NONE;
            }
            else if(figma_string_equal_view(
                        &value,
                        figma_string_view_cstr("ROUND")) == FIGMA_TRUE)
            {
                record->node.stroke_cap = FIGMA_CANVAS_STROKE_CAP_ROUND;
            }
            else if(figma_string_equal_view(
                        &value,
                        figma_string_view_cstr("SQUARE")) == FIGMA_TRUE)
            {
                record->node.stroke_cap = FIGMA_CANVAS_STROKE_CAP_SQUARE;
            }
            else
            {
                record->node.stroke_cap =
                    FIGMA_CANVAS_STROKE_CAP_UNSUPPORTED;
            }

            figma_string_destroy(decoder->memory, &value);
        }
        else if(__figma_canvas_field_is(field, "strokeJoin") == FIGMA_TRUE)
        {
            figma_string_t value;
            figma_string_init(&value);
            __figma_canvas_read_enum_value(decoder, reader, field, &value);

            if(figma_string_equal_view(
                   &value, figma_string_view_cstr("MITER")) == FIGMA_TRUE)
            {
                record->node.stroke_join =
                    FIGMA_CANVAS_STROKE_JOIN_MITER;
            }
            else if(figma_string_equal_view(
                        &value,
                        figma_string_view_cstr("BEVEL")) == FIGMA_TRUE)
            {
                record->node.stroke_join =
                    FIGMA_CANVAS_STROKE_JOIN_BEVEL;
            }
            else if(figma_string_equal_view(
                        &value,
                        figma_string_view_cstr("ROUND")) == FIGMA_TRUE)
            {
                record->node.stroke_join =
                    FIGMA_CANVAS_STROKE_JOIN_ROUND;
            }
            else
            {
                record->node.stroke_join =
                    FIGMA_CANVAS_STROKE_JOIN_UNSUPPORTED;
            }

            figma_string_destroy(decoder->memory, &value);
        }
        else if(__figma_canvas_field_is(field, "dashPattern") == FIGMA_TRUE)
        {
            __figma_canvas_decode_float_array(
                decoder, reader, &record->node.dash_pattern);
        }
        else if(__figma_canvas_field_is(field, "mask") == FIGMA_TRUE)
        {
            record->node.mask =
                figma_kiwi_read_byte(reader) != 0u ? FIGMA_TRUE : FIGMA_FALSE;
        }
        else if(__figma_canvas_field_is(field, "maskType") == FIGMA_TRUE)
        {
            figma_string_t value;
            figma_string_init(&value);
            __figma_canvas_read_enum_value(decoder, reader, field, &value);

            if(figma_string_equal_view(
                   &value, figma_string_view_cstr("ALPHA")) == FIGMA_TRUE)
            {
                record->node.mask_type = FIGMA_CANVAS_MASK_ALPHA;
            }
            else if(figma_string_equal_view(
                        &value,
                        figma_string_view_cstr("OUTLINE")) == FIGMA_TRUE)
            {
                record->node.mask_type = FIGMA_CANVAS_MASK_OUTLINE;
            }
            else if(figma_string_equal_view(
                        &value,
                        figma_string_view_cstr("LUMINANCE")) == FIGMA_TRUE)
            {
                record->node.mask_type = FIGMA_CANVAS_MASK_LUMINANCE;
            }
            else
            {
                record->node.mask_type = FIGMA_CANVAS_MASK_UNKNOWN;
            }

            figma_string_destroy(decoder->memory, &value);
        }
        else if(__figma_canvas_field_is(
                    field, "frameMaskDisabled") == FIGMA_TRUE)
        {
            record->node.frame_mask_disabled =
                figma_kiwi_read_byte(reader) != 0u ? FIGMA_TRUE : FIGMA_FALSE;
        }
        else if(__figma_canvas_field_is(
                    field, "rectangleTopLeftCornerRadius") == FIGMA_TRUE)
        {
            const float value = figma_kiwi_read_var_float(reader);
            if(value > record->node.corner_radius)
            {
                record->node.corner_radius = value;
            }
        }
        else if(__figma_canvas_field_is(field, "fontSize") == FIGMA_TRUE)
        {
            record->node.font_size = figma_kiwi_read_var_float(reader);
        }
        else if(__figma_canvas_field_is(field, "lineHeight") == FIGMA_TRUE)
        {
            const figma_canvas_number_t line_height =
                __figma_canvas_decode_number(decoder, reader);
            record->node.line_height =
                line_height.percent == FIGMA_TRUE ? 0.0f : line_height.value;
        }
        else if(__figma_canvas_field_is(field, "fontName") == FIGMA_TRUE)
        {
            figma_canvas_font_name_t font_name;
            __figma_canvas_font_name_init(&font_name);

            if(__figma_canvas_decode_font_name(
                   decoder, reader, &font_name) == FIGMA_TRUE)
            {
                __figma_canvas_string_set(
                    decoder, &record->node.font_family, &font_name.family);
                __figma_canvas_string_set(
                    decoder, &record->node.font_style, &font_name.style);
                __figma_canvas_string_set(
                    decoder,
                    &record->node.font_postscript_name,
                    &font_name.postscript);
            }

            __figma_canvas_font_name_destroy(
                decoder->memory, &font_name);
        }
        else if(__figma_canvas_field_is(
                    field, "textAlignHorizontal") == FIGMA_TRUE)
        {
            figma_string_t value;
            figma_string_init(&value);
            __figma_canvas_read_enum_value(decoder, reader, field, &value);

            if(figma_string_equal_view(
                   &value, figma_string_view_cstr("CENTER")) == FIGMA_TRUE)
            {
                record->node.text_align_horizontal =
                    FIGMA_CANVAS_TEXT_ALIGN_HORIZONTAL_CENTER;
            }
            else if(figma_string_equal_view(
                        &value,
                        figma_string_view_cstr("RIGHT")) == FIGMA_TRUE)
            {
                record->node.text_align_horizontal =
                    FIGMA_CANVAS_TEXT_ALIGN_HORIZONTAL_RIGHT;
            }

            figma_string_destroy(decoder->memory, &value);
        }
        else if(__figma_canvas_field_is(
                    field, "textAlignVertical") == FIGMA_TRUE)
        {
            figma_string_t value;
            figma_string_init(&value);
            __figma_canvas_read_enum_value(decoder, reader, field, &value);

            if(figma_string_equal_view(
                   &value, figma_string_view_cstr("CENTER")) == FIGMA_TRUE)
            {
                record->node.text_align_vertical =
                    FIGMA_CANVAS_TEXT_ALIGN_VERTICAL_CENTER;
            }
            else if(figma_string_equal_view(
                        &value,
                        figma_string_view_cstr("BOTTOM")) == FIGMA_TRUE)
            {
                record->node.text_align_vertical =
                    FIGMA_CANVAS_TEXT_ALIGN_VERTICAL_BOTTOM;
            }

            figma_string_destroy(decoder->memory, &value);
        }
        else if(__figma_canvas_field_is(field, "textData") == FIGMA_TRUE)
        {
            __figma_canvas_decode_text_data(
                decoder, reader, &record->node);
        }
        else if(__figma_canvas_field_is(field, "derivedTextData") == FIGMA_TRUE)
        {
            __figma_canvas_decode_derived_text_data(
                decoder, reader, &record->node);
        }
        else if(__figma_canvas_field_is(field, "fillPaints") == FIGMA_TRUE ||
            __figma_canvas_field_is(field, "backgroundPaints") == FIGMA_TRUE)
        {
            __figma_canvas_decode_paint_array(
                decoder, reader, &record->node.fills);
        }
        else if(__figma_canvas_field_is(field, "strokePaints") == FIGMA_TRUE)
        {
            __figma_canvas_decode_paint_array(
                decoder, reader, &record->node.strokes);
        }
        else if(__figma_canvas_field_is(field, "fillGeometry") == FIGMA_TRUE)
        {
            record->node.has_fill_geometry = FIGMA_TRUE;
            __figma_canvas_decode_path_array(
                decoder, reader, &record->node.fill_geometry);
        }
        else if(__figma_canvas_field_is(field, "strokeGeometry") == FIGMA_TRUE)
        {
            record->node.has_stroke_geometry = FIGMA_TRUE;
            __figma_canvas_decode_path_array(
                decoder, reader, &record->node.stroke_geometry);
        }
        else if(__figma_canvas_field_is(field, "vectorData") == FIGMA_TRUE)
        {
            __figma_canvas_decode_vector_data(
                decoder, reader, &record->node);
        }
        else if(__figma_canvas_field_is(field, "arcData") == FIGMA_TRUE)
        {
            record->node.arc_data =
                __figma_canvas_decode_arc_data(decoder, reader);
        }
        else if(__figma_canvas_field_is(field, "pathTrim") == FIGMA_TRUE)
        {
            __figma_canvas_skip(decoder, reader, field);
        }
        else if(__figma_canvas_field_is(
                    field, "prototypeStartNodeID") == FIGMA_TRUE)
        {
            __figma_canvas_decode_guid(
                decoder,
                reader,
                &record->node.prototype_start_node_id);
        }
        else if(__figma_canvas_field_is(
                    field, "prototypeStartingPoint") == FIGMA_TRUE)
        {
            record->node.has_prototype_starting_point = FIGMA_TRUE;
            __figma_canvas_skip(decoder, reader, field);
        }
        else if(__figma_canvas_field_is(
                    field, "prototypeInteractions") == FIGMA_TRUE)
        {
            const uint32_t count = figma_kiwi_read_var_uint(reader);
            uint32_t index;

            if(UINT32_MAX - record->node.prototype_interaction_count < count ||
                figma_array_reserve(
                    decoder->memory,
                    &record->node.prototype_interactions,
                    record->node.prototype_interactions.size + count) ==
                    FIGMA_FALSE)
            {
                decoder->failed = FIGMA_TRUE;
                return FIGMA_FALSE;
            }

            record->node.prototype_interaction_count += count;

            for(index = 0u; index != count; ++index)
            {
                figma_prototype_interaction_t * interaction =
                    (figma_prototype_interaction_t *)
                        figma_array_push_uninitialized(
                            decoder->memory,
                            &record->node.prototype_interactions);

                if(interaction == NULL)
                {
                    decoder->failed = FIGMA_TRUE;
                    return FIGMA_FALSE;
                }

                figma_prototype_interaction_init(interaction);

                if(__figma_canvas_decode_prototype_interaction(
                       decoder, reader, interaction) == FIGMA_FALSE)
                {
                    return FIGMA_FALSE;
                }
            }
        }
        else if(__figma_canvas_field_is(field, "symbolData") == FIGMA_TRUE)
        {
            __figma_canvas_decode_symbol_data(
                decoder, reader, &record->symbol_id);
        }
        else
        {
            __figma_canvas_skip(decoder, reader, field);
        }

        if(decoder->failed == FIGMA_TRUE || reader->failed == FIGMA_TRUE)
        {
            return FIGMA_FALSE;
        }
    }

    record->node.rect.x = 0.0f;
    record->node.rect.y = 0.0f;
    record->node.rect.w = record->size.x;
    record->node.rect.h = record->size.y;
    return FIGMA_TRUE;
}

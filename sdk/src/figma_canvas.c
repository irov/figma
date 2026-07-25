#include "figma_canvas.h"

#include <limits.h>
#include <string.h>

#include <zlib.h>
#define ZSTD_STATIC_LINKING_ONLY
#include <zstd.h>

typedef struct figma_canvas_chunk
{
    size_t offset;
    size_t size;
} figma_canvas_chunk_t;

typedef struct figma_kiwi_raw_field
{
    figma_string_t name;
    int32_t type;
    figma_bool_t array;
    uint32_t value;
} figma_kiwi_raw_field_t;

typedef struct figma_kiwi_raw_definition
{
    figma_array_t fields;
} figma_kiwi_raw_definition_t;

//////////////////////////////////////////////////////////////////////////
static void __figma_kiwi_field_destroy(figma_memory_t * memory, void * value)
{
    figma_kiwi_field_t * field = (figma_kiwi_field_t *)value;
    figma_string_destroy(memory, &field->name);
    figma_string_destroy(memory, &field->type);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_kiwi_definition_destroy(figma_memory_t * memory, void * value)
{
    figma_kiwi_definition_t * definition = (figma_kiwi_definition_t *)value;
    figma_string_destroy(memory, &definition->name);
    figma_array_destroy(memory, &definition->fields, &__figma_kiwi_field_destroy);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_kiwi_raw_field_destroy(figma_memory_t * memory, void * value)
{
    figma_kiwi_raw_field_t * field = (figma_kiwi_raw_field_t *)value;
    figma_string_destroy(memory, &field->name);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_kiwi_raw_definition_destroy(figma_memory_t * memory, void * value)
{
    figma_kiwi_raw_definition_t * definition =
        (figma_kiwi_raw_definition_t *)value;
    figma_array_destroy(
        memory, &definition->fields, &__figma_kiwi_raw_field_destroy);
}

//////////////////////////////////////////////////////////////////////////
void figma_kiwi_schema_init(figma_kiwi_schema_t * schema, figma_memory_t * memory)
{
    memset(schema, 0, sizeof(*schema));
    schema->memory = memory;
    figma_array_init(&schema->definitions, sizeof(figma_kiwi_definition_t));
}

//////////////////////////////////////////////////////////////////////////
void figma_kiwi_schema_destroy(figma_kiwi_schema_t * schema)
{
    if(schema == NULL)
    {
        return;
    }

    figma_array_destroy(
        schema->memory, &schema->definitions, &__figma_kiwi_definition_destroy);
    schema->memory = NULL;
}

//////////////////////////////////////////////////////////////////////////
const figma_kiwi_definition_t * figma_kiwi_find_definition(const figma_kiwi_schema_t * schema, figma_string_view_t name)
{
    size_t index;

    if(schema == NULL)
    {
        return NULL;
    }

    for(index = 0u; index != schema->definitions.size; ++index)
    {
        const figma_kiwi_definition_t * definition =
            FIGMA_ARRAY_CONST_PTR(
                figma_kiwi_definition_t, &schema->definitions, index);

        if(figma_string_equal_view(&definition->name, name) == FIGMA_TRUE)
        {
            return definition;
        }
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////
const figma_kiwi_field_t * figma_kiwi_find_field(const figma_kiwi_definition_t * definition, uint32_t value)
{
    size_t index;

    if(definition == NULL)
    {
        return NULL;
    }

    for(index = 0u; index != definition->fields.size; ++index)
    {
        const figma_kiwi_field_t * field =
            FIGMA_ARRAY_CONST_PTR(figma_kiwi_field_t, &definition->fields, index);

        if(field->value == value)
        {
            return field;
        }
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////
void figma_kiwi_reader_init(figma_kiwi_reader_t * reader, const uint8_t * data, size_t size)
{
    reader->data = data;
    reader->size = size;
    reader->offset = 0u;
    reader->failed = data == NULL && size != 0u ? FIGMA_TRUE : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_kiwi_reader_eof(const figma_kiwi_reader_t * reader)
{
    return reader == NULL || reader->failed == FIGMA_TRUE ||
            reader->offset >= reader->size
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_kiwi_can_read(figma_kiwi_reader_t * reader, size_t size)
{
    if(reader == NULL || reader->failed == FIGMA_TRUE ||
        reader->offset > reader->size || size > reader->size - reader->offset)
    {
        if(reader != NULL)
        {
            reader->failed = FIGMA_TRUE;
        }

        return FIGMA_FALSE;
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
uint8_t figma_kiwi_read_byte(figma_kiwi_reader_t * reader)
{
    if(__figma_kiwi_can_read(reader, 1u) == FIGMA_FALSE)
    {
        return 0u;
    }

    return reader->data[reader->offset++];
}

//////////////////////////////////////////////////////////////////////////
uint32_t figma_kiwi_read_var_uint(figma_kiwi_reader_t * reader)
{
    uint32_t value = 0u;
    uint32_t shift = 0u;

    while(shift < 35u)
    {
        const uint8_t byte = figma_kiwi_read_byte(reader);

        if(reader->failed == FIGMA_TRUE ||
            (shift == 28u && (byte & 0xf0u) != 0u))
        {
            reader->failed = FIGMA_TRUE;
            return 0u;
        }

        value |= (uint32_t)(byte & 0x7fu) << shift;

        if((byte & 0x80u) == 0u)
        {
            return value;
        }

        shift += 7u;
    }

    reader->failed = FIGMA_TRUE;
    return 0u;
}

//////////////////////////////////////////////////////////////////////////
int32_t figma_kiwi_read_var_int(figma_kiwi_reader_t * reader)
{
    const uint32_t value = figma_kiwi_read_var_uint(reader);
    return (value & 1u) != 0u
        ? ~(int32_t)(value >> 1u)
        : (int32_t)(value >> 1u);
}

//////////////////////////////////////////////////////////////////////////
uint64_t figma_kiwi_read_var_uint64(figma_kiwi_reader_t * reader)
{
    uint64_t value = 0u;
    uint32_t shift = 0u;

    while(shift < 70u)
    {
        const uint8_t byte = figma_kiwi_read_byte(reader);

        if(reader->failed == FIGMA_TRUE ||
            (shift == 63u && (byte & 0xfeu) != 0u))
        {
            reader->failed = FIGMA_TRUE;
            return 0u;
        }

        value |= (uint64_t)(byte & 0x7fu) << shift;

        if((byte & 0x80u) == 0u)
        {
            return value;
        }

        shift += 7u;
    }

    reader->failed = FIGMA_TRUE;
    return 0u;
}

//////////////////////////////////////////////////////////////////////////
int64_t figma_kiwi_read_var_int64(figma_kiwi_reader_t * reader)
{
    const uint64_t value = figma_kiwi_read_var_uint64(reader);
    return (value & 1u) != 0u
        ? ~(int64_t)(value >> 1u)
        : (int64_t)(value >> 1u);
}

//////////////////////////////////////////////////////////////////////////
float figma_kiwi_read_var_float(figma_kiwi_reader_t * reader)
{
    uint32_t encoded;
    float value;

    if(__figma_kiwi_can_read(reader, 1u) == FIGMA_FALSE)
    {
        return 0.0f;
    }

    if(reader->data[reader->offset] == 0u)
    {
        ++reader->offset;
        return 0.0f;
    }

    if(__figma_kiwi_can_read(reader, 4u) == FIGMA_FALSE)
    {
        return 0.0f;
    }

    encoded =
        (uint32_t)reader->data[reader->offset] |
        ((uint32_t)reader->data[reader->offset + 1u] << 8u) |
        ((uint32_t)reader->data[reader->offset + 2u] << 16u) |
        ((uint32_t)reader->data[reader->offset + 3u] << 24u);
    reader->offset += 4u;
    encoded = (encoded << 23u) | (encoded >> 9u);
    memcpy(&value, &encoded, sizeof(value));

    return value;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_kiwi_append_codepoint(figma_memory_t * memory, figma_string_t * string, uint32_t codepoint)
{
    char encoded[4];
    size_t size;
    figma_string_view_t value;

    if(codepoint < 0x80u)
    {
        encoded[0] = (char)codepoint;
        size = 1u;
    }
    else if(codepoint < 0x800u)
    {
        encoded[0] = (char)(0xc0u | (codepoint >> 6u));
        encoded[1] = (char)(0x80u | (codepoint & 0x3fu));
        size = 2u;
    }
    else if(codepoint < 0x10000u)
    {
        if(codepoint >= 0xd800u && codepoint <= 0xdfffu)
        {
            return FIGMA_FALSE;
        }

        encoded[0] = (char)(0xe0u | (codepoint >> 12u));
        encoded[1] = (char)(0x80u | ((codepoint >> 6u) & 0x3fu));
        encoded[2] = (char)(0x80u | (codepoint & 0x3fu));
        size = 3u;
    }
    else if(codepoint <= 0x10ffffu)
    {
        encoded[0] = (char)(0xf0u | (codepoint >> 18u));
        encoded[1] = (char)(0x80u | ((codepoint >> 12u) & 0x3fu));
        encoded[2] = (char)(0x80u | ((codepoint >> 6u) & 0x3fu));
        encoded[3] = (char)(0x80u | (codepoint & 0x3fu));
        size = 4u;
    }
    else
    {
        return FIGMA_FALSE;
    }

    value.data = encoded;
    value.size = size;
    return figma_string_append(memory, string, value);
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_kiwi_read_string(figma_memory_t * memory, figma_kiwi_reader_t * reader, figma_string_t * string)
{
    figma_string_t temporary;

    if(memory == NULL || reader == NULL || string == NULL)
    {
        return FIGMA_FALSE;
    }

    figma_string_init(&temporary);

    for(;;)
    {
        uint32_t codepoint;
        uint8_t first;

        if(__figma_kiwi_can_read(reader, 1u) == FIGMA_FALSE)
        {
            break;
        }

        first = figma_kiwi_read_byte(reader);

        if(first < 0x80u)
        {
            codepoint = first;
        }
        else if(first < 0xe0u)
        {
            uint8_t second = figma_kiwi_read_byte(reader);
            codepoint =
                ((uint32_t)(first & 0x1fu) << 6u) |
                (uint32_t)(second & 0x3fu);
        }
        else if(first < 0xf0u)
        {
            uint8_t second = figma_kiwi_read_byte(reader);
            uint8_t third = figma_kiwi_read_byte(reader);
            codepoint =
                ((uint32_t)(first & 0x0fu) << 12u) |
                ((uint32_t)(second & 0x3fu) << 6u) |
                (uint32_t)(third & 0x3fu);
        }
        else
        {
            uint8_t second = figma_kiwi_read_byte(reader);
            uint8_t third = figma_kiwi_read_byte(reader);
            uint8_t fourth = figma_kiwi_read_byte(reader);
            codepoint =
                ((uint32_t)(first & 0x07u) << 18u) |
                ((uint32_t)(second & 0x3fu) << 12u) |
                ((uint32_t)(third & 0x3fu) << 6u) |
                (uint32_t)(fourth & 0x3fu);
        }

        if(reader->failed == FIGMA_TRUE)
        {
            break;
        }

        if(codepoint == 0u)
        {
            figma_string_destroy(memory, string);
            *string = temporary;
            return FIGMA_TRUE;
        }

        if(__figma_kiwi_append_codepoint(memory, &temporary, codepoint) == FIGMA_FALSE)
        {
            reader->failed = FIGMA_TRUE;
            break;
        }
    }

    figma_string_destroy(memory, &temporary);
    return FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_kiwi_read_byte_array(figma_memory_t * memory, figma_kiwi_reader_t * reader, figma_array_t * bytes)
{
    uint32_t size;
    figma_array_t temporary;

    if(memory == NULL || reader == NULL || bytes == NULL)
    {
        return FIGMA_FALSE;
    }

    size = figma_kiwi_read_var_uint(reader);

    if(reader->failed == FIGMA_TRUE ||
        __figma_kiwi_can_read(reader, size) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    figma_array_init(&temporary, sizeof(uint8_t));

    if(figma_array_resize(memory, &temporary, size, NULL) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    if(size != 0u)
    {
        memcpy(temporary.data, reader->data + reader->offset, size);
    }

    reader->offset += size;
    figma_array_destroy(memory, bytes, NULL);
    *bytes = temporary;
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_kiwi_skip_value_depth(figma_memory_t * memory, const figma_kiwi_schema_t * schema, figma_kiwi_reader_t * reader, figma_string_view_t type, figma_bool_t array, uint32_t depth)
{
    const figma_kiwi_definition_t * definition;
    size_t index;

    if(depth > 256u)
    {
        reader->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    if(array == FIGMA_TRUE)
    {
        uint32_t count;
        uint32_t item_index;

        if(figma_string_view_equal_cstr(type, "byte") == FIGMA_TRUE)
        {
            uint32_t size = figma_kiwi_read_var_uint(reader);
            if(reader->failed == FIGMA_TRUE ||
                __figma_kiwi_can_read(reader, size) == FIGMA_FALSE)
            {
                return FIGMA_FALSE;
            }

            reader->offset += size;
            return FIGMA_TRUE;
        }

        count = figma_kiwi_read_var_uint(reader);

        for(item_index = 0u;
            item_index != count && reader->failed == FIGMA_FALSE;
            ++item_index)
        {
            if(__figma_kiwi_skip_value_depth(
                   memory, schema, reader, type, FIGMA_FALSE, depth + 1u) ==
                FIGMA_FALSE)
            {
                return FIGMA_FALSE;
            }
        }

        return reader->failed == FIGMA_FALSE ? FIGMA_TRUE : FIGMA_FALSE;
    }

    if(figma_string_view_equal_cstr(type, "bool") == FIGMA_TRUE ||
        figma_string_view_equal_cstr(type, "byte") == FIGMA_TRUE)
    {
        (void)figma_kiwi_read_byte(reader);
        return reader->failed == FIGMA_FALSE ? FIGMA_TRUE : FIGMA_FALSE;
    }

    if(figma_string_view_equal_cstr(type, "int") == FIGMA_TRUE)
    {
        (void)figma_kiwi_read_var_int(reader);
        return reader->failed == FIGMA_FALSE ? FIGMA_TRUE : FIGMA_FALSE;
    }

    if(figma_string_view_equal_cstr(type, "uint") == FIGMA_TRUE)
    {
        (void)figma_kiwi_read_var_uint(reader);
        return reader->failed == FIGMA_FALSE ? FIGMA_TRUE : FIGMA_FALSE;
    }

    if(figma_string_view_equal_cstr(type, "float") == FIGMA_TRUE)
    {
        (void)figma_kiwi_read_var_float(reader);
        return reader->failed == FIGMA_FALSE ? FIGMA_TRUE : FIGMA_FALSE;
    }

    if(figma_string_view_equal_cstr(type, "string") == FIGMA_TRUE)
    {
        figma_string_t ignored;
        figma_bool_t result;
        figma_string_init(&ignored);
        result = figma_kiwi_read_string(memory, reader, &ignored);
        figma_string_destroy(memory, &ignored);
        return result;
    }

    if(figma_string_view_equal_cstr(type, "int64") == FIGMA_TRUE)
    {
        (void)figma_kiwi_read_var_int64(reader);
        return reader->failed == FIGMA_FALSE ? FIGMA_TRUE : FIGMA_FALSE;
    }

    if(figma_string_view_equal_cstr(type, "uint64") == FIGMA_TRUE)
    {
        (void)figma_kiwi_read_var_uint64(reader);
        return reader->failed == FIGMA_FALSE ? FIGMA_TRUE : FIGMA_FALSE;
    }

    definition = figma_kiwi_find_definition(schema, type);

    if(definition == NULL)
    {
        reader->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    if(definition->kind == FIGMA_KIWI_DEFINITION_ENUM)
    {
        (void)figma_kiwi_read_var_uint(reader);
        return reader->failed == FIGMA_FALSE ? FIGMA_TRUE : FIGMA_FALSE;
    }

    if(definition->kind == FIGMA_KIWI_DEFINITION_STRUCT)
    {
        for(index = 0u; index != definition->fields.size; ++index)
        {
            const figma_kiwi_field_t * field =
                FIGMA_ARRAY_CONST_PTR(
                    figma_kiwi_field_t, &definition->fields, index);

            if(__figma_kiwi_skip_value_depth(
                   memory,
                   schema,
                   reader,
                   figma_string_view(&field->type),
                   field->array,
                   depth + 1u) == FIGMA_FALSE)
            {
                return FIGMA_FALSE;
            }
        }

        return FIGMA_TRUE;
    }

    for(;;)
    {
        uint32_t tag = figma_kiwi_read_var_uint(reader);
        const figma_kiwi_field_t * field;

        if(reader->failed == FIGMA_TRUE)
        {
            return FIGMA_FALSE;
        }

        if(tag == 0u)
        {
            return FIGMA_TRUE;
        }

        field = figma_kiwi_find_field(definition, tag);

        if(field == NULL ||
            __figma_kiwi_skip_value_depth(
                memory,
                schema,
                reader,
                figma_string_view(&field->type),
                field->array,
                depth + 1u) == FIGMA_FALSE)
        {
            reader->failed = FIGMA_TRUE;
            return FIGMA_FALSE;
        }
    }
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_kiwi_skip_value(figma_memory_t * memory, const figma_kiwi_schema_t * schema, figma_kiwi_reader_t * reader, figma_string_view_t type, figma_bool_t array)
{
    return __figma_kiwi_skip_value_depth(
        memory, schema, reader, type, array, 0u);
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_kiwi_read_enum(figma_memory_t * memory, const figma_kiwi_schema_t * schema, figma_kiwi_reader_t * reader, figma_string_view_t type, figma_string_t * value)
{
    const uint32_t enum_value = figma_kiwi_read_var_uint(reader);
    const figma_kiwi_definition_t * definition =
        figma_kiwi_find_definition(schema, type);
    size_t index;

    if(reader->failed == FIGMA_TRUE || definition == NULL ||
        definition->kind != FIGMA_KIWI_DEFINITION_ENUM)
    {
        reader->failed = FIGMA_TRUE;
        return FIGMA_FALSE;
    }

    for(index = 0u; index != definition->fields.size; ++index)
    {
        const figma_kiwi_field_t * field =
            FIGMA_ARRAY_CONST_PTR(figma_kiwi_field_t, &definition->fields, index);

        if(field->value == enum_value)
        {
            return figma_string_assign(
                memory, value, figma_string_view(&field->name));
        }
    }

    figma_string_clear(value);
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_kiwi_is_enum(const figma_kiwi_schema_t * schema, figma_string_view_t type)
{
    const figma_kiwi_definition_t * definition =
        figma_kiwi_find_definition(schema, type);
    return definition != NULL &&
            definition->kind == FIGMA_KIWI_DEFINITION_ENUM
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static uint32_t __figma_canvas_read_u32(const uint8_t * bytes)
{
    return (uint32_t)bytes[0] |
        ((uint32_t)bytes[1] << 8u) |
        ((uint32_t)bytes[2] << 16u) |
        ((uint32_t)bytes[3] << 24u);
}

//////////////////////////////////////////////////////////////////////////
static voidpf __figma_canvas_zalloc(voidpf opaque, uInt items, uInt size)
{
    size_t bytes;
    if(figma_size_mul(items, size, &bytes) == FIGMA_FALSE)
    {
        return NULL;
    }
    return figma_memory_allocate((figma_memory_t *)opaque, bytes);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_zfree(voidpf opaque, voidpf address)
{
    figma_memory_deallocate((figma_memory_t *)opaque, address);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_inflate_raw(figma_memory_t * memory, const uint8_t * data, size_t size, figma_array_t * output)
{
    z_stream stream;
    int result;

    if(size > UINT_MAX)
    {
        return FIGMA_FALSE;
    }

    figma_array_clear(memory, output, NULL);
    memset(&stream, 0, sizeof(stream));
    stream.zalloc = &__figma_canvas_zalloc;
    stream.zfree = &__figma_canvas_zfree;
    stream.opaque = memory;
    stream.next_in = (Bytef *)(uintptr_t)data;
    stream.avail_in = (uInt)size;

    if(inflateInit2(&stream, -MAX_WBITS) != Z_OK)
    {
        return FIGMA_FALSE;
    }

    result = Z_OK;

    while(result != Z_STREAM_END)
    {
        const size_t old_size = output->size;
        size_t new_size;
        size_t available;

        if(figma_size_add(old_size, 65536u, &new_size) == FIGMA_FALSE ||
            figma_array_resize(memory, output, new_size, NULL) == FIGMA_FALSE)
        {
            inflateEnd(&stream);
            figma_array_clear(memory, output, NULL);
            return FIGMA_FALSE;
        }

        stream.next_out = (Bytef *)output->data + old_size;
        stream.avail_out = 65536u;
        result = inflate(&stream, Z_NO_FLUSH);

        if(result != Z_OK && result != Z_STREAM_END)
        {
            inflateEnd(&stream);
            figma_array_clear(memory, output, NULL);
            return FIGMA_FALSE;
        }

        available = 65536u - stream.avail_out;
        output->size = old_size + available;
    }

    inflateEnd(&stream);
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static void * __figma_canvas_zstd_allocate(void * opaque, size_t size)
{
    return figma_memory_allocate((figma_memory_t *)opaque, size);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_zstd_deallocate(void * opaque, void * address)
{
    figma_memory_deallocate((figma_memory_t *)opaque, address);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decompress_zstd(figma_memory_t * memory, const uint8_t * data, size_t size, figma_array_t * output)
{
    const unsigned long long content_size =
        ZSTD_getFrameContentSize(data, size);
    ZSTD_customMem custom_memory;
    ZSTD_DCtx * context;
    size_t result;

    if(content_size == ZSTD_CONTENTSIZE_ERROR ||
        content_size == ZSTD_CONTENTSIZE_UNKNOWN ||
        content_size > SIZE_MAX ||
        figma_array_resize(memory, output, (size_t)content_size, NULL) ==
            FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    custom_memory.customAlloc = &__figma_canvas_zstd_allocate;
    custom_memory.customFree = &__figma_canvas_zstd_deallocate;
    custom_memory.opaque = memory;
    context = ZSTD_createDCtx_advanced(custom_memory);

    if(context == NULL)
    {
        figma_array_clear(memory, output, NULL);
        return FIGMA_FALSE;
    }

    result =
        ZSTD_decompressDCtx(context, output->data, output->size, data, size);
    ZSTD_freeDCtx(context);

    if(ZSTD_isError(result) != 0 || result != output->size)
    {
        figma_array_clear(memory, output, NULL);
        return FIGMA_FALSE;
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decompress_chunk(figma_memory_t * memory, const uint8_t * data, size_t size, figma_array_t * output)
{
    if(__figma_canvas_inflate_raw(memory, data, size, output) == FIGMA_TRUE)
    {
        return FIGMA_TRUE;
    }

    return __figma_canvas_decompress_zstd(memory, data, size, output);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_read_chunks(figma_memory_t * memory, const figma_array_t * bytes, figma_array_t * chunks)
{
    const uint8_t * data = (const uint8_t *)bytes->data;
    size_t offset;

    if(bytes->size < 16u || memcmp(data, "fig-kiwi", 8u) != 0)
    {
        return FIGMA_FALSE;
    }

    offset = 12u;

    while(offset < bytes->size)
    {
        uint32_t chunk_size;
        figma_canvas_chunk_t chunk;

        if(bytes->size - offset < 4u)
        {
            return FIGMA_FALSE;
        }

        chunk_size = __figma_canvas_read_u32(data + offset);
        offset += 4u;

        if(chunk_size > bytes->size - offset)
        {
            return FIGMA_FALSE;
        }

        chunk.offset = offset;
        chunk.size = chunk_size;

        if(figma_array_push_copy(memory, chunks, &chunk) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }

        offset += chunk_size;
    }

    return chunks->size >= 2u ? FIGMA_TRUE : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_decode_schema(figma_memory_t * memory, const figma_array_t * bytes, figma_kiwi_schema_t * schema)
{
    static const char * primitive_types[] = {
        "bool", "byte", "int", "uint", "float", "string", "int64", "uint64"};
    figma_kiwi_reader_t reader;
    figma_array_t raw_definitions;
    uint32_t definition_count;
    uint32_t definition_index;
    figma_bool_t success = FIGMA_FALSE;

    figma_kiwi_reader_init(
        &reader, (const uint8_t *)bytes->data, bytes->size);
    figma_array_init(
        &raw_definitions, sizeof(figma_kiwi_raw_definition_t));
    definition_count = figma_kiwi_read_var_uint(&reader);

    if(reader.failed == FIGMA_TRUE ||
        figma_array_reserve(
            memory, &schema->definitions, definition_count) == FIGMA_FALSE ||
        figma_array_reserve(
            memory, &raw_definitions, definition_count) == FIGMA_FALSE)
    {
        goto cleanup;
    }

    for(definition_index = 0u;
        definition_index != definition_count;
        ++definition_index)
    {
        figma_kiwi_definition_t * definition;
        figma_kiwi_raw_definition_t * raw_definition;
        uint8_t raw_kind;
        uint32_t field_count;
        uint32_t field_index;

        definition = (figma_kiwi_definition_t *)
            figma_array_push_uninitialized(memory, &schema->definitions);
        raw_definition = (figma_kiwi_raw_definition_t *)
            figma_array_push_uninitialized(memory, &raw_definitions);

        if(definition == NULL || raw_definition == NULL)
        {
            goto cleanup;
        }

        memset(definition, 0, sizeof(*definition));
        memset(raw_definition, 0, sizeof(*raw_definition));
        figma_string_init(&definition->name);
        figma_array_init(&definition->fields, sizeof(figma_kiwi_field_t));
        figma_array_init(
            &raw_definition->fields, sizeof(figma_kiwi_raw_field_t));

        if(figma_kiwi_read_string(
               memory, &reader, &definition->name) == FIGMA_FALSE)
        {
            goto cleanup;
        }

        raw_kind = figma_kiwi_read_byte(&reader);

        if(raw_kind > 2u)
        {
            reader.failed = FIGMA_TRUE;
            goto cleanup;
        }

        definition->kind = (figma_kiwi_definition_kind_t)raw_kind;
        field_count = figma_kiwi_read_var_uint(&reader);

        if(reader.failed == FIGMA_TRUE ||
            figma_array_reserve(
                memory, &definition->fields, field_count) == FIGMA_FALSE ||
            figma_array_reserve(
                memory, &raw_definition->fields, field_count) == FIGMA_FALSE)
        {
            goto cleanup;
        }

        for(field_index = 0u; field_index != field_count; ++field_index)
        {
            figma_kiwi_field_t * field = (figma_kiwi_field_t *)
                figma_array_push_uninitialized(memory, &definition->fields);
            figma_kiwi_raw_field_t * raw_field =
                (figma_kiwi_raw_field_t *)figma_array_push_uninitialized(
                    memory, &raw_definition->fields);

            if(field == NULL || raw_field == NULL)
            {
                goto cleanup;
            }

            memset(field, 0, sizeof(*field));
            memset(raw_field, 0, sizeof(*raw_field));
            figma_string_init(&field->name);
            figma_string_init(&field->type);
            figma_string_init(&raw_field->name);

            if(figma_kiwi_read_string(
                   memory, &reader, &raw_field->name) == FIGMA_FALSE ||
                figma_string_copy(
                    memory, &field->name, &raw_field->name) == FIGMA_FALSE)
            {
                goto cleanup;
            }

            raw_field->type = figma_kiwi_read_var_int(&reader);
            raw_field->array =
                (figma_kiwi_read_byte(&reader) & 1u) != 0u
                ? FIGMA_TRUE
                : FIGMA_FALSE;
            raw_field->value = figma_kiwi_read_var_uint(&reader);
            field->array = raw_field->array;
            field->value = raw_field->value;

            if(reader.failed == FIGMA_TRUE)
            {
                goto cleanup;
            }
        }
    }

    for(definition_index = 0u;
        definition_index != definition_count;
        ++definition_index)
    {
        figma_kiwi_definition_t * definition =
            FIGMA_ARRAY_PTR(
                figma_kiwi_definition_t,
                &schema->definitions,
                definition_index);
        figma_kiwi_raw_definition_t * raw_definition =
            FIGMA_ARRAY_PTR(
                figma_kiwi_raw_definition_t,
                &raw_definitions,
                definition_index);
        size_t field_index;

        for(field_index = 0u;
            field_index != definition->fields.size;
            ++field_index)
        {
            figma_kiwi_field_t * field =
                FIGMA_ARRAY_PTR(
                    figma_kiwi_field_t, &definition->fields, field_index);
            const figma_kiwi_raw_field_t * raw_field =
                FIGMA_ARRAY_CONST_PTR(
                    figma_kiwi_raw_field_t,
                    &raw_definition->fields,
                    field_index);

            if(definition->kind == FIGMA_KIWI_DEFINITION_ENUM)
            {
                if(figma_string_assign_cstr(
                       memory, &field->type, "enum") == FIGMA_FALSE)
                {
                    goto cleanup;
                }
            }
            else if(raw_field->type < 0)
            {
                size_t primitive_index = (size_t)~raw_field->type;

                if(primitive_index >=
                        sizeof(primitive_types) / sizeof(primitive_types[0]) ||
                    figma_string_assign_cstr(
                        memory,
                        &field->type,
                        primitive_types[primitive_index]) == FIGMA_FALSE)
                {
                    goto cleanup;
                }
            }
            else
            {
                size_t type_index = (size_t)raw_field->type;
                const figma_kiwi_definition_t * type_definition;

                if(type_index >= schema->definitions.size)
                {
                    goto cleanup;
                }

                type_definition = FIGMA_ARRAY_CONST_PTR(
                    figma_kiwi_definition_t,
                    &schema->definitions,
                    type_index);

                if(figma_string_copy(
                       memory,
                       &field->type,
                       &type_definition->name) == FIGMA_FALSE)
                {
                    goto cleanup;
                }
            }
        }
    }

    success = reader.failed == FIGMA_FALSE ? FIGMA_TRUE : FIGMA_FALSE;

cleanup:
    figma_array_destroy(memory, &raw_definitions, &__figma_kiwi_raw_definition_destroy);
    return success;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_canvas_decode(figma_runtime_t * runtime, const figma_array_t * bytes, figma_document_t * document, figma_diagnostics_t * diagnostics)
{
    figma_memory_t * memory;
    figma_array_t chunks;
    figma_array_t encoded_schema;
    figma_array_t encoded_data;
    figma_kiwi_schema_t schema;
    const figma_canvas_chunk_t * schema_chunk;
    const figma_canvas_chunk_t * data_chunk;
    figma_bool_t success = FIGMA_FALSE;

    if(runtime == NULL || bytes == NULL || document == NULL)
    {
        return FIGMA_FALSE;
    }

    memory = &runtime->memory;
    figma_array_init(&chunks, sizeof(figma_canvas_chunk_t));
    figma_array_init(&encoded_schema, sizeof(uint8_t));
    figma_array_init(&encoded_data, sizeof(uint8_t));
    figma_kiwi_schema_init(&schema, memory);

    if(__figma_canvas_read_chunks(memory, bytes, &chunks) == FIGMA_FALSE)
    {
        FIGMA_DIAGNOSTIC_ADD(
            diagnostics,
            FIGMA_DIAGNOSTIC_WARNING,
            "fig_canvas_decode_failed",
            "Unable to parse fig-kiwi chunk table",
            "");
        goto cleanup;
    }

    schema_chunk =
        FIGMA_ARRAY_CONST_PTR(figma_canvas_chunk_t, &chunks, 0u);
    data_chunk =
        FIGMA_ARRAY_CONST_PTR(figma_canvas_chunk_t, &chunks, 1u);

    if(__figma_canvas_decompress_chunk(
           memory,
           (const uint8_t *)bytes->data + schema_chunk->offset,
           schema_chunk->size,
           &encoded_schema) == FIGMA_FALSE ||
        __figma_canvas_decompress_chunk(
           memory,
           (const uint8_t *)bytes->data + data_chunk->offset,
           data_chunk->size,
           &encoded_data) == FIGMA_FALSE)
    {
        FIGMA_DIAGNOSTIC_ADD(
            diagnostics,
            FIGMA_DIAGNOSTIC_WARNING,
            "fig_canvas_inflate_failed",
            "Unable to inflate fig-kiwi schema or scene chunk",
            "");
        goto cleanup;
    }

    if(__figma_canvas_decode_schema(memory, &encoded_schema, &schema) ==
            FIGMA_FALSE ||
        figma_canvas_decode_document_internal(
            memory, &schema, &encoded_data, document) == FIGMA_FALSE)
    {
        FIGMA_DIAGNOSTIC_ADD(
            diagnostics,
            FIGMA_DIAGNOSTIC_WARNING,
            "fig_canvas_decode_failed",
            "Unable to decode fig-kiwi scene graph",
            "");
        goto cleanup;
    }

    success = FIGMA_TRUE;

cleanup:
    figma_kiwi_schema_destroy(&schema);
    figma_array_destroy(memory, &encoded_data, NULL);
    figma_array_destroy(memory, &encoded_schema, NULL);
    figma_array_destroy(memory, &chunks, NULL);
    return success;
}

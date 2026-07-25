#pragma once

#include "figma_model.h"

typedef enum figma_kiwi_definition_kind
{
    FIGMA_KIWI_DEFINITION_ENUM = 0,
    FIGMA_KIWI_DEFINITION_STRUCT = 1,
    FIGMA_KIWI_DEFINITION_MESSAGE = 2
} figma_kiwi_definition_kind_t;

typedef struct figma_kiwi_field
{
    figma_string_t name;
    figma_string_t type;
    figma_bool_t array;
    uint32_t value;
} figma_kiwi_field_t;

typedef struct figma_kiwi_definition
{
    figma_string_t name;
    figma_kiwi_definition_kind_t kind;
    figma_array_t fields;
} figma_kiwi_definition_t;

typedef struct figma_kiwi_schema
{
    figma_memory_t * memory;
    figma_array_t definitions;
} figma_kiwi_schema_t;

typedef struct figma_kiwi_reader
{
    const uint8_t * data;
    size_t size;
    size_t offset;
    figma_bool_t failed;
} figma_kiwi_reader_t;

void figma_kiwi_schema_init(figma_kiwi_schema_t * schema, figma_memory_t * memory);
void figma_kiwi_schema_destroy(figma_kiwi_schema_t * schema);
const figma_kiwi_definition_t * figma_kiwi_find_definition(const figma_kiwi_schema_t * schema, figma_string_view_t name);
const figma_kiwi_field_t * figma_kiwi_find_field(const figma_kiwi_definition_t * definition, uint32_t value);

void figma_kiwi_reader_init(figma_kiwi_reader_t * reader, const uint8_t * data, size_t size);
figma_bool_t figma_kiwi_reader_eof(const figma_kiwi_reader_t * reader);
uint8_t figma_kiwi_read_byte(figma_kiwi_reader_t * reader);
uint32_t figma_kiwi_read_var_uint(figma_kiwi_reader_t * reader);
int32_t figma_kiwi_read_var_int(figma_kiwi_reader_t * reader);
uint64_t figma_kiwi_read_var_uint64(figma_kiwi_reader_t * reader);
int64_t figma_kiwi_read_var_int64(figma_kiwi_reader_t * reader);
float figma_kiwi_read_var_float(figma_kiwi_reader_t * reader);
figma_bool_t figma_kiwi_read_string(figma_memory_t * memory, figma_kiwi_reader_t * reader, figma_string_t * string);
figma_bool_t figma_kiwi_read_byte_array(figma_memory_t * memory, figma_kiwi_reader_t * reader, figma_array_t * bytes);

figma_bool_t figma_kiwi_skip_value(figma_memory_t * memory, const figma_kiwi_schema_t * schema, figma_kiwi_reader_t * reader, figma_string_view_t type, figma_bool_t array);
figma_bool_t figma_kiwi_read_enum(figma_memory_t * memory, const figma_kiwi_schema_t * schema, figma_kiwi_reader_t * reader, figma_string_view_t type, figma_string_t * value);
figma_bool_t figma_kiwi_is_enum(const figma_kiwi_schema_t * schema, figma_string_view_t type);

figma_bool_t figma_canvas_decode(figma_runtime_t * runtime, const figma_array_t * bytes, figma_document_t * document, figma_diagnostics_t * diagnostics);
figma_bool_t figma_canvas_decode_document_internal(figma_memory_t * memory, const figma_kiwi_schema_t * schema, const figma_array_t * encoded_data, figma_document_t * document);

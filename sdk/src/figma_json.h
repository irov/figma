#pragma once

#include "figma_internal.h"

#include "json/json.h"

typedef struct figma_json_document
{
    js_element_t * root;
} figma_json_document_t;

void figma_json_document_init(figma_json_document_t * document);
void figma_json_document_destroy(figma_json_document_t * document);
figma_result_t figma_json_parse(figma_memory_t * memory, figma_string_view_t data, figma_diagnostics_t * diagnostics, figma_json_document_t * document);
const js_element_t * figma_json_member(const js_element_t * value, figma_string_view_t key);
const js_element_t * figma_json_index(const js_element_t * value, size_t index);
figma_string_view_t figma_json_string(const js_element_t * value, figma_string_view_t default_value);
double figma_json_number(const js_element_t * value, double default_value);
figma_bool_t figma_json_bool(const js_element_t * value, figma_bool_t default_value);

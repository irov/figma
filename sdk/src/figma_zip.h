#pragma once

#include "figma_internal.h"

typedef struct figma_zip_entry
{
    figma_string_t name;
    uint16_t method;
    uint32_t crc32;
    uint64_t compressed_size;
    uint64_t uncompressed_size;
    uint64_t local_header_offset;
    figma_bool_t directory;
} figma_zip_entry_t;

typedef struct figma_zip_archive
{
    figma_memory_t * memory;
    const uint8_t * data;
    size_t size;
    figma_array_t entries;
} figma_zip_archive_t;

void figma_zip_init(figma_zip_archive_t * archive, figma_memory_t * memory);
void figma_zip_destroy(figma_zip_archive_t * archive);
figma_result_t figma_zip_open(figma_zip_archive_t * archive, const void * data, size_t size, figma_diagnostics_t * diagnostics);
void figma_zip_close(figma_zip_archive_t * archive);
size_t figma_zip_get_file_count(const figma_zip_archive_t * archive);
figma_bool_t figma_zip_get_file_name(const figma_zip_archive_t * archive, size_t index, figma_string_view_t * name);
figma_bool_t figma_zip_is_directory(const figma_zip_archive_t * archive, size_t index);
figma_result_t figma_zip_extract_file(const figma_zip_archive_t * archive, figma_string_view_t path, figma_array_t * bytes, figma_diagnostics_t * diagnostics);
figma_result_t figma_zip_extract_file_by_index(const figma_zip_archive_t * archive, size_t index, figma_array_t * bytes, figma_diagnostics_t * diagnostics);

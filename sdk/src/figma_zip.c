#include "figma_zip.h"

#include <limits.h>
#include <string.h>

#include <zlib.h>

#define FIGMA_ZIP_EOCD_SIGNATURE 0x06054b50u
#define FIGMA_ZIP_CENTRAL_SIGNATURE 0x02014b50u
#define FIGMA_ZIP_LOCAL_SIGNATURE 0x04034b50u
#define FIGMA_ZIP_METHOD_STORED 0u
#define FIGMA_ZIP_METHOD_DEFLATED 8u

//////////////////////////////////////////////////////////////////////////
static uint16_t __figma_zip_read_u16(const uint8_t * data)
{
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8u);
}

//////////////////////////////////////////////////////////////////////////
static uint32_t __figma_zip_read_u32(const uint8_t * data)
{
    return (uint32_t)data[0] |
        ((uint32_t)data[1] << 8u) |
        ((uint32_t)data[2] << 16u) |
        ((uint32_t)data[3] << 24u);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_zip_checked_range(size_t size, uint64_t offset, uint64_t length)
{
    return offset <= (uint64_t)size && length <= (uint64_t)size - offset
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_zip_entry_destroy(figma_memory_t * memory, void * value)
{
    figma_zip_entry_t * entry = (figma_zip_entry_t *)value;
    figma_string_destroy(memory, &entry->name);
}

//////////////////////////////////////////////////////////////////////////
static voidpf __figma_zip_zalloc(voidpf opaque, uInt items, uInt size)
{
    size_t bytes;

    if(figma_size_mul((size_t)items, (size_t)size, &bytes) == FIGMA_FALSE)
    {
        return NULL;
    }

    return figma_memory_allocate((figma_memory_t *)opaque, bytes);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_zip_zfree(voidpf opaque, voidpf address)
{
    figma_memory_deallocate((figma_memory_t *)opaque, address);
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_zip_error(figma_diagnostics_t * diagnostics, const char * code, const char * message)
{
    if(diagnostics != NULL)
    {
        FIGMA_DIAGNOSTIC_ADD(
            diagnostics, FIGMA_DIAGNOSTIC_ERROR, code, message, "");
    }

    return FIGMA_RESULT_PARSE_FAILED;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_zip_parse_central_directory(figma_zip_archive_t * archive, figma_diagnostics_t * diagnostics)
{
    const size_t eocd_size = 22u;
    const size_t max_comment_size = 0xffffu;
    size_t search_begin;
    size_t eocd_offset;
    size_t offset_cursor;
    uint16_t disk_number;
    uint16_t central_directory_disk;
    uint16_t disk_entry_count;
    uint16_t entry_count;
    uint32_t central_directory_size;
    uint32_t central_directory_offset;
    uint64_t offset;
    uint16_t index;

    if(archive->size < eocd_size)
    {
        return __figma_zip_error(
            diagnostics, "fig_zip_invalid", "File is too small to be a ZIP archive");
    }

    search_begin = archive->size > eocd_size + max_comment_size
        ? archive->size - eocd_size - max_comment_size
        : 0u;
    eocd_offset = SIZE_MAX;
    offset_cursor = archive->size - eocd_size + 1u;

    while(offset_cursor-- > search_begin)
    {
        uint16_t comment_size;

        if(__figma_zip_read_u32(archive->data + offset_cursor) !=
            FIGMA_ZIP_EOCD_SIGNATURE)
        {
            continue;
        }

        comment_size = __figma_zip_read_u16(archive->data + offset_cursor + 20u);

        if(offset_cursor + eocd_size + comment_size == archive->size)
        {
            eocd_offset = offset_cursor;
            break;
        }
    }

    if(eocd_offset == SIZE_MAX)
    {
        return __figma_zip_error(
            diagnostics,
            "fig_zip_invalid",
            "ZIP end of central directory was not found");
    }

    disk_number = __figma_zip_read_u16(archive->data + eocd_offset + 4u);
    central_directory_disk =
        __figma_zip_read_u16(archive->data + eocd_offset + 6u);
    disk_entry_count = __figma_zip_read_u16(archive->data + eocd_offset + 8u);
    entry_count = __figma_zip_read_u16(archive->data + eocd_offset + 10u);
    central_directory_size =
        __figma_zip_read_u32(archive->data + eocd_offset + 12u);
    central_directory_offset =
        __figma_zip_read_u32(archive->data + eocd_offset + 16u);

    if(disk_number != 0u ||
        central_directory_disk != 0u ||
        disk_entry_count != entry_count)
    {
        return __figma_zip_error(
            diagnostics,
            "fig_zip_multidisk_unsupported",
            "Multidisk ZIP archives are unsupported");
    }

    if(entry_count == UINT16_MAX ||
        central_directory_size == UINT32_MAX ||
        central_directory_offset == UINT32_MAX)
    {
        return __figma_zip_error(
            diagnostics, "fig_zip64_unsupported", "ZIP64 archives are unsupported");
    }

    if(__figma_zip_checked_range(
           archive->size,
           central_directory_offset,
           central_directory_size) == FIGMA_FALSE)
    {
        return __figma_zip_error(
            diagnostics,
            "fig_zip_invalid",
            "ZIP central directory points outside the file");
    }

    offset = central_directory_offset;

    for(index = 0u; index != entry_count; ++index)
    {
        uint16_t flags;
        uint16_t method;
        uint32_t crc;
        uint32_t compressed_size;
        uint32_t uncompressed_size;
        uint16_t name_size;
        uint16_t extra_size;
        uint16_t comment_size;
        uint32_t local_header_offset;
        uint64_t entry_size;
        figma_zip_entry_t * entry;
        figma_string_view_t name;

        if(__figma_zip_checked_range(archive->size, offset, 46u) == FIGMA_FALSE ||
            __figma_zip_read_u32(archive->data + (size_t)offset) !=
                FIGMA_ZIP_CENTRAL_SIGNATURE)
        {
            return __figma_zip_error(
                diagnostics,
                "fig_zip_invalid",
                "ZIP central directory entry is invalid");
        }

        flags = __figma_zip_read_u16(archive->data + (size_t)offset + 8u);
        method = __figma_zip_read_u16(archive->data + (size_t)offset + 10u);
        crc = __figma_zip_read_u32(archive->data + (size_t)offset + 16u);
        compressed_size =
            __figma_zip_read_u32(archive->data + (size_t)offset + 20u);
        uncompressed_size =
            __figma_zip_read_u32(archive->data + (size_t)offset + 24u);
        name_size = __figma_zip_read_u16(archive->data + (size_t)offset + 28u);
        extra_size = __figma_zip_read_u16(archive->data + (size_t)offset + 30u);
        comment_size = __figma_zip_read_u16(archive->data + (size_t)offset + 32u);
        local_header_offset =
            __figma_zip_read_u32(archive->data + (size_t)offset + 42u);
        entry_size =
            46u + (uint64_t)name_size + (uint64_t)extra_size + (uint64_t)comment_size;

        if(compressed_size == UINT32_MAX ||
            uncompressed_size == UINT32_MAX ||
            local_header_offset == UINT32_MAX)
        {
            return __figma_zip_error(
                diagnostics,
                "fig_zip64_unsupported",
                "ZIP64 entries are unsupported");
        }

        if((flags & 0x1u) != 0u)
        {
            return __figma_zip_error(
                diagnostics,
                "fig_zip_encrypted_unsupported",
                "Encrypted ZIP entries are unsupported");
        }

        if(__figma_zip_checked_range(
               archive->size, offset, entry_size) == FIGMA_FALSE)
        {
            return __figma_zip_error(
                diagnostics,
                "fig_zip_invalid",
                "ZIP central directory entry exceeds file bounds");
        }

        entry = (figma_zip_entry_t *)figma_array_push_uninitialized(
            archive->memory, &archive->entries);

        if(entry == NULL)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }

        memset(entry, 0, sizeof(*entry));
        figma_string_init(&entry->name);
        name.data = (const char *)(archive->data + (size_t)offset + 46u);
        name.size = name_size;

        if(figma_string_assign(
               archive->memory, &entry->name, name) == FIGMA_FALSE)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }

        entry->method = method;
        entry->crc32 = crc;
        entry->compressed_size = compressed_size;
        entry->uncompressed_size = uncompressed_size;
        entry->local_header_offset = local_header_offset;
        entry->directory =
            entry->name.size != 0u &&
                entry->name.data[entry->name.size - 1u] == '/'
            ? FIGMA_TRUE
            : FIGMA_FALSE;
        offset += entry_size;
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_zip_resolve_data_offset(const figma_zip_archive_t * archive, const figma_zip_entry_t * entry, uint64_t * data_offset, figma_diagnostics_t * diagnostics)
{
    uint16_t name_size;
    uint16_t extra_size;
    uint64_t offset;

    if(data_offset == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    if(__figma_zip_checked_range(
           archive->size, entry->local_header_offset, 30u) == FIGMA_FALSE ||
        __figma_zip_read_u32(
            archive->data + (size_t)entry->local_header_offset) !=
            FIGMA_ZIP_LOCAL_SIGNATURE)
    {
        return __figma_zip_error(
            diagnostics, "fig_zip_invalid", "ZIP local header is invalid");
    }

    name_size = __figma_zip_read_u16(
        archive->data + (size_t)entry->local_header_offset + 26u);
    extra_size = __figma_zip_read_u16(
        archive->data + (size_t)entry->local_header_offset + 28u);
    offset =
        entry->local_header_offset + 30u + (uint64_t)name_size + (uint64_t)extra_size;

    if(__figma_zip_checked_range(
           archive->size, offset, entry->compressed_size) == FIGMA_FALSE)
    {
        return __figma_zip_error(
            diagnostics, "fig_zip_invalid", "ZIP entry data exceeds file bounds");
    }

    *data_offset = offset;

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_zip_extract_stored(const figma_zip_archive_t * archive, const figma_zip_entry_t * entry, uint64_t data_offset, figma_array_t * bytes, figma_diagnostics_t * diagnostics)
{
    uLong crc;

    if(entry->compressed_size != entry->uncompressed_size)
    {
        return __figma_zip_error(
            diagnostics,
            "fig_zip_invalid",
            "Stored ZIP entry has mismatched compressed and uncompressed sizes");
    }

    if(entry->uncompressed_size > SIZE_MAX ||
        figma_array_resize(
            archive->memory, bytes, (size_t)entry->uncompressed_size, NULL) ==
            FIGMA_FALSE)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    if(entry->uncompressed_size != 0u)
    {
        memcpy(
            bytes->data,
            archive->data + (size_t)data_offset,
            (size_t)entry->uncompressed_size);
    }

    crc = crc32(0L, (const Bytef *)bytes->data, (uInt)bytes->size);

    if(crc != entry->crc32)
    {
        figma_array_clear(archive->memory, bytes, NULL);
        return __figma_zip_error(
            diagnostics,
            "fig_zip_crc_failed",
            "Stored ZIP entry CRC check failed");
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_zip_extract_deflated(const figma_zip_archive_t * archive, const figma_zip_entry_t * entry, uint64_t data_offset, figma_array_t * bytes, figma_diagnostics_t * diagnostics)
{
    z_stream stream;
    Bytef empty_output;
    int result;
    uLong crc;

    if(entry->uncompressed_size > SIZE_MAX ||
        figma_array_resize(
            archive->memory, bytes, (size_t)entry->uncompressed_size, NULL) ==
            FIGMA_FALSE)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    memset(&stream, 0, sizeof(stream));
    stream.zalloc = &__figma_zip_zalloc;
    stream.zfree = &__figma_zip_zfree;
    stream.opaque = archive->memory;
    stream.next_in =
        (Bytef *)(uintptr_t)(archive->data + (size_t)data_offset);
    stream.avail_in = (uInt)entry->compressed_size;
    empty_output = 0u;
    stream.next_out =
        bytes->size != 0u ? (Bytef *)bytes->data : &empty_output;
    stream.avail_out = bytes->size != 0u ? (uInt)bytes->size : 1u;

    if(inflateInit2(&stream, -MAX_WBITS) != Z_OK)
    {
        figma_array_clear(archive->memory, bytes, NULL);
        return __figma_zip_error(
            diagnostics,
            "fig_zip_inflate_failed",
            "Unable to initialize zlib inflate");
    }

    result = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);

    if(result != Z_STREAM_END || stream.total_out != entry->uncompressed_size)
    {
        figma_array_clear(archive->memory, bytes, NULL);
        return __figma_zip_error(
            diagnostics, "fig_zip_inflate_failed", "Unable to inflate ZIP entry");
    }

    crc = crc32(0L, (const Bytef *)bytes->data, (uInt)bytes->size);

    if(crc != entry->crc32)
    {
        figma_array_clear(archive->memory, bytes, NULL);
        return __figma_zip_error(
            diagnostics,
            "fig_zip_crc_failed",
            "Deflated ZIP entry CRC check failed");
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
void figma_zip_init(figma_zip_archive_t * archive, figma_memory_t * memory)
{
    memset(archive, 0, sizeof(*archive));
    archive->memory = memory;
    figma_array_init(&archive->entries, sizeof(figma_zip_entry_t));
}

//////////////////////////////////////////////////////////////////////////
void figma_zip_destroy(figma_zip_archive_t * archive)
{
    if(archive == NULL)
    {
        return;
    }

    figma_array_destroy(
        archive->memory, &archive->entries, &__figma_zip_entry_destroy);
    archive->memory = NULL;
    archive->data = NULL;
    archive->size = 0u;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t figma_zip_open(figma_zip_archive_t * archive, const void * data, size_t size, figma_diagnostics_t * diagnostics)
{
    figma_result_t result;

    if(archive == NULL || data == NULL || size == 0u)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    figma_zip_close(archive);
    archive->data = (const uint8_t *)data;
    archive->size = size;
    result = __figma_zip_parse_central_directory(archive, diagnostics);

    if(result != FIGMA_RESULT_OK)
    {
        figma_zip_close(archive);
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
void figma_zip_close(figma_zip_archive_t * archive)
{
    if(archive == NULL)
    {
        return;
    }

    figma_array_clear(
        archive->memory, &archive->entries, &__figma_zip_entry_destroy);
    archive->data = NULL;
    archive->size = 0u;
}

//////////////////////////////////////////////////////////////////////////
size_t figma_zip_get_file_count(const figma_zip_archive_t * archive)
{
    return archive != NULL ? archive->entries.size : 0u;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_zip_get_file_name(const figma_zip_archive_t * archive, size_t index, figma_string_view_t * name)
{
    const figma_zip_entry_t * entry;

    if(archive == NULL || name == NULL || index >= archive->entries.size)
    {
        return FIGMA_FALSE;
    }

    entry = FIGMA_ARRAY_CONST_PTR(figma_zip_entry_t, &archive->entries, index);
    *name = figma_string_view(&entry->name);

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_zip_is_directory(const figma_zip_archive_t * archive, size_t index)
{
    if(archive == NULL || index >= archive->entries.size)
    {
        return FIGMA_FALSE;
    }

    return FIGMA_ARRAY_CONST_PTR(
               figma_zip_entry_t, &archive->entries, index)
        ->directory;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t figma_zip_extract_file(const figma_zip_archive_t * archive, figma_string_view_t path, figma_array_t * bytes, figma_diagnostics_t * diagnostics)
{
    size_t index;

    if(archive == NULL || bytes == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    for(index = 0u; index != archive->entries.size; ++index)
    {
        const figma_zip_entry_t * entry =
            FIGMA_ARRAY_CONST_PTR(figma_zip_entry_t, &archive->entries, index);

        if(figma_string_equal_view(&entry->name, path) == FIGMA_TRUE)
        {
            return figma_zip_extract_file_by_index(
                archive, index, bytes, diagnostics);
        }
    }

    if(diagnostics != NULL)
    {
        FIGMA_DIAGNOSTIC_ADD(
            diagnostics,
            FIGMA_DIAGNOSTIC_ERROR,
            "fig_zip_entry_missing",
            "Required ZIP entry is missing",
            "");
    }

    return FIGMA_RESULT_MISSING_ENTRY;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t figma_zip_extract_file_by_index(const figma_zip_archive_t * archive, size_t index, figma_array_t * bytes, figma_diagnostics_t * diagnostics)
{
    const figma_zip_entry_t * entry;
    uint64_t data_offset;
    figma_result_t result;

    if(archive == NULL || bytes == NULL || index >= archive->entries.size)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    figma_array_clear(archive->memory, bytes, NULL);
    entry = FIGMA_ARRAY_CONST_PTR(figma_zip_entry_t, &archive->entries, index);

    if(entry->directory == FIGMA_TRUE)
    {
        return FIGMA_RESULT_OK;
    }

    if(entry->uncompressed_size > SIZE_MAX)
    {
        return __figma_zip_error(
            diagnostics, "fig_zip_entry_too_large", "ZIP entry is too large");
    }

    data_offset = 0u;
    result =
        __figma_zip_resolve_data_offset(archive, entry, &data_offset, diagnostics);

    if(result != FIGMA_RESULT_OK)
    {
        return result;
    }

    if(entry->method == FIGMA_ZIP_METHOD_STORED)
    {
        return __figma_zip_extract_stored(
            archive, entry, data_offset, bytes, diagnostics);
    }

    if(entry->method == FIGMA_ZIP_METHOD_DEFLATED)
    {
        return __figma_zip_extract_deflated(
            archive, entry, data_offset, bytes, diagnostics);
    }

    __figma_zip_error(
        diagnostics,
        "fig_zip_method_unsupported",
        "ZIP entry compression method is unsupported");

    return FIGMA_RESULT_UNSUPPORTED_FORMAT;
}

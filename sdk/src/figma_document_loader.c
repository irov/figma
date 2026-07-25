#include "figma_canvas.h"
#include "figma_json.h"
#include "figma_model.h"
#include "figma_zip.h"

#include <string.h>

#include "json/json.h"

#define FIGMA_KIWI_PREFIX "fig-kiwi"
#define FIGMA_SUPPORTED_KIWI_VERSION 'j'

typedef struct figma_meta_info
{
    figma_string_t file_name;
    figma_rectf_t render_coordinates;
    figma_vec2f_t thumbnail_size;
} figma_meta_info_t;

//////////////////////////////////////////////////////////////////////////
static figma_string_view_t __figma_loader_key(const char * value)
{
    return figma_string_view_cstr(value);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_loader_asset_init(figma_asset_t * asset)
{
    memset(asset, 0, sizeof(*asset));
    figma_string_init(&asset->id);
    figma_string_init(&asset->path);
    figma_string_init(&asset->mime);
    figma_array_init(&asset->bytes, sizeof(uint8_t));
}

//////////////////////////////////////////////////////////////////////////
static void __figma_loader_binding_destroy(figma_memory_t * memory, void * value)
{
    figma_binding_t * binding = (figma_binding_t *)value;
    figma_string_destroy(memory, &binding->node_id);
    figma_string_destroy(memory, &binding->key);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_loader_action_destroy(figma_memory_t * memory, void * value)
{
    figma_action_t * action = (figma_action_t *)value;
    figma_string_destroy(memory, &action->node_id);
    figma_string_destroy(memory, &action->action_id);
    figma_string_destroy(memory, &action->target_frame_id);
}

//////////////////////////////////////////////////////////////////////////
static uint32_t __figma_loader_read_big_endian_u32(const uint8_t * bytes)
{
    return ((uint32_t)bytes[0] << 24u) |
        ((uint32_t)bytes[1] << 16u) |
        ((uint32_t)bytes[2] << 8u) |
        (uint32_t)bytes[3];
}

//////////////////////////////////////////////////////////////////////////
static void __figma_loader_fill_image_metadata(figma_asset_t * asset)
{
    const uint8_t * bytes;

    if(asset == NULL || asset->bytes.size < 33u)
    {
        return;
    }

    bytes = (const uint8_t *)asset->bytes.data;

    if(memcmp(bytes, "\x89PNG\r\n\x1a\n", 8u) == 0 &&
        __figma_loader_read_big_endian_u32(bytes + 8u) == 13u &&
        memcmp(bytes + 12u, "IHDR", 4u) == 0)
    {
        asset->width = __figma_loader_read_big_endian_u32(bytes + 16u);
        asset->height = __figma_loader_read_big_endian_u32(bytes + 20u);
        asset->color_type = bytes[25u];
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_loader_set_mime(figma_memory_t * memory, figma_string_t * mime, const figma_array_t * bytes)
{
    const uint8_t * data = (const uint8_t *)bytes->data;
    const char * value = "application/octet-stream";

    if(bytes->size >= 8u &&
        memcmp(data, "\x89PNG\r\n\x1a\n", 8u) == 0)
    {
        value = "image/png";
    }
    else if(bytes->size >= 3u &&
        data[0] == 0xffu &&
        data[1] == 0xd8u &&
        data[2] == 0xffu)
    {
        value = "image/jpeg";
    }
    else if(bytes->size >= 12u &&
        memcmp(data, "RIFF", 4u) == 0 &&
        memcmp(data + 8u, "WEBP", 4u) == 0)
    {
        value = "image/webp";
    }
    else if(bytes->size >= 6u &&
        (memcmp(data, "GIF87a", 6u) == 0 ||
            memcmp(data, "GIF89a", 6u) == 0))
    {
        value = "image/gif";
    }

    return figma_string_assign_cstr(memory, mime, value);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_loader_meta_init(figma_meta_info_t * meta)
{
    memset(meta, 0, sizeof(*meta));
    figma_string_init(&meta->file_name);
    meta->render_coordinates.w = 1024.0f;
    meta->render_coordinates.h = 768.0f;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_loader_meta_destroy(figma_memory_t * memory, figma_meta_info_t * meta)
{
    figma_string_destroy(memory, &meta->file_name);
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_loader_parse_meta(figma_memory_t * memory, figma_diagnostics_t * diagnostics, figma_string_view_t data, figma_meta_info_t * meta)
{
    figma_json_document_t json;
    figma_result_t result;
    const js_element_t * root;
    const js_element_t * client_meta;
    const js_element_t * render_coordinates;
    const js_element_t * thumbnail_size;
    figma_string_view_t file_name;

    figma_json_document_init(&json);
    result = figma_json_parse(memory, data, diagnostics, &json);

    if(result != FIGMA_RESULT_OK)
    {
        return result;
    }

    root = json.root;

    if(root == NULL || js_is_object(root) != JS_TRUE)
    {
        FIGMA_DIAGNOSTIC_ADD(
            diagnostics,
            FIGMA_DIAGNOSTIC_ERROR,
            "fig_meta_invalid",
            "meta.json root must be a JSON object",
            "");
        figma_json_document_destroy(&json);
        return FIGMA_RESULT_PARSE_FAILED;
    }

    file_name = figma_json_string(
        figma_json_member(root, __figma_loader_key("file_name")),
        __figma_loader_key(NULL));

    if(file_name.size != 0u &&
        figma_string_assign(memory, &meta->file_name, file_name) == FIGMA_FALSE)
    {
        figma_json_document_destroy(&json);
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    client_meta =
        figma_json_member(root, __figma_loader_key("client_meta"));
    render_coordinates = figma_json_member(
        client_meta, __figma_loader_key("render_coordinates"));
    meta->render_coordinates.x = (float)figma_json_number(
        figma_json_member(render_coordinates, __figma_loader_key("x")), 0.0);
    meta->render_coordinates.y = (float)figma_json_number(
        figma_json_member(render_coordinates, __figma_loader_key("y")), 0.0);
    meta->render_coordinates.w = (float)figma_json_number(
        figma_json_member(render_coordinates, __figma_loader_key("width")),
        1024.0);
    meta->render_coordinates.h = (float)figma_json_number(
        figma_json_member(render_coordinates, __figma_loader_key("height")),
        768.0);

    thumbnail_size = figma_json_member(
        client_meta, __figma_loader_key("thumbnail_size"));
    meta->thumbnail_size.x = (float)figma_json_number(
        figma_json_member(thumbnail_size, __figma_loader_key("width")), 0.0);
    meta->thumbnail_size.y = (float)figma_json_number(
        figma_json_member(thumbnail_size, __figma_loader_key("height")), 0.0);

    figma_json_document_destroy(&json);
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_loader_starts_with(figma_string_view_t value, const char * prefix)
{
    const size_t prefix_size = strlen(prefix);
    return value.size >= prefix_size &&
            memcmp(value.data, prefix, prefix_size) == 0
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_loader_add_thumbnail(figma_document_t * document, figma_zip_archive_t * archive)
{
    figma_array_t bytes;
    figma_result_t result;
    figma_asset_t * asset;

    figma_array_init(&bytes, sizeof(uint8_t));
    result = figma_zip_extract_file(
        archive, __figma_loader_key("thumbnail.png"), &bytes, NULL);

    if(result != FIGMA_RESULT_OK)
    {
        figma_array_destroy(document->memory, &bytes, NULL);
        FIGMA_DIAGNOSTIC_ADD(
            &document->diagnostics,
            FIGMA_DIAGNOSTIC_WARNING,
            "fig_thumbnail_missing",
            "thumbnail.png is missing",
            "");
        return FIGMA_RESULT_OK;
    }

    asset = (figma_asset_t *)figma_array_push_uninitialized(
        document->memory, &document->assets);

    if(asset == NULL)
    {
        figma_array_destroy(document->memory, &bytes, NULL);
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    __figma_loader_asset_init(asset);

    if(figma_string_assign_cstr(
           document->memory, &asset->id, "thumbnail.png") == FIGMA_FALSE ||
        figma_string_assign_cstr(
           document->memory, &asset->path, "thumbnail.png") == FIGMA_FALSE ||
        figma_string_assign_cstr(
           document->memory, &asset->mime, "image/png") == FIGMA_FALSE)
    {
        figma_array_destroy(document->memory, &bytes, NULL);
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    asset->bytes = bytes;
    figma_array_init(&bytes, sizeof(uint8_t));
    __figma_loader_fill_image_metadata(asset);
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_loader_add_images(figma_document_t * document, const figma_zip_archive_t * archive)
{
    size_t index;

    for(index = 0u; index != figma_zip_get_file_count(archive); ++index)
    {
        figma_string_view_t name;
        figma_array_t bytes;
        figma_asset_t * asset;
        figma_string_view_t id;
        figma_result_t result;

        if(figma_zip_is_directory(archive, index) == FIGMA_TRUE ||
            figma_zip_get_file_name(archive, index, &name) == FIGMA_FALSE ||
            __figma_loader_starts_with(name, "images/") == FIGMA_FALSE ||
            name.size == 7u)
        {
            continue;
        }

        figma_array_init(&bytes, sizeof(uint8_t));
        result = figma_zip_extract_file_by_index(
            archive, index, &bytes, &document->diagnostics);

        if(result != FIGMA_RESULT_OK)
        {
            figma_array_destroy(document->memory, &bytes, NULL);
            continue;
        }

        asset = (figma_asset_t *)figma_array_push_uninitialized(
            document->memory, &document->assets);

        if(asset == NULL)
        {
            figma_array_destroy(document->memory, &bytes, NULL);
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }

        __figma_loader_asset_init(asset);
        id.data = name.data + 7u;
        id.size = name.size - 7u;

        if(figma_string_assign(
               document->memory, &asset->id, id) == FIGMA_FALSE ||
            figma_string_assign(
               document->memory, &asset->path, name) == FIGMA_FALSE ||
            __figma_loader_set_mime(
               document->memory, &asset->mime, &bytes) == FIGMA_FALSE)
        {
            figma_array_destroy(document->memory, &bytes, NULL);
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }

        asset->bytes = bytes;
        figma_array_init(&bytes, sizeof(uint8_t));
        __figma_loader_fill_image_metadata(asset);
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_runtime_load_document_from_fig_data(figma_runtime_t * runtime, const void * data, size_t size, const figma_load_options_t * options, figma_document_t ** document)
{
    figma_memory_t * memory;
    figma_document_t * value = NULL;
    figma_zip_archive_t archive;
    figma_array_t meta_bytes;
    figma_array_t canvas_bytes;
    figma_meta_info_t meta;
    figma_result_t result;
    figma_bool_t archive_initialized = FIGMA_FALSE;
    figma_bool_t document_initialized = FIGMA_FALSE;

    if(document == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    *document = NULL;

    if(runtime == NULL || data == NULL || size == 0u || options == NULL ||
        (options->source_name.data == NULL &&
            options->source_name.size != 0u))
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    memory = &runtime->memory;
    figma_array_init(&meta_bytes, sizeof(uint8_t));
    figma_array_init(&canvas_bytes, sizeof(uint8_t));
    __figma_loader_meta_init(&meta);
    figma_zip_init(&archive, memory);
    archive_initialized = FIGMA_TRUE;
    value = (figma_document_t *)figma_memory_allocate(
        memory, sizeof(figma_document_t));

    if(value == NULL)
    {
        result = FIGMA_RESULT_OUT_OF_MEMORY;
        goto cleanup;
    }

    figma_document_init(value, runtime, memory);
    document_initialized = FIGMA_TRUE;
    value->render_coordinates.x = 0.0f;
    value->render_coordinates.y = 0.0f;
    value->render_coordinates.w = 1024.0f;
    value->render_coordinates.h = 768.0f;

    if(figma_string_assign(
           memory, &value->path, options->source_name) == FIGMA_FALSE)
    {
        result = FIGMA_RESULT_OUT_OF_MEMORY;
        goto cleanup;
    }

    result = figma_zip_open(
        &archive, data, size, &value->diagnostics);
    if(result != FIGMA_RESULT_OK)
    {
        goto cleanup;
    }

    result = figma_zip_extract_file(
        &archive,
        __figma_loader_key("meta.json"),
        &meta_bytes,
        &value->diagnostics);
    if(result != FIGMA_RESULT_OK)
    {
        result = FIGMA_RESULT_MISSING_ENTRY;
        goto cleanup;
    }

    {
        figma_string_view_t meta_json;
        meta_json.data = (const char *)meta_bytes.data;
        meta_json.size = meta_bytes.size;
        result = __figma_loader_parse_meta(
            memory, &value->diagnostics, meta_json, &meta);
    }
    if(result != FIGMA_RESULT_OK)
    {
        goto cleanup;
    }

    if(figma_string_copy(
           memory, &value->file_name, &meta.file_name) == FIGMA_FALSE)
    {
        result = FIGMA_RESULT_OUT_OF_MEMORY;
        goto cleanup;
    }

    value->render_coordinates = meta.render_coordinates;
    value->thumbnail_size = meta.thumbnail_size;

    result = __figma_loader_add_thumbnail(value, &archive);
    if(result != FIGMA_RESULT_OK)
    {
        goto cleanup;
    }

    if(options->extract_image_assets == FIGMA_TRUE)
    {
        result = __figma_loader_add_images(value, &archive);
        if(result != FIGMA_RESULT_OK)
        {
            goto cleanup;
        }
    }

    result = figma_zip_extract_file(
        &archive,
        __figma_loader_key("canvas.fig"),
        &canvas_bytes,
        &value->diagnostics);
    if(result != FIGMA_RESULT_OK)
    {
        result = FIGMA_RESULT_MISSING_ENTRY;
        goto cleanup;
    }

    if(canvas_bytes.size < 9u ||
        memcmp(canvas_bytes.data, FIGMA_KIWI_PREFIX, 8u) != 0)
    {
        FIGMA_DIAGNOSTIC_ADD(
            &value->diagnostics,
            FIGMA_DIAGNOSTIC_ERROR,
            "fig_canvas_bad_magic",
            "canvas.fig does not start with fig-kiwi",
            "");
        result = FIGMA_RESULT_UNSUPPORTED_FORMAT;
        goto cleanup;
    }

    value->canvas_version = ((const char *)canvas_bytes.data)[8u];

    if(value->canvas_version != FIGMA_SUPPORTED_KIWI_VERSION)
    {
        FIGMA_DIAGNOSTIC_ADD(
            &value->diagnostics,
            FIGMA_DIAGNOSTIC_ERROR,
            "fig_canvas_unsupported_version",
            "canvas.fig uses an unsupported fig-kiwi revision",
            "");
        result = FIGMA_RESULT_UNSUPPORTED_FORMAT;
        goto cleanup;
    }

    if(figma_canvas_decode(
           runtime,
           &canvas_bytes,
           value,
           &value->diagnostics) == FIGMA_FALSE)
    {
        FIGMA_DIAGNOSTIC_ADD(
            &value->diagnostics,
            FIGMA_DIAGNOSTIC_WARNING,
            "fig_canvas_decoder_unsupported",
            "Binary fig-kiwi canvas decoding is not available for this file; render commands will be skipped until the required scene data is decoded",
            "");
    }

    if(options->keep_canvas_bytes == FIGMA_TRUE)
    {
        value->canvas_bytes = canvas_bytes;
        figma_array_init(&canvas_bytes, sizeof(uint8_t));
    }

    if(value->file_name.size == 0u &&
        figma_string_copy(
            memory, &value->file_name, &value->path) == FIGMA_FALSE)
    {
        result = FIGMA_RESULT_OUT_OF_MEMORY;
        goto cleanup;
    }

    *document = value;
    value = NULL;
    document_initialized = FIGMA_FALSE;
    result = FIGMA_RESULT_OK;

cleanup:
        if(document_initialized == FIGMA_TRUE)
    {
        figma_document_deinit(value);
    }
    figma_memory_deallocate(memory, value);
    if(archive_initialized == FIGMA_TRUE)
    {
        figma_zip_destroy(&archive);
    }
    __figma_loader_meta_destroy(memory, &meta);
    figma_array_destroy(memory, &canvas_bytes, NULL);
    figma_array_destroy(memory, &meta_bytes, NULL);
    return result;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_loader_parse_binding_property(figma_string_view_t value, figma_binding_property_t * property)
{
    if(value.size == 0u ||
        figma_string_view_equal_cstr(value, "text") == FIGMA_TRUE)
    {
        *property = FIGMA_BINDING_PROPERTY_TEXT;
        return FIGMA_TRUE;
    }
    if(figma_string_view_equal_cstr(value, "visible") == FIGMA_TRUE)
    {
        *property = FIGMA_BINDING_PROPERTY_VISIBLE;
        return FIGMA_TRUE;
    }
    if(figma_string_view_equal_cstr(value, "enabled") == FIGMA_TRUE)
    {
        *property = FIGMA_BINDING_PROPERTY_ENABLED;
        return FIGMA_TRUE;
    }
    if(figma_string_view_equal_cstr(value, "selected") == FIGMA_TRUE)
    {
        *property = FIGMA_BINDING_PROPERTY_SELECTED;
        return FIGMA_TRUE;
    }
    if(figma_string_view_equal_cstr(value, "image") == FIGMA_TRUE)
    {
        *property = FIGMA_BINDING_PROPERTY_IMAGE;
        return FIGMA_TRUE;
    }
    return FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_loader_parse_bindings(figma_document_t * document, const js_element_t * array, figma_array_t * bindings)
{
    size_t count;
    size_t index;

    if(array == NULL)
    {
        return FIGMA_RESULT_OK;
    }

    count = js_array_size(array);

    if(figma_array_reserve(
           document->memory, bindings, count) == FIGMA_FALSE)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    for(index = 0u; index != count; ++index)
    {
        const js_element_t * item = figma_json_index(array, index);
        const figma_string_view_t node_id = figma_json_string(
            figma_json_member(item, __figma_loader_key("nodeId")),
            __figma_loader_key(NULL));
        const figma_string_view_t key = figma_json_string(
            figma_json_member(item, __figma_loader_key("key")),
            __figma_loader_key(NULL));
        const figma_string_view_t property_value = figma_json_string(
            figma_json_member(item, __figma_loader_key("property")),
            __figma_loader_key("text"));
        figma_binding_property_t property;
        figma_binding_t * binding;

        if(node_id.size == 0u || key.size == 0u)
        {
            FIGMA_DIAGNOSTIC_ADD(
                &document->diagnostics,
                FIGMA_DIAGNOSTIC_WARNING,
                "ux_binding_skipped",
                "Binding entry requires nodeId and key",
                "");
            continue;
        }

        if(__figma_loader_parse_binding_property(
               property_value, &property) == FIGMA_FALSE)
        {
            FIGMA_DIAGNOSTIC_ADD(
                &document->diagnostics,
                FIGMA_DIAGNOSTIC_WARNING,
                "ux_binding_property_unsupported",
                "Binding entry uses an unsupported property",
                "");
            continue;
        }

        binding = (figma_binding_t *)figma_array_push_uninitialized(
            document->memory, bindings);

        if(binding == NULL)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }

        memset(binding, 0, sizeof(*binding));
        figma_string_init(&binding->node_id);
        figma_string_init(&binding->key);
        binding->property = property;

        if(figma_string_assign(
               document->memory, &binding->node_id, node_id) == FIGMA_FALSE ||
            figma_string_assign(
               document->memory, &binding->key, key) == FIGMA_FALSE)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_prototype_event_type_t __figma_loader_parse_trigger(figma_string_view_t trigger)
{
    if(figma_string_view_equal_cstr(trigger, "hover") == FIGMA_TRUE ||
        figma_string_view_equal_cstr(trigger, "hoverEnter") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_HOVER_ENTER;
    }
    if(figma_string_view_equal_cstr(trigger, "hoverLeave") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_HOVER_LEAVE;
    }
    if(figma_string_view_equal_cstr(trigger, "press") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_PRESS;
    }
    if(figma_string_view_equal_cstr(trigger, "pointerDown") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_POINTER_DOWN;
    }
    if(figma_string_view_equal_cstr(trigger, "pointerUp") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_POINTER_UP;
    }
    if(figma_string_view_equal_cstr(trigger, "keyDown") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_KEY_DOWN;
    }
    if(figma_string_view_equal_cstr(trigger, "click") == FIGMA_TRUE)
    {
        return FIGMA_PROTOTYPE_EVENT_CLICK;
    }
    return FIGMA_PROTOTYPE_EVENT_UNSUPPORTED;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_loader_parse_actions(figma_document_t * document, const js_element_t * array, figma_array_t * actions)
{
    size_t count;
    size_t index;

    if(array == NULL)
    {
        return FIGMA_RESULT_OK;
    }

    count = js_array_size(array);

    if(figma_array_reserve(
           document->memory, actions, count) == FIGMA_FALSE)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    for(index = 0u; index != count; ++index)
    {
        const js_element_t * item = figma_json_index(array, index);
        const figma_string_view_t node_id = figma_json_string(
            figma_json_member(item, __figma_loader_key("nodeId")),
            __figma_loader_key(NULL));
        const figma_string_view_t action_id = figma_json_string(
            figma_json_member(item, __figma_loader_key("actionId")),
            __figma_loader_key(NULL));
        const figma_string_view_t target_frame_id = figma_json_string(
            figma_json_member(item, __figma_loader_key("targetFrameId")),
            __figma_loader_key(NULL));
        const figma_string_view_t trigger = figma_json_string(
            figma_json_member(item, __figma_loader_key("trigger")),
            __figma_loader_key("click"));
        figma_action_t * action;

        if(node_id.size == 0u || action_id.size == 0u)
        {
            FIGMA_DIAGNOSTIC_ADD(
                &document->diagnostics,
                FIGMA_DIAGNOSTIC_WARNING,
                "ux_action_skipped",
                "Action entry requires nodeId and actionId",
                "");
            continue;
        }

        action = (figma_action_t *)figma_array_push_uninitialized(
            document->memory, actions);

        if(action == NULL)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }

        memset(action, 0, sizeof(*action));
        figma_string_init(&action->node_id);
        figma_string_init(&action->action_id);
        figma_string_init(&action->target_frame_id);
        action->event_type = __figma_loader_parse_trigger(trigger);

        if(figma_string_assign(
               document->memory, &action->node_id, node_id) == FIGMA_FALSE ||
            figma_string_assign(
               document->memory,
               &action->action_id,
               action_id) == FIGMA_FALSE ||
            figma_string_assign(
               document->memory,
               &action->target_frame_id,
               target_frame_id) == FIGMA_FALSE)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }

        if(action->event_type == FIGMA_PROTOTYPE_EVENT_KEY_DOWN)
        {
            action->key_code = (uint32_t)figma_json_number(
                figma_json_member(item, __figma_loader_key("keyCode")), 0.0);
        }
        else if(action->event_type == FIGMA_PROTOTYPE_EVENT_UNSUPPORTED)
        {
            FIGMA_DIAGNOSTIC_ADD(
                &document->diagnostics,
                FIGMA_DIAGNOSTIC_WARNING,
                "ux_action_trigger_unsupported",
                "Action entry has an unsupported trigger",
                "");
        }
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_document_load_ux(figma_document_t * document, figma_string_view_t data)
{
    figma_json_document_t json;
    figma_array_t bindings;
    figma_array_t actions;
    const js_element_t * root;
    const js_element_t * bindings_json;
    const js_element_t * actions_json;
    figma_result_t result;

    if(document == NULL || data.data == NULL || data.size == 0u)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    figma_json_document_init(&json);
    figma_array_init(&bindings, sizeof(figma_binding_t));
    figma_array_init(&actions, sizeof(figma_action_t));
    result = figma_json_parse(
        document->memory, data, &document->diagnostics, &json);

    if(result != FIGMA_RESULT_OK)
    {
        goto cleanup;
    }

    root = json.root;

    if(root == NULL || js_is_object(root) != JS_TRUE)
    {
        FIGMA_DIAGNOSTIC_ADD(
            &document->diagnostics,
            FIGMA_DIAGNOSTIC_ERROR,
            "ux_root_invalid",
            "UX data root must be a JSON object",
            "");
        result = FIGMA_RESULT_PARSE_FAILED;
        goto cleanup;
    }

    bindings_json =
        figma_json_member(root, __figma_loader_key("bindings"));
    actions_json =
        figma_json_member(root, __figma_loader_key("actions"));

    if(bindings_json != NULL && js_is_array(bindings_json) != JS_TRUE)
    {
        FIGMA_DIAGNOSTIC_ADD(
            &document->diagnostics,
            FIGMA_DIAGNOSTIC_ERROR,
            "ux_bindings_invalid",
            "UX bindings must be a JSON array",
            "");
        result = FIGMA_RESULT_PARSE_FAILED;
        goto cleanup;
    }

    if(actions_json != NULL && js_is_array(actions_json) != JS_TRUE)
    {
        FIGMA_DIAGNOSTIC_ADD(
            &document->diagnostics,
            FIGMA_DIAGNOSTIC_ERROR,
            "ux_actions_invalid",
            "UX actions must be a JSON array",
            "");
        result = FIGMA_RESULT_PARSE_FAILED;
        goto cleanup;
    }

    result = __figma_loader_parse_bindings(
        document, bindings_json, &bindings);

    if(result != FIGMA_RESULT_OK)
    {
        goto cleanup;
    }

    result = __figma_loader_parse_actions(
        document, actions_json, &actions);

    if(result != FIGMA_RESULT_OK)
    {
        goto cleanup;
    }

    figma_array_destroy(
        document->memory,
        &document->bindings,
        &__figma_loader_binding_destroy);
    document->bindings = bindings;
    figma_array_init(&bindings, sizeof(figma_binding_t));
    figma_array_destroy(
        document->memory,
        &document->actions,
        &__figma_loader_action_destroy);
    document->actions = actions;
    figma_array_init(&actions, sizeof(figma_action_t));
    result = FIGMA_RESULT_OK;

cleanup:
    figma_array_destroy(document->memory, &actions, &__figma_loader_action_destroy);
    figma_array_destroy(
        document->memory, &bindings, &__figma_loader_binding_destroy);
    figma_json_document_destroy(&json);
    return result;
}

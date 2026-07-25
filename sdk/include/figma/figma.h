#pragma once

#include <stddef.h>
#include <stdint.h>

#if defined(FIGMA_STATIC)
#   define FIGMA_API
#elif defined(_WIN32)
#   if defined(FIGMA_BUILDING_SDK)
#       define FIGMA_API __declspec(dllexport)
#   else
#       define FIGMA_API __declspec(dllimport)
#   endif
#elif defined(FIGMA_BUILDING_SDK)
#   define FIGMA_API __attribute__((visibility("default")))
#else
#   define FIGMA_API
#endif

#if defined(_WIN32)
#   define FIGMA_CALL __cdecl
#else
#   define FIGMA_CALL
#endif

#define FIGMA_SDK_VERSION 7u

typedef uint8_t figma_bool_t;

#define FIGMA_FALSE ((figma_bool_t)0u)
#define FIGMA_TRUE ((figma_bool_t)1u)

typedef struct figma_runtime figma_runtime_t;
typedef struct figma_document figma_document_t;
typedef struct figma_player figma_player_t;
typedef struct figma_render_list figma_render_list_t;
typedef struct figma_diagnostics figma_diagnostics_t;

typedef int32_t figma_result_t;

enum figma_result_value
{
    FIGMA_RESULT_OK = 0,
    FIGMA_RESULT_INVALID_ARGUMENT = 1,
    FIGMA_RESULT_OUT_OF_MEMORY = 2,
    FIGMA_RESULT_IO_FAILED = 3,
    FIGMA_RESULT_PARSE_FAILED = 4,
    FIGMA_RESULT_UNSUPPORTED_FORMAT = 5,
    FIGMA_RESULT_MISSING_ENTRY = 6,
    FIGMA_RESULT_NOT_FOUND = 7,
    FIGMA_RESULT_INVALID_STATE = 8,
    FIGMA_RESULT_VERSION_MISMATCH = 9
};

typedef struct figma_string_view
{
    const char * data;
    size_t size;
} figma_string_view_t;

typedef struct figma_bytes_view
{
    const uint8_t * data;
    size_t size;
} figma_bytes_view_t;

typedef struct figma_vec2f
{
    float x;
    float y;
} figma_vec2f_t;

typedef struct figma_rectf
{
    float x;
    float y;
    float w;
    float h;
} figma_rectf_t;

typedef struct figma_colorf
{
    float r;
    float g;
    float b;
    float a;
} figma_colorf_t;

typedef void * (FIGMA_CALL * figma_alloc_fn)(size_t size, void * user_data);
typedef void * (FIGMA_CALL * figma_realloc_fn)(void * ptr, size_t size, void * user_data);
typedef void (FIGMA_CALL * figma_free_fn)(void * ptr, void * user_data);

typedef struct figma_allocator_desc
{
    figma_alloc_fn alloc;
    figma_realloc_fn realloc;
    figma_free_fn free;
    void * user_data;
} figma_allocator_desc_t;

typedef struct figma_runtime_desc
{
    figma_allocator_desc_t allocator;
} figma_runtime_desc_t;

typedef struct figma_load_options
{
    figma_string_view_t source_name;
    figma_bool_t extract_image_assets;
    figma_bool_t keep_canvas_bytes;
} figma_load_options_t;

typedef struct figma_viewport_desc
{
    float width;
    float height;
    float scale;
} figma_viewport_desc_t;

typedef struct figma_player_desc
{
    figma_viewport_desc_t viewport;
    figma_string_view_t start_frame_id;
    const void * user_data;
} figma_player_desc_t;

typedef int32_t figma_binding_value_type_t;

enum figma_binding_value_type_value
{
    FIGMA_BINDING_VALUE_NONE = 0,
    FIGMA_BINDING_VALUE_TEXT = 1,
    FIGMA_BINDING_VALUE_NUMBER = 2,
    FIGMA_BINDING_VALUE_BOOLEAN = 3,
    FIGMA_BINDING_VALUE_IMAGE = 4
};

typedef struct figma_binding_value
{
    figma_binding_value_type_t type;
    figma_string_view_t string_value;
    double number_value;
    figma_bool_t bool_value;
} figma_binding_value_t;

typedef figma_bool_t (FIGMA_CALL * figma_get_binding_value_fn)(void * user_data, figma_string_view_t key, figma_binding_value_t * value);
typedef figma_bool_t (FIGMA_CALL * figma_is_binding_dirty_fn)(void * user_data, figma_string_view_t key);

typedef struct figma_data_context
{
    void * user_data;
    figma_get_binding_value_fn get_binding_value;
    figma_is_binding_dirty_fn is_binding_dirty;
} figma_data_context_t;

typedef int32_t figma_pointer_event_type_t;

enum figma_pointer_event_type_value
{
    FIGMA_POINTER_EVENT_DOWN = 0,
    FIGMA_POINTER_EVENT_MOVE = 1,
    FIGMA_POINTER_EVENT_UP = 2,
    FIGMA_POINTER_EVENT_CANCEL = 3
};

typedef int32_t figma_pointer_button_t;

enum figma_pointer_button_value
{
    FIGMA_POINTER_BUTTON_NONE = 0,
    FIGMA_POINTER_BUTTON_LEFT = 1,
    FIGMA_POINTER_BUTTON_RIGHT = 2,
    FIGMA_POINTER_BUTTON_MIDDLE = 3,
    FIGMA_POINTER_BUTTON_OTHER = 4
};

typedef uint32_t figma_input_modifier_flags_t;

#define FIGMA_INPUT_MODIFIER_NONE ((figma_input_modifier_flags_t)0u)
#define FIGMA_INPUT_MODIFIER_SHIFT ((figma_input_modifier_flags_t)(1u << 0u))
#define FIGMA_INPUT_MODIFIER_CONTROL ((figma_input_modifier_flags_t)(1u << 1u))
#define FIGMA_INPUT_MODIFIER_ALT ((figma_input_modifier_flags_t)(1u << 2u))
#define FIGMA_INPUT_MODIFIER_COMMAND ((figma_input_modifier_flags_t)(1u << 3u))

typedef struct figma_pointer_event
{
    figma_pointer_event_type_t type;
    uint32_t pointer_id;
    float x;
    float y;
    figma_pointer_button_t button;
    figma_input_modifier_flags_t modifiers;
} figma_pointer_event_t;

typedef int32_t figma_key_event_type_t;

enum figma_key_event_type_value
{
    FIGMA_KEY_EVENT_DOWN = 0,
    FIGMA_KEY_EVENT_UP = 1
};

typedef struct figma_key_event
{
    figma_key_event_type_t type;
    uint32_t key_code;
    figma_input_modifier_flags_t modifiers;
} figma_key_event_t;

typedef struct figma_input_dispatch_result
{
    figma_bool_t hit;
    figma_bool_t handled;
    figma_bool_t captured;
} figma_input_dispatch_result_t;

typedef int32_t figma_action_result_t;

enum figma_action_result_value
{
    FIGMA_ACTION_RESULT_ALLOW_DEFAULT = 0,
    FIGMA_ACTION_RESULT_CONSUME = 1,
    FIGMA_ACTION_RESULT_NAVIGATE_FRAME = 2,
    FIGMA_ACTION_RESULT_OPEN_OVERLAY = 3,
    FIGMA_ACTION_RESULT_CLOSE_OVERLAY = 4
};

typedef int32_t figma_action_input_kind_t;

enum figma_action_input_kind_value
{
    FIGMA_ACTION_INPUT_POINTER = 0,
    FIGMA_ACTION_INPUT_KEY = 1,
    FIGMA_ACTION_INPUT_TIMER = 2,
    FIGMA_ACTION_INPUT_PROGRAMMATIC = 3
};

typedef int32_t figma_trigger_type_t;

enum figma_trigger_type_value
{
    FIGMA_TRIGGER_CLICK = 0,
    FIGMA_TRIGGER_HOVER_ENTER = 1,
    FIGMA_TRIGGER_HOVER_LEAVE = 2,
    FIGMA_TRIGGER_PRESS = 3,
    FIGMA_TRIGGER_POINTER_DOWN = 4,
    FIGMA_TRIGGER_POINTER_UP = 5,
    FIGMA_TRIGGER_AFTER_TIMEOUT = 6,
    FIGMA_TRIGGER_KEY_DOWN = 7,
    FIGMA_TRIGGER_UNSUPPORTED = 8
};

typedef int32_t figma_connection_type_t;

enum figma_connection_type_value
{
    FIGMA_CONNECTION_NONE = 0,
    FIGMA_CONNECTION_INTERNAL_NODE = 1,
    FIGMA_CONNECTION_BACK = 2,
    FIGMA_CONNECTION_CLOSE = 3,
    FIGMA_CONNECTION_UNSUPPORTED = 4
};

typedef int32_t figma_navigation_type_t;

enum figma_navigation_type_value
{
    FIGMA_NAVIGATION_NAVIGATE = 0,
    FIGMA_NAVIGATION_OVERLAY = 1,
    FIGMA_NAVIGATION_SWAP = 2,
    FIGMA_NAVIGATION_SCROLL_TO = 3,
    FIGMA_NAVIGATION_UNSUPPORTED = 4
};

typedef struct figma_trigger_event
{
    figma_action_input_kind_t input_kind;
    figma_trigger_type_t trigger_type;
    figma_string_view_t interaction_id;
    figma_string_view_t source_node_id;
    figma_string_view_t current_frame_id;
    figma_pointer_event_t pointer;
    figma_key_event_t key;
    const void * player_user_data;
} figma_trigger_event_t;

typedef struct figma_action_event
{
    figma_action_input_kind_t input_kind;
    figma_trigger_type_t trigger_type;
    figma_connection_type_t connection_type;
    figma_navigation_type_t navigation_type;
    figma_string_view_t action_id;
    figma_string_view_t interaction_id;
    figma_string_view_t source_node_id;
    figma_string_view_t current_frame_id;
    figma_string_view_t target_frame_id;
    figma_pointer_event_t pointer;
    figma_key_event_t key;
    const void * player_user_data;
} figma_action_event_t;

typedef struct figma_action_response
{
    figma_action_result_t result;
    figma_string_view_t target_frame_id;
} figma_action_response_t;

typedef figma_result_t (FIGMA_CALL * figma_route_trigger_fn)(void * user_data, const figma_trigger_event_t * event);
typedef figma_result_t (FIGMA_CALL * figma_route_action_fn)(void * user_data, const figma_action_event_t * event, figma_action_response_t * response);
typedef void (FIGMA_CALL * figma_frame_changed_fn)(void * user_data, figma_string_view_t previous_frame_id, figma_string_view_t current_frame_id);
typedef void (FIGMA_CALL * figma_overlay_changed_fn)(void * user_data, figma_string_view_t frame_id);
typedef void (FIGMA_CALL * figma_state_changed_fn)(void * user_data, figma_string_view_t source_node_id, figma_string_view_t previous_state_id, figma_string_view_t current_state_id);

typedef struct figma_action_router
{
    void * user_data;
    figma_route_trigger_fn route_trigger;
    figma_route_action_fn route_action;
    figma_frame_changed_fn on_frame_changed;
    figma_overlay_changed_fn on_overlay_opened;
    figma_overlay_changed_fn on_overlay_closed;
    figma_state_changed_fn on_state_changed;
} figma_action_router_t;

typedef int32_t figma_diagnostic_severity_t;

enum figma_diagnostic_severity_value
{
    FIGMA_DIAGNOSTIC_INFO = 0,
    FIGMA_DIAGNOSTIC_WARNING = 1,
    FIGMA_DIAGNOSTIC_ERROR = 2
};

typedef struct figma_diagnostic
{
    figma_diagnostic_severity_t severity;
    figma_string_view_t code;
    figma_string_view_t message;
    figma_string_view_t node_id;
} figma_diagnostic_t;

typedef struct figma_asset_desc
{
    figma_string_view_t id;
    figma_string_view_t path;
    figma_string_view_t mime;
    figma_bytes_view_t bytes;
    uint32_t width;
    uint32_t height;
    uint8_t color_type;
} figma_asset_desc_t;

typedef int32_t figma_render_batch_type_t;

enum figma_render_batch_type_value
{
    FIGMA_RENDER_BATCH_GEOMETRY = 0,
    FIGMA_RENDER_BATCH_CLIP_BEGIN = 1,
    FIGMA_RENDER_BATCH_CLIP_END = 2
};

typedef int32_t figma_render_blend_mode_t;

enum figma_render_blend_mode_value
{
    FIGMA_RENDER_BLEND_PASS_THROUGH = 0,
    FIGMA_RENDER_BLEND_NORMAL = 1,
    FIGMA_RENDER_BLEND_MULTIPLY = 2,
    FIGMA_RENDER_BLEND_SCREEN = 3,
    FIGMA_RENDER_BLEND_OVERLAY = 4,
    FIGMA_RENDER_BLEND_DARKEN = 5,
    FIGMA_RENDER_BLEND_LIGHTEN = 6,
    FIGMA_RENDER_BLEND_COLOR_DODGE = 7,
    FIGMA_RENDER_BLEND_COLOR_BURN = 8,
    FIGMA_RENDER_BLEND_SOFT_LIGHT = 9,
    FIGMA_RENDER_BLEND_HARD_LIGHT = 10,
    FIGMA_RENDER_BLEND_DIFFERENCE = 11,
    FIGMA_RENDER_BLEND_EXCLUSION = 12,
    FIGMA_RENDER_BLEND_HUE = 13,
    FIGMA_RENDER_BLEND_SATURATION = 14,
    FIGMA_RENDER_BLEND_COLOR = 15,
    FIGMA_RENDER_BLEND_LUMINOSITY = 16,
    FIGMA_RENDER_BLEND_UNSUPPORTED = 17
};

typedef int32_t figma_render_shader_type_t;

enum figma_render_shader_type_value
{
    FIGMA_RENDER_SHADER_COLOR = 0,
    FIGMA_RENDER_SHADER_TEXTURE = 1,
    FIGMA_RENDER_SHADER_DEBUG = 2
};

typedef int32_t figma_render_texture_type_t;

enum figma_render_texture_type_value
{
    FIGMA_RENDER_TEXTURE_NONE = 0,
    FIGMA_RENDER_TEXTURE_ASSET = 1,
    FIGMA_RENDER_TEXTURE_GENERATED = 2
};

typedef int32_t figma_render_text_align_horizontal_t;

enum figma_render_text_align_horizontal_value
{
    FIGMA_RENDER_TEXT_ALIGN_HORIZONTAL_LEFT = 0,
    FIGMA_RENDER_TEXT_ALIGN_HORIZONTAL_CENTER = 1,
    FIGMA_RENDER_TEXT_ALIGN_HORIZONTAL_RIGHT = 2
};

typedef int32_t figma_render_text_align_vertical_t;

enum figma_render_text_align_vertical_value
{
    FIGMA_RENDER_TEXT_ALIGN_VERTICAL_TOP = 0,
    FIGMA_RENDER_TEXT_ALIGN_VERTICAL_CENTER = 1,
    FIGMA_RENDER_TEXT_ALIGN_VERTICAL_BOTTOM = 2
};

typedef struct figma_render_vertex
{
    float x;
    float y;
    float u;
    float v;
    figma_colorf_t color;
} figma_render_vertex_t;

typedef struct figma_render_batch_desc
{
    figma_render_batch_type_t batch_type;
    figma_render_shader_type_t shader_type;
    figma_render_texture_type_t texture_type;
    figma_string_view_t texture_key;
    figma_render_blend_mode_t blend_mode;
    float opacity;
    uint32_t render_layer_id;
    float render_layer_opacity;
    float filter_color_adjust[8];
    float paint_filter[10];
    figma_rectf_t clip_rect;
    uint32_t vertex_count;
    const figma_render_vertex_t * vertices;
    uint32_t index_count;
    const uint16_t * indices;
    figma_bool_t has_filter_color_adjust;
    figma_bool_t has_paint_filter;
} figma_render_batch_desc_t;

typedef struct figma_render_generated_text_line_desc
{
    figma_string_view_t text;
    float x;
    float y;
    float width;
    float line_height;
    float line_ascent;
} figma_render_generated_text_line_desc_t;

typedef struct figma_render_generated_texture_desc
{
    figma_string_view_t key;
    figma_string_view_t text;
    figma_string_view_t font_family;
    figma_string_view_t font_style;
    figma_string_view_t font_postscript_name;
    figma_rectf_t rect;
    figma_colorf_t color;
    figma_render_text_align_horizontal_t text_align_horizontal;
    figma_render_text_align_vertical_t text_align_vertical;
    float font_size;
    float line_height;
    int32_t font_weight;
    uint32_t text_line_count;
} figma_render_generated_texture_desc_t;

/*
 * Ownership and lifetimes:
 *   runtime outlives every document created from it;
 *   document outlives every player created for it.
 * Callback descriptors are copied by the player; user_data remains host-owned.
 * String views received by callbacks are valid only until the callback returns.
 * Asset, diagnostic, render-batch, and generated-text strings/bytes/arrays are
 * read-only borrowed views. Document-backed views remain valid until the
 * document is mutated or destroyed. Render-list-backed views remain valid
 * until the next operation that rebuilds that player's render list or until
 * the player is destroyed.
 */
FIGMA_API figma_result_t FIGMA_CALL figma_runtime_create(uint32_t version, const figma_runtime_desc_t * desc, figma_runtime_t ** runtime);
FIGMA_API void FIGMA_CALL figma_runtime_destroy(figma_runtime_t * runtime);
FIGMA_API figma_result_t FIGMA_CALL figma_runtime_get_desc(const figma_runtime_t * runtime, figma_runtime_desc_t * desc);
FIGMA_API figma_result_t FIGMA_CALL figma_runtime_load_document_from_fig_data(figma_runtime_t * runtime, const void * data, size_t size, const figma_load_options_t * options, figma_document_t ** document);
FIGMA_API figma_result_t FIGMA_CALL figma_runtime_create_player(figma_runtime_t * runtime, figma_document_t * document, const figma_player_desc_t * desc, figma_player_t ** player);

FIGMA_API void FIGMA_CALL figma_document_destroy(figma_document_t * document);
FIGMA_API figma_result_t FIGMA_CALL figma_document_load_ux(figma_document_t * document, figma_string_view_t data);
FIGMA_API figma_bool_t FIGMA_CALL figma_document_get_frame_rect(const figma_document_t * document, figma_string_view_t node_id, figma_rectf_t * rect);
FIGMA_API figma_bool_t FIGMA_CALL figma_document_get_prototype_start_frame_rect(const figma_document_t * document, figma_rectf_t * rect);
FIGMA_API figma_bool_t FIGMA_CALL figma_document_find_asset(const figma_document_t * document, figma_string_view_t asset_id, figma_asset_desc_t * asset);
FIGMA_API const figma_diagnostics_t * FIGMA_CALL figma_document_get_diagnostics(const figma_document_t * document);

FIGMA_API void FIGMA_CALL figma_player_destroy(figma_player_t * player);
FIGMA_API figma_result_t FIGMA_CALL figma_player_set_action_router(figma_player_t * player, const figma_action_router_t * router);
FIGMA_API figma_result_t FIGMA_CALL figma_player_set_data_context(figma_player_t * player, const figma_data_context_t * context);
FIGMA_API figma_result_t FIGMA_CALL figma_player_set_viewport(figma_player_t * player, const figma_viewport_desc_t * viewport);
FIGMA_API figma_result_t FIGMA_CALL figma_player_hit_test(const figma_player_t * player, float x, float y, figma_bool_t * hit);
FIGMA_API figma_result_t FIGMA_CALL figma_player_input_pointer(figma_player_t * player, const figma_pointer_event_t * event, figma_input_dispatch_result_t * dispatch);
FIGMA_API figma_result_t FIGMA_CALL figma_player_input_key(figma_player_t * player, const figma_key_event_t * event, figma_input_dispatch_result_t * dispatch);
FIGMA_API figma_result_t FIGMA_CALL figma_player_update(figma_player_t * player, float dt);
FIGMA_API figma_result_t FIGMA_CALL figma_player_restart(figma_player_t * player);
FIGMA_API figma_result_t FIGMA_CALL figma_player_navigate_to_frame(figma_player_t * player, figma_string_view_t target_frame_id);
FIGMA_API figma_result_t FIGMA_CALL figma_player_open_overlay(figma_player_t * player, figma_string_view_t target_frame_id);
FIGMA_API figma_result_t FIGMA_CALL figma_player_close_overlay(figma_player_t * player);
FIGMA_API figma_result_t FIGMA_CALL figma_player_go_back(figma_player_t * player);
FIGMA_API figma_result_t FIGMA_CALL figma_player_set_text(figma_player_t * player, figma_string_view_t key, figma_string_view_t value);
FIGMA_API figma_result_t FIGMA_CALL figma_player_set_number(figma_player_t * player, figma_string_view_t key, double value);
FIGMA_API figma_result_t FIGMA_CALL figma_player_set_visible(figma_player_t * player, figma_string_view_t key, figma_bool_t value);
FIGMA_API figma_result_t FIGMA_CALL figma_player_set_enabled(figma_player_t * player, figma_string_view_t key, figma_bool_t value);
FIGMA_API figma_result_t FIGMA_CALL figma_player_set_image(figma_player_t * player, figma_string_view_t key, figma_string_view_t asset_id);
FIGMA_API figma_result_t FIGMA_CALL figma_player_set_state(figma_player_t * player, figma_string_view_t key, figma_bool_t value);
FIGMA_API figma_result_t FIGMA_CALL figma_player_set_binding_value(figma_player_t * player, figma_string_view_t key, const figma_binding_value_t * value);
FIGMA_API figma_result_t FIGMA_CALL figma_player_clear_binding_value(figma_player_t * player, figma_string_view_t key);
FIGMA_API const figma_render_list_t * FIGMA_CALL figma_player_get_render_list(const figma_player_t * player);
FIGMA_API const figma_diagnostics_t * FIGMA_CALL figma_player_get_diagnostics(const figma_player_t * player);

FIGMA_API uint32_t FIGMA_CALL figma_render_list_get_batch_count(const figma_render_list_t * render_list);
FIGMA_API figma_result_t FIGMA_CALL figma_render_list_get_batch(const figma_render_list_t * render_list, uint32_t index, figma_render_batch_desc_t * batch);
FIGMA_API figma_result_t FIGMA_CALL figma_render_list_get_generated_texture(const figma_render_list_t * render_list, uint32_t index, figma_render_generated_texture_desc_t * desc);
FIGMA_API figma_result_t FIGMA_CALL figma_render_list_get_generated_texture_text_line(const figma_render_list_t * render_list, uint32_t index, uint32_t line_index, figma_render_generated_text_line_desc_t * line);

FIGMA_API uint32_t FIGMA_CALL figma_diagnostics_get_count(const figma_diagnostics_t * diagnostics);
FIGMA_API figma_bool_t FIGMA_CALL figma_diagnostics_has_errors(const figma_diagnostics_t * diagnostics);
FIGMA_API figma_result_t FIGMA_CALL figma_diagnostics_get(const figma_diagnostics_t * diagnostics, uint32_t index, figma_diagnostic_t * diagnostic);

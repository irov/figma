#pragma once

#include "figma_internal.h"

typedef enum figma_binding_property
{
    FIGMA_BINDING_PROPERTY_TEXT = 0,
    FIGMA_BINDING_PROPERTY_VISIBLE = 1,
    FIGMA_BINDING_PROPERTY_ENABLED = 2,
    FIGMA_BINDING_PROPERTY_SELECTED = 3,
    FIGMA_BINDING_PROPERTY_IMAGE = 4
} figma_binding_property_t;

typedef enum figma_canvas_mask_type
{
    FIGMA_CANVAS_MASK_ALPHA = 0,
    FIGMA_CANVAS_MASK_OUTLINE = 1,
    FIGMA_CANVAS_MASK_LUMINANCE = 2,
    FIGMA_CANVAS_MASK_UNKNOWN = 3
} figma_canvas_mask_type_t;

typedef enum figma_animation_track_type
{
    FIGMA_ANIMATION_TRACK_OPACITY = 0,
    FIGMA_ANIMATION_TRACK_TRANSFORM = 1,
    FIGMA_ANIMATION_TRACK_RECT = 2,
    FIGMA_ANIMATION_TRACK_ARC = 3,
    FIGMA_ANIMATION_TRACK_COLOR = 4,
    FIGMA_ANIMATION_TRACK_VISIBILITY = 5
} figma_animation_track_type_t;

typedef enum figma_animation_source
{
    FIGMA_ANIMATION_SOURCE_NONE = 0,
    FIGMA_ANIMATION_SOURCE_PROTOTYPE_TRANSITION = 1,
    FIGMA_ANIMATION_SOURCE_SMART_ANIMATE = 2
} figma_animation_source_t;

typedef struct figma_canvas_paint
{
    figma_canvas_paint_type_t type;
    figma_canvas_blend_mode_t blend_mode;
    figma_canvas_image_scale_mode_t image_scale_mode;
    figma_colorf_t color;
    float opacity;
    figma_bool_t visible;
    figma_string_t asset_id;
    figma_string_t raw_type;
    figma_string_t raw_blend_mode;
    float transform[6];
    float filter_color_adjust[8];
    float paint_filter[10];
    uint32_t original_image_width;
    uint32_t original_image_height;
    figma_bool_t has_transform;
    figma_bool_t has_filter_color_adjust;
    figma_bool_t has_paint_filter;
} figma_canvas_paint_t;

typedef struct figma_canvas_path_style_override
{
    uint32_t style_id;
    figma_array_t fills;
    figma_array_t strokes;
} figma_canvas_path_style_override_t;

typedef struct figma_canvas_path
{
    figma_canvas_winding_rule_t winding_rule;
    uint32_t commands_blob;
    uint32_t style_id;
    figma_bool_t commands_decoded;
    figma_array_t commands;
    figma_array_t paints;
} figma_canvas_path_t;

typedef struct figma_prototype_action
{
    figma_string_t target_node_id;
    figma_string_t raw_connection_type;
    figma_string_t raw_navigation_type;
    figma_string_t raw_transition_type;
    figma_string_t raw_transition_direction;
    figma_string_t raw_transition_easing;
    figma_prototype_connection_type_t connection_type;
    figma_prototype_navigation_type_t navigation_type;
    figma_prototype_transition_type_t transition_type;
    figma_prototype_transition_direction_t transition_direction;
    figma_animation_easing_t transition_easing;
    float transition_duration;
    figma_bool_t smart_animate;
    figma_bool_t transition_preserve_scroll;
    figma_bool_t transition_reset_video_position;
    figma_bool_t has_easing_function;
    figma_array_t unsupported_fields;
} figma_prototype_action_t;

typedef struct figma_prototype_interaction
{
    figma_string_t id;
    figma_string_t raw_event_type;
    figma_prototype_event_type_t event_type;
    float transition_timeout;
    uint32_t key_code;
    figma_array_t actions;
    figma_array_t unsupported_fields;
} figma_prototype_interaction_t;

typedef struct figma_canvas_text_line
{
    figma_string_t text;
    float x;
    float y;
    float width;
    float line_height;
    float line_ascent;
} figma_canvas_text_line_t;

typedef struct figma_canvas_node
{
    figma_string_t id;
    figma_string_t name;
    figma_canvas_node_type_t type;
    figma_vec2f_t size;
    figma_rectf_t rect;
    figma_vec2f_t quad[4];
    float opacity;
    float corner_radius;
    float stroke_weight;
    float font_size;
    float line_height;
    int32_t font_weight;
    figma_bool_t visible;
    figma_bool_t mask;
    figma_bool_t frame_mask_disabled;
    figma_bool_t has_fill_geometry;
    figma_bool_t has_stroke_geometry;
    figma_bool_t has_vector_data;
    figma_bool_t has_vector_network_blob;
    figma_bool_t has_prototype_starting_point;
    uint32_t vector_network_blob;
    uint32_t prototype_interaction_count;
    figma_vec2f_t vector_normalized_size;
    figma_canvas_mask_type_t mask_type;
    figma_canvas_blend_mode_t blend_mode;
    figma_canvas_stroke_align_t stroke_align;
    figma_canvas_stroke_cap_t stroke_cap;
    figma_canvas_stroke_join_t stroke_join;
    figma_canvas_arc_data_t arc_data;
    figma_canvas_text_align_horizontal_t text_align_horizontal;
    figma_canvas_text_align_vertical_t text_align_vertical;
    figma_string_t text;
    figma_string_t font_family;
    figma_string_t font_style;
    figma_string_t font_postscript_name;
    figma_string_t prototype_start_node_id;
    figma_string_t symbol_id;
    figma_string_t fill_style_node_id;
    figma_string_t stroke_fill_style_node_id;
    figma_string_t raw_blend_mode;
    figma_array_t dash_pattern;
    figma_array_t path_style_overrides;
    figma_array_t fill_geometry;
    figma_array_t stroke_geometry;
    figma_array_t prototype_interactions;
    figma_array_t text_lines;
    figma_array_t fills;
    figma_array_t strokes;
    figma_array_t children;
} figma_canvas_node_t;

typedef struct figma_binding
{
    figma_string_t node_id;
    figma_string_t key;
    figma_binding_property_t property;
} figma_binding_t;

typedef struct figma_action
{
    figma_string_t node_id;
    figma_string_t action_id;
    figma_string_t target_frame_id;
    figma_prototype_event_type_t event_type;
    uint32_t key_code;
} figma_action_t;

typedef struct figma_asset
{
    figma_string_t id;
    figma_string_t path;
    figma_string_t mime;
    figma_array_t bytes;
    uint32_t width;
    uint32_t height;
    uint8_t color_type;
} figma_asset_t;

typedef struct figma_animation_track
{
    figma_string_t node_id;
    figma_string_t target_node_id;
    figma_animation_track_type_t type;
    float from[4];
    float to[4];
    figma_vec2f_t from_quad[4];
    figma_vec2f_t to_quad[4];
    const figma_canvas_node_t * persistent_source_node;
    figma_bool_t has_quad;
    figma_bool_t persistent;
} figma_animation_track_t;

typedef struct figma_animation_clip
{
    figma_string_t id;
    figma_string_t source_frame_id;
    figma_string_t target_frame_id;
    figma_string_t source_node_id;
    figma_animation_source_t source;
    figma_prototype_transition_type_t transition_type;
    figma_prototype_transition_direction_t transition_direction;
    figma_animation_easing_t easing;
    float duration;
    figma_bool_t smart_animate;
    figma_array_t tracks;
} figma_animation_clip_t;

typedef struct figma_player_animation_state
{
    figma_animation_clip_t clip;
    float elapsed;
    float progress;
    figma_bool_t active;
} figma_player_animation_state_t;

typedef struct figma_render_text_line
{
    figma_string_t text;
    float x;
    float y;
    float width;
    float line_height;
    float line_ascent;
} figma_render_text_line_t;

typedef struct figma_render_command
{
    figma_render_command_type_t type;
    figma_string_t id;
    figma_string_t node_id;
    figma_string_t asset_id;
    figma_string_t text;
    figma_string_t font_family;
    figma_string_t font_style;
    figma_string_t font_postscript_name;
    figma_rectf_t rect;
    figma_colorf_t color;
    figma_render_shape_type_t shape;
    figma_render_text_align_horizontal_t text_align_horizontal;
    figma_render_text_align_vertical_t text_align_vertical;
    figma_render_blend_mode_t blend_mode;
    figma_render_image_scale_mode_t image_scale_mode;
    float corner_radius;
    float font_size;
    float line_height;
    int32_t font_weight;
    float stroke_width;
    float opacity;
    uint32_t render_layer_id;
    float render_layer_opacity;
    float arc_starting_angle;
    float arc_ending_angle;
    float arc_inner_radius;
    float image_transform[6];
    float filter_color_adjust[8];
    float paint_filter[10];
    uint32_t original_image_width;
    uint32_t original_image_height;
    figma_bool_t has_arc_data;
    figma_bool_t has_image_transform;
    figma_bool_t has_filter_color_adjust;
    figma_bool_t has_paint_filter;
    figma_array_t text_lines;
    figma_array_t vertices;
    figma_array_t indices;
} figma_render_command_t;

struct figma_render_list
{
    figma_memory_t * memory;
    figma_array_t commands;
};

struct figma_document
{
    figma_runtime_t * runtime;
    figma_memory_t * memory;
    figma_string_t path;
    figma_string_t file_name;
    figma_rectf_t render_coordinates;
    figma_vec2f_t thumbnail_size;
    figma_canvas_node_t canvas_root;
    figma_string_t prototype_start_node_id;
    figma_array_t assets;
    figma_array_t canvas_bytes;
    figma_array_t bindings;
    figma_array_t actions;
    figma_diagnostics_t diagnostics;
    char canvas_version;
    figma_bool_t has_canvas_root;
};

void figma_canvas_paint_init(figma_canvas_paint_t * paint);
void figma_canvas_paint_destroy(figma_memory_t * memory, void * paint);
figma_bool_t figma_canvas_paint_copy(figma_memory_t * memory, figma_canvas_paint_t * target, const figma_canvas_paint_t * source);

void figma_canvas_path_init(figma_canvas_path_t * path);
void figma_canvas_path_destroy(figma_memory_t * memory, void * path);
figma_bool_t figma_canvas_path_copy(figma_memory_t * memory, figma_canvas_path_t * target, const figma_canvas_path_t * source);

void figma_prototype_action_init(figma_prototype_action_t * action);
void figma_prototype_action_destroy(figma_memory_t * memory, void * action);
figma_bool_t figma_prototype_action_copy(figma_memory_t * memory, figma_prototype_action_t * target, const figma_prototype_action_t * source);

void figma_prototype_interaction_init(figma_prototype_interaction_t * interaction);
void figma_prototype_interaction_destroy(figma_memory_t * memory, void * interaction);
figma_bool_t figma_prototype_interaction_copy(figma_memory_t * memory, figma_prototype_interaction_t * target, const figma_prototype_interaction_t * source);

void figma_canvas_node_init(figma_canvas_node_t * node);
void figma_canvas_node_destroy(figma_memory_t * memory, void * node);
figma_bool_t figma_canvas_node_copy(figma_memory_t * memory, figma_canvas_node_t * target, const figma_canvas_node_t * source);
const figma_canvas_node_t * figma_canvas_node_find(const figma_canvas_node_t * node, figma_string_view_t id);

void figma_animation_track_init(figma_animation_track_t * track);
void figma_animation_track_destroy(figma_memory_t * memory, void * track);
figma_bool_t figma_animation_track_copy(figma_memory_t * memory, figma_animation_track_t * target, const figma_animation_track_t * source);
void figma_animation_clip_init(figma_animation_clip_t * clip);
void figma_animation_clip_destroy(figma_memory_t * memory, figma_animation_clip_t * clip);
void figma_player_animation_state_init(figma_player_animation_state_t * state);
void figma_player_animation_state_destroy(figma_memory_t * memory, figma_player_animation_state_t * state);

void figma_render_command_init(figma_render_command_t * command, figma_render_command_type_t type);
void figma_render_command_destroy(figma_memory_t * memory, void * command);
void figma_render_list_init(figma_render_list_t * render_list, figma_memory_t * memory);
void figma_render_list_destroy(figma_render_list_t * render_list);
void figma_render_list_clear(figma_render_list_t * render_list);
figma_render_command_t * figma_render_list_add(figma_render_list_t * render_list, figma_render_command_type_t type);
void figma_render_list_remove_last(figma_render_list_t * render_list);

void figma_document_init(figma_document_t * document, figma_runtime_t * runtime, figma_memory_t * memory);
void figma_document_deinit(figma_document_t * document);
const figma_canvas_node_t * figma_document_find_canvas_node(const figma_document_t * document, figma_string_view_t id);
const figma_canvas_node_t * figma_document_get_prototype_start_frame(const figma_document_t * document);
const figma_asset_t * figma_document_find_asset_internal(const figma_document_t * document, figma_string_view_t asset_id);

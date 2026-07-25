#pragma once

#include "figma/figma.h"

/*
 * Private tooling API. It is available to repository-owned tools and the
 * viewer, but is not part of the installed SDK integration contract.
 * Every pointer and string view is borrowed from its document, player, or
 * render list and is invalidated when that owner is mutated or destroyed.
 */

typedef struct figma_inspection_node figma_inspection_node_t;
typedef struct figma_inspection_path figma_inspection_path_t;
typedef struct figma_inspection_interaction figma_inspection_interaction_t;

typedef enum figma_canvas_node_type
{
    FIGMA_CANVAS_NODE_UNKNOWN = 0,
    FIGMA_CANVAS_NODE_DOCUMENT = 1,
    FIGMA_CANVAS_NODE_CANVAS = 2,
    FIGMA_CANVAS_NODE_FRAME = 3,
    FIGMA_CANVAS_NODE_GROUP = 4,
    FIGMA_CANVAS_NODE_RECTANGLE = 5,
    FIGMA_CANVAS_NODE_ROUNDED_RECTANGLE = 6,
    FIGMA_CANVAS_NODE_ELLIPSE = 7,
    FIGMA_CANVAS_NODE_TEXT = 8,
    FIGMA_CANVAS_NODE_VECTOR = 9
} figma_canvas_node_type_t;

typedef enum figma_canvas_paint_type
{
    FIGMA_CANVAS_PAINT_UNSUPPORTED = 0,
    FIGMA_CANVAS_PAINT_SOLID = 1,
    FIGMA_CANVAS_PAINT_IMAGE = 2
} figma_canvas_paint_type_t;

typedef enum figma_canvas_blend_mode
{
    FIGMA_CANVAS_BLEND_PASS_THROUGH = 0,
    FIGMA_CANVAS_BLEND_NORMAL = 1,
    FIGMA_CANVAS_BLEND_MULTIPLY = 2,
    FIGMA_CANVAS_BLEND_SCREEN = 3,
    FIGMA_CANVAS_BLEND_OVERLAY = 4,
    FIGMA_CANVAS_BLEND_DARKEN = 5,
    FIGMA_CANVAS_BLEND_LIGHTEN = 6,
    FIGMA_CANVAS_BLEND_COLOR_DODGE = 7,
    FIGMA_CANVAS_BLEND_COLOR_BURN = 8,
    FIGMA_CANVAS_BLEND_SOFT_LIGHT = 9,
    FIGMA_CANVAS_BLEND_HARD_LIGHT = 10,
    FIGMA_CANVAS_BLEND_DIFFERENCE = 11,
    FIGMA_CANVAS_BLEND_EXCLUSION = 12,
    FIGMA_CANVAS_BLEND_HUE = 13,
    FIGMA_CANVAS_BLEND_SATURATION = 14,
    FIGMA_CANVAS_BLEND_COLOR = 15,
    FIGMA_CANVAS_BLEND_LUMINOSITY = 16,
    FIGMA_CANVAS_BLEND_UNSUPPORTED = 17
} figma_canvas_blend_mode_t;

typedef enum figma_canvas_image_scale_mode
{
    FIGMA_CANVAS_IMAGE_SCALE_STRETCH = 0,
    FIGMA_CANVAS_IMAGE_SCALE_FIT = 1,
    FIGMA_CANVAS_IMAGE_SCALE_FILL = 2,
    FIGMA_CANVAS_IMAGE_SCALE_TILE = 3,
    FIGMA_CANVAS_IMAGE_SCALE_UNKNOWN = 4
} figma_canvas_image_scale_mode_t;

typedef enum figma_canvas_stroke_align
{
    FIGMA_CANVAS_STROKE_ALIGN_CENTER = 0,
    FIGMA_CANVAS_STROKE_ALIGN_INSIDE = 1,
    FIGMA_CANVAS_STROKE_ALIGN_OUTSIDE = 2,
    FIGMA_CANVAS_STROKE_ALIGN_UNSUPPORTED = 3
} figma_canvas_stroke_align_t;

typedef enum figma_canvas_stroke_cap
{
    FIGMA_CANVAS_STROKE_CAP_NONE = 0,
    FIGMA_CANVAS_STROKE_CAP_ROUND = 1,
    FIGMA_CANVAS_STROKE_CAP_SQUARE = 2,
    FIGMA_CANVAS_STROKE_CAP_UNSUPPORTED = 3
} figma_canvas_stroke_cap_t;

typedef enum figma_canvas_stroke_join
{
    FIGMA_CANVAS_STROKE_JOIN_MITER = 0,
    FIGMA_CANVAS_STROKE_JOIN_BEVEL = 1,
    FIGMA_CANVAS_STROKE_JOIN_ROUND = 2,
    FIGMA_CANVAS_STROKE_JOIN_UNSUPPORTED = 3
} figma_canvas_stroke_join_t;

typedef enum figma_canvas_text_align_horizontal
{
    FIGMA_CANVAS_TEXT_ALIGN_HORIZONTAL_LEFT = 0,
    FIGMA_CANVAS_TEXT_ALIGN_HORIZONTAL_CENTER = 1,
    FIGMA_CANVAS_TEXT_ALIGN_HORIZONTAL_RIGHT = 2
} figma_canvas_text_align_horizontal_t;

typedef enum figma_canvas_text_align_vertical
{
    FIGMA_CANVAS_TEXT_ALIGN_VERTICAL_TOP = 0,
    FIGMA_CANVAS_TEXT_ALIGN_VERTICAL_CENTER = 1,
    FIGMA_CANVAS_TEXT_ALIGN_VERTICAL_BOTTOM = 2
} figma_canvas_text_align_vertical_t;

typedef enum figma_canvas_winding_rule
{
    FIGMA_CANVAS_WINDING_NON_ZERO = 0,
    FIGMA_CANVAS_WINDING_ODD = 1
} figma_canvas_winding_rule_t;

typedef enum figma_canvas_path_command_type
{
    FIGMA_CANVAS_PATH_MOVE_TO = 0,
    FIGMA_CANVAS_PATH_LINE_TO = 1,
    FIGMA_CANVAS_PATH_QUADRATIC_TO = 2,
    FIGMA_CANVAS_PATH_CUBIC_TO = 3,
    FIGMA_CANVAS_PATH_CLOSE = 4
} figma_canvas_path_command_type_t;

typedef struct figma_canvas_path_command
{
    figma_canvas_path_command_type_t type;
    figma_vec2f_t p0;
    figma_vec2f_t p1;
    figma_vec2f_t p2;
} figma_canvas_path_command_t;

typedef struct figma_canvas_arc_data
{
    float starting_angle;
    float ending_angle;
    float inner_radius;
    figma_bool_t valid;
} figma_canvas_arc_data_t;

typedef enum figma_prototype_event_type
{
    FIGMA_PROTOTYPE_EVENT_CLICK = 0,
    FIGMA_PROTOTYPE_EVENT_HOVER_ENTER = 1,
    FIGMA_PROTOTYPE_EVENT_HOVER_LEAVE = 2,
    FIGMA_PROTOTYPE_EVENT_PRESS = 3,
    FIGMA_PROTOTYPE_EVENT_POINTER_DOWN = 4,
    FIGMA_PROTOTYPE_EVENT_POINTER_UP = 5,
    FIGMA_PROTOTYPE_EVENT_AFTER_TIMEOUT = 6,
    FIGMA_PROTOTYPE_EVENT_KEY_DOWN = 7,
    FIGMA_PROTOTYPE_EVENT_UNSUPPORTED = 8
} figma_prototype_event_type_t;

typedef enum figma_prototype_connection_type
{
    FIGMA_PROTOTYPE_CONNECTION_NONE = 0,
    FIGMA_PROTOTYPE_CONNECTION_INTERNAL_NODE = 1,
    FIGMA_PROTOTYPE_CONNECTION_BACK = 2,
    FIGMA_PROTOTYPE_CONNECTION_CLOSE = 3,
    FIGMA_PROTOTYPE_CONNECTION_UNSUPPORTED = 4
} figma_prototype_connection_type_t;

typedef enum figma_prototype_navigation_type
{
    FIGMA_PROTOTYPE_NAVIGATION_NAVIGATE = 0,
    FIGMA_PROTOTYPE_NAVIGATION_OVERLAY = 1,
    FIGMA_PROTOTYPE_NAVIGATION_SWAP = 2,
    FIGMA_PROTOTYPE_NAVIGATION_SCROLL_TO = 3,
    FIGMA_PROTOTYPE_NAVIGATION_UNSUPPORTED = 4
} figma_prototype_navigation_type_t;

typedef enum figma_prototype_transition_type
{
    FIGMA_PROTOTYPE_TRANSITION_INSTANT = 0,
    FIGMA_PROTOTYPE_TRANSITION_DISSOLVE = 1,
    FIGMA_PROTOTYPE_TRANSITION_SMART_ANIMATE = 2,
    FIGMA_PROTOTYPE_TRANSITION_MOVE_IN = 3,
    FIGMA_PROTOTYPE_TRANSITION_MOVE_OUT = 4,
    FIGMA_PROTOTYPE_TRANSITION_PUSH = 5,
    FIGMA_PROTOTYPE_TRANSITION_SLIDE_IN = 6,
    FIGMA_PROTOTYPE_TRANSITION_SLIDE_OUT = 7,
    FIGMA_PROTOTYPE_TRANSITION_UNSUPPORTED = 8
} figma_prototype_transition_type_t;

typedef enum figma_prototype_transition_direction
{
    FIGMA_PROTOTYPE_TRANSITION_DIRECTION_NONE = 0,
    FIGMA_PROTOTYPE_TRANSITION_DIRECTION_LEFT = 1,
    FIGMA_PROTOTYPE_TRANSITION_DIRECTION_RIGHT = 2,
    FIGMA_PROTOTYPE_TRANSITION_DIRECTION_UP = 3,
    FIGMA_PROTOTYPE_TRANSITION_DIRECTION_DOWN = 4,
    FIGMA_PROTOTYPE_TRANSITION_DIRECTION_UNSUPPORTED = 5
} figma_prototype_transition_direction_t;

typedef enum figma_animation_easing
{
    FIGMA_ANIMATION_EASING_LINEAR = 0,
    FIGMA_ANIMATION_EASING_EASE_IN = 1,
    FIGMA_ANIMATION_EASING_EASE_OUT = 2,
    FIGMA_ANIMATION_EASING_EASE_IN_OUT = 3,
    FIGMA_ANIMATION_EASING_IN_CUBIC = 4,
    FIGMA_ANIMATION_EASING_OUT_CUBIC = 5,
    FIGMA_ANIMATION_EASING_IN_OUT_CUBIC = 6,
    FIGMA_ANIMATION_EASING_UNSUPPORTED = 7
} figma_animation_easing_t;

typedef enum figma_render_command_type
{
    FIGMA_RENDER_COMMAND_FILL = 0,
    FIGMA_RENDER_COMMAND_STROKE = 1,
    FIGMA_RENDER_COMMAND_IMAGE = 2,
    FIGMA_RENDER_COMMAND_TEXT = 3,
    FIGMA_RENDER_COMMAND_MESH = 4,
    FIGMA_RENDER_COMMAND_CLIP_BEGIN = 5,
    FIGMA_RENDER_COMMAND_CLIP_END = 6,
    FIGMA_RENDER_COMMAND_DEBUG_HOTSPOT = 7
} figma_render_command_type_t;

typedef enum figma_render_shape_type
{
    FIGMA_RENDER_SHAPE_RECTANGLE = 0,
    FIGMA_RENDER_SHAPE_ROUNDED_RECTANGLE = 1,
    FIGMA_RENDER_SHAPE_ELLIPSE = 2
} figma_render_shape_type_t;

typedef enum figma_render_image_scale_mode
{
    FIGMA_RENDER_IMAGE_SCALE_STRETCH = 0,
    FIGMA_RENDER_IMAGE_SCALE_FIT = 1,
    FIGMA_RENDER_IMAGE_SCALE_FILL = 2,
    FIGMA_RENDER_IMAGE_SCALE_TILE = 3,
    FIGMA_RENDER_IMAGE_SCALE_UNKNOWN = 4
} figma_render_image_scale_mode_t;

typedef struct figma_inspection_document_desc
{
    figma_string_view_t path;
    figma_string_view_t file_name;
    figma_vec2f_t thumbnail_size;
    uint32_t asset_count;
    uint32_t binding_count;
    uint32_t action_count;
    char canvas_version;
    figma_bool_t has_canvas_bytes;
} figma_inspection_document_desc_t;

typedef struct figma_inspection_node_desc
{
    figma_string_view_t id;
    figma_string_view_t name;
    figma_canvas_node_type_t type;
    figma_vec2f_t size;
    figma_rectf_t rect;
    figma_vec2f_t quad[4];
    figma_colorf_t reserved_color;
    figma_string_view_t text;
    figma_string_view_t font_family;
    figma_string_view_t font_style;
    figma_string_view_t font_postscript_name;
    figma_string_view_t prototype_start_node_id;
    figma_string_view_t symbol_id;
    figma_string_view_t fill_style_node_id;
    figma_string_view_t stroke_fill_style_node_id;
    figma_string_view_t raw_blend_mode;
    figma_canvas_node_type_t reserved_type;
    figma_canvas_blend_mode_t blend_mode;
    figma_canvas_stroke_align_t stroke_align;
    figma_canvas_stroke_cap_t stroke_cap;
    figma_canvas_stroke_join_t stroke_join;
    figma_canvas_arc_data_t arc_data;
    figma_canvas_text_align_horizontal_t text_align_horizontal;
    figma_canvas_text_align_vertical_t text_align_vertical;
    float opacity;
    float corner_radius;
    float stroke_weight;
    float font_size;
    float line_height;
    int32_t font_weight;
    uint32_t child_count;
    uint32_t fill_count;
    uint32_t stroke_count;
    uint32_t fill_geometry_count;
    uint32_t stroke_geometry_count;
    uint32_t interaction_count;
    uint32_t text_line_count;
    figma_bool_t visible;
    figma_bool_t mask;
    figma_bool_t frame_mask_disabled;
    figma_bool_t has_fill_geometry;
    figma_bool_t has_stroke_geometry;
    figma_bool_t has_vector_data;
    figma_bool_t has_vector_network_blob;
    figma_bool_t has_prototype_starting_point;
} figma_inspection_node_desc_t;

typedef struct figma_inspection_paint_desc
{
    figma_canvas_paint_type_t type;
    figma_canvas_blend_mode_t blend_mode;
    figma_canvas_image_scale_mode_t image_scale_mode;
    figma_colorf_t color;
    figma_string_view_t asset_id;
    figma_string_view_t raw_type;
    figma_string_view_t raw_blend_mode;
    float opacity;
    float transform[6];
    float filter_color_adjust[8];
    float paint_filter[10];
    uint32_t original_image_width;
    uint32_t original_image_height;
    figma_bool_t visible;
    figma_bool_t has_transform;
    figma_bool_t has_filter_color_adjust;
    figma_bool_t has_paint_filter;
} figma_inspection_paint_desc_t;

typedef struct figma_inspection_path_desc
{
    figma_canvas_winding_rule_t winding_rule;
    uint32_t command_count;
    uint32_t paint_count;
    uint32_t commands_blob;
    uint32_t style_id;
    figma_bool_t commands_decoded;
} figma_inspection_path_desc_t;

typedef struct figma_inspection_interaction_desc
{
    figma_string_view_t id;
    figma_string_view_t raw_event_type;
    figma_prototype_event_type_t event_type;
    float transition_timeout;
    uint32_t key_code;
    uint32_t action_count;
    uint32_t unsupported_field_count;
} figma_inspection_interaction_desc_t;

typedef struct figma_inspection_action_desc
{
    figma_string_view_t target_node_id;
    figma_string_view_t raw_connection_type;
    figma_string_view_t raw_navigation_type;
    figma_string_view_t raw_transition_type;
    figma_string_view_t raw_transition_direction;
    figma_string_view_t raw_transition_easing;
    figma_prototype_connection_type_t connection_type;
    figma_prototype_navigation_type_t navigation_type;
    figma_prototype_transition_type_t transition_type;
    figma_prototype_transition_direction_t transition_direction;
    figma_animation_easing_t transition_easing;
    float transition_duration;
    uint32_t unsupported_field_count;
    figma_bool_t smart_animate;
    figma_bool_t transition_preserve_scroll;
    figma_bool_t transition_reset_video_position;
    figma_bool_t has_easing_function;
} figma_inspection_action_desc_t;

typedef struct figma_inspection_render_command_desc
{
    figma_render_command_type_t type;
    figma_string_view_t id;
    figma_string_view_t node_id;
    figma_string_view_t asset_id;
    figma_string_view_t text;
    figma_string_view_t font_family;
    figma_string_view_t font_style;
    figma_string_view_t font_postscript_name;
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
    uint32_t text_line_count;
    uint32_t vertex_count;
    const figma_render_vertex_t * vertices;
    uint32_t index_count;
    const uint16_t * indices;
    figma_bool_t has_arc_data;
    figma_bool_t has_image_transform;
    figma_bool_t has_filter_color_adjust;
    figma_bool_t has_paint_filter;
} figma_inspection_render_command_desc_t;

FIGMA_API figma_result_t FIGMA_CALL figma_inspection_get_document(const figma_document_t * document, figma_inspection_document_desc_t * desc);
FIGMA_API figma_result_t FIGMA_CALL figma_inspection_get_asset(const figma_document_t * document, uint32_t index, figma_asset_desc_t * asset);
FIGMA_API const figma_inspection_node_t * FIGMA_CALL figma_inspection_get_canvas_root(const figma_document_t * document);
FIGMA_API const figma_inspection_node_t * FIGMA_CALL figma_inspection_find_node(const figma_document_t * document, figma_string_view_t node_id);
FIGMA_API const figma_inspection_node_t * FIGMA_CALL figma_inspection_get_prototype_start_node(const figma_document_t * document);
FIGMA_API figma_result_t FIGMA_CALL figma_inspection_get_node(const figma_inspection_node_t * node, figma_inspection_node_desc_t * desc);
FIGMA_API const figma_inspection_node_t * FIGMA_CALL figma_inspection_get_child(const figma_inspection_node_t * node, uint32_t index);
FIGMA_API figma_result_t FIGMA_CALL figma_inspection_get_fill(const figma_inspection_node_t * node, uint32_t index, figma_inspection_paint_desc_t * paint);
FIGMA_API figma_result_t FIGMA_CALL figma_inspection_get_stroke(const figma_inspection_node_t * node, uint32_t index, figma_inspection_paint_desc_t * paint);
FIGMA_API const figma_inspection_path_t * FIGMA_CALL figma_inspection_get_fill_path(const figma_inspection_node_t * node, uint32_t index);
FIGMA_API const figma_inspection_path_t * FIGMA_CALL figma_inspection_get_stroke_path(const figma_inspection_node_t * node, uint32_t index);
FIGMA_API figma_result_t FIGMA_CALL figma_inspection_get_path(const figma_inspection_path_t * path, figma_inspection_path_desc_t * desc);
FIGMA_API figma_result_t FIGMA_CALL figma_inspection_get_path_command(const figma_inspection_path_t * path, uint32_t index, figma_canvas_path_command_t * command);
FIGMA_API figma_result_t FIGMA_CALL figma_inspection_get_path_paint(const figma_inspection_path_t * path, uint32_t index, figma_inspection_paint_desc_t * paint);
FIGMA_API const figma_inspection_interaction_t * FIGMA_CALL figma_inspection_get_interaction(const figma_inspection_node_t * node, uint32_t index);
FIGMA_API figma_result_t FIGMA_CALL figma_inspection_get_interaction_desc(const figma_inspection_interaction_t * interaction, figma_inspection_interaction_desc_t * desc);
FIGMA_API figma_result_t FIGMA_CALL figma_inspection_get_interaction_action(const figma_inspection_interaction_t * interaction, uint32_t index, figma_inspection_action_desc_t * action);
FIGMA_API figma_result_t FIGMA_CALL figma_inspection_get_interaction_unsupported_field(const figma_inspection_interaction_t * interaction, uint32_t index, figma_string_view_t * field);
FIGMA_API figma_result_t FIGMA_CALL figma_inspection_get_action_unsupported_field(const figma_inspection_interaction_t * interaction, uint32_t action_index, uint32_t field_index, figma_string_view_t * field);
FIGMA_API figma_result_t FIGMA_CALL figma_inspection_get_render_command(const figma_render_list_t * render_list, uint32_t index, figma_inspection_render_command_desc_t * command);
FIGMA_API float FIGMA_CALL figma_inspection_get_prototype_intro_advance_time(const figma_document_t * document);

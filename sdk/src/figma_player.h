#pragma once

#include "figma_model.h"

typedef struct figma_binding_override
{
    figma_string_t key;
    figma_binding_value_type_t type;
    figma_string_t string_value;
    double number_value;
    figma_bool_t bool_value;
} figma_binding_override_t;

typedef struct figma_player_hotspot
{
    figma_rectf_t rect;
    figma_vec2f_t quad[4];
    figma_rectf_t clip;
    figma_string_t node_id;
    figma_string_t action_id;
    figma_string_t target_frame_id;
    const figma_prototype_interaction_t * interaction;
    const figma_prototype_action_t * prototype_action;
    figma_prototype_event_type_t event_type;
    uint32_t key_code;
    figma_bool_t has_clip;
    figma_bool_t ux_action;
} figma_player_hotspot_t;

typedef struct figma_player_pointer_capture
{
    uint32_t pointer_id;
    figma_string_t node_id;
    figma_string_t interaction_id;
    figma_pointer_button_t button;
} figma_player_pointer_capture_t;

typedef struct figma_player_node_swap
{
    figma_string_t source_node_id;
    figma_string_t current_node_id;
    float started_at;
} figma_player_node_swap_t;

typedef struct figma_player_local_animation
{
    figma_string_t source_node_id;
    figma_string_t from_node_id;
    figma_string_t target_node_id;
    figma_prototype_transition_type_t transition_type;
    figma_animation_easing_t easing;
    figma_array_t tracks;
    float elapsed;
    float duration;
    float progress;
    figma_bool_t smart_animate;
    figma_bool_t active;
} figma_player_local_animation_t;

struct figma_player
{
    figma_runtime_t * runtime;
    figma_document_t * document;
    figma_memory_t * memory;
    figma_player_desc_t desc;
    figma_string_t start_frame_id;
    figma_action_router_t action_router;
    figma_data_context_t data_context;
    figma_render_list_t render_list;
    figma_diagnostics_t diagnostics;
    figma_array_t hotspots;
    figma_array_t hovered_node_ids;
    figma_array_t fired_timer_ids;
    figma_array_t node_swaps;
    figma_array_t local_animations;
    figma_array_t overrides;
    figma_array_t pointer_captures;
    figma_array_t navigation_history;
    figma_array_t overlay_frames;
    figma_array_t overlay_start_times;
    const figma_canvas_node_t * current_frame;
    figma_string_t current_frame_id;
    figma_player_animation_state_t animation_state;
    float time;
    figma_bool_t hotspots_dirty;
};

figma_result_t figma_player_rebuild_hotspots(figma_player_t * player);
figma_result_t figma_player_rebuild_render_list(figma_player_t * player);

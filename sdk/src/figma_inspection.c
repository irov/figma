#include "figma_inspection.h"
#include "figma_model.h"

#include <float.h>
#include <limits.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////////
static uint32_t __figma_inspection_count(size_t value)
{
    return value <= UINT32_MAX ? (uint32_t)value : 0u;
}

//////////////////////////////////////////////////////////////////////////
static const figma_canvas_node_t * __figma_inspection_node_internal(const figma_inspection_node_t * node)
{
    return (const figma_canvas_node_t *)(const void *)node;
}

//////////////////////////////////////////////////////////////////////////
static const figma_canvas_path_t * __figma_inspection_path_internal(const figma_inspection_path_t * path)
{
    return (const figma_canvas_path_t *)(const void *)path;
}

//////////////////////////////////////////////////////////////////////////
static const figma_prototype_interaction_t * __figma_inspection_interaction_internal(const figma_inspection_interaction_t * interaction)
{
    return (const figma_prototype_interaction_t *)(const void *)interaction;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_inspection_make_paint(const figma_canvas_paint_t * source, figma_inspection_paint_desc_t * paint)
{
    memset(paint, 0, sizeof(*paint));
    paint->type = source->type;
    paint->blend_mode = source->blend_mode;
    paint->image_scale_mode = source->image_scale_mode;
    paint->color = source->color;
    paint->asset_id = figma_string_view(&source->asset_id);
    paint->raw_type = figma_string_view(&source->raw_type);
    paint->raw_blend_mode = figma_string_view(&source->raw_blend_mode);
    paint->opacity = source->opacity;
    memcpy(paint->transform, source->transform, sizeof(paint->transform));
    memcpy(
        paint->filter_color_adjust,
        source->filter_color_adjust,
        sizeof(paint->filter_color_adjust));
    memcpy(
        paint->paint_filter,
        source->paint_filter,
        sizeof(paint->paint_filter));
    paint->original_image_width = source->original_image_width;
    paint->original_image_height = source->original_image_height;
    paint->visible = source->visible;
    paint->has_transform = source->has_transform;
    paint->has_filter_color_adjust = source->has_filter_color_adjust;
    paint->has_paint_filter = source->has_paint_filter;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_inspection_get_document(const figma_document_t * document, figma_inspection_document_desc_t * desc)
{
    if(document == NULL || desc == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    memset(desc, 0, sizeof(*desc));
    desc->path = figma_string_view(&document->path);
    desc->file_name = figma_string_view(&document->file_name);
    desc->thumbnail_size = document->thumbnail_size;
    desc->asset_count = __figma_inspection_count(document->assets.size);
    desc->binding_count = __figma_inspection_count(document->bindings.size);
    desc->action_count = __figma_inspection_count(document->actions.size);
    desc->canvas_version = document->canvas_version;
    desc->has_canvas_bytes =
        document->canvas_bytes.size != 0u ? FIGMA_TRUE : FIGMA_FALSE;
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_inspection_get_asset(const figma_document_t * document, uint32_t index, figma_asset_desc_t * asset)
{
    const figma_asset_t * source;
    if(document == NULL || asset == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    if((size_t)index >= document->assets.size)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }
    source = FIGMA_ARRAY_CONST_PTR(
        figma_asset_t, &document->assets, index);
    memset(asset, 0, sizeof(*asset));
    asset->id = figma_string_view(&source->id);
    asset->path = figma_string_view(&source->path);
    asset->mime = figma_string_view(&source->mime);
    asset->bytes.data = (const uint8_t *)source->bytes.data;
    asset->bytes.size = source->bytes.size;
    asset->width = source->width;
    asset->height = source->height;
    asset->color_type = source->color_type;
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
const figma_inspection_node_t * FIGMA_CALL figma_inspection_get_canvas_root(const figma_document_t * document)
{
    return document != NULL && document->has_canvas_root == FIGMA_TRUE
        ? (const figma_inspection_node_t *)(const void *)&document->canvas_root
        : NULL;
}

//////////////////////////////////////////////////////////////////////////
const figma_inspection_node_t * FIGMA_CALL figma_inspection_find_node(const figma_document_t * document, figma_string_view_t node_id)
{
    return (const figma_inspection_node_t *)(const void *)
        figma_document_find_canvas_node(document, node_id);
}

//////////////////////////////////////////////////////////////////////////
const figma_inspection_node_t * FIGMA_CALL figma_inspection_get_prototype_start_node(const figma_document_t * document)
{
    return (const figma_inspection_node_t *)(const void *)
        figma_document_get_prototype_start_frame(document);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_inspection_get_node(const figma_inspection_node_t * opaque, figma_inspection_node_desc_t * desc)
{
    const figma_canvas_node_t * node = __figma_inspection_node_internal(opaque);
    if(node == NULL || desc == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    memset(desc, 0, sizeof(*desc));
    desc->id = figma_string_view(&node->id);
    desc->name = figma_string_view(&node->name);
    desc->type = node->type;
    desc->size = node->size;
    desc->rect = node->rect;
    memcpy(desc->quad, node->quad, sizeof(desc->quad));
    desc->text = figma_string_view(&node->text);
    desc->font_family = figma_string_view(&node->font_family);
    desc->font_style = figma_string_view(&node->font_style);
    desc->font_postscript_name =
        figma_string_view(&node->font_postscript_name);
    desc->prototype_start_node_id =
        figma_string_view(&node->prototype_start_node_id);
    desc->symbol_id = figma_string_view(&node->symbol_id);
    desc->fill_style_node_id = figma_string_view(&node->fill_style_node_id);
    desc->stroke_fill_style_node_id =
        figma_string_view(&node->stroke_fill_style_node_id);
    desc->raw_blend_mode = figma_string_view(&node->raw_blend_mode);
    desc->blend_mode = node->blend_mode;
    desc->stroke_align = node->stroke_align;
    desc->stroke_cap = node->stroke_cap;
    desc->stroke_join = node->stroke_join;
    desc->arc_data = node->arc_data;
    desc->text_align_horizontal = node->text_align_horizontal;
    desc->text_align_vertical = node->text_align_vertical;
    desc->opacity = node->opacity;
    desc->corner_radius = node->corner_radius;
    desc->stroke_weight = node->stroke_weight;
    desc->font_size = node->font_size;
    desc->line_height = node->line_height;
    desc->font_weight = node->font_weight;
    desc->child_count = __figma_inspection_count(node->children.size);
    desc->fill_count = __figma_inspection_count(node->fills.size);
    desc->stroke_count = __figma_inspection_count(node->strokes.size);
    desc->fill_geometry_count =
        __figma_inspection_count(node->fill_geometry.size);
    desc->stroke_geometry_count =
        __figma_inspection_count(node->stroke_geometry.size);
    desc->interaction_count =
        __figma_inspection_count(node->prototype_interactions.size);
    desc->text_line_count = __figma_inspection_count(node->text_lines.size);
    desc->visible = node->visible;
    desc->mask = node->mask;
    desc->frame_mask_disabled = node->frame_mask_disabled;
    desc->has_fill_geometry = node->has_fill_geometry;
    desc->has_stroke_geometry = node->has_stroke_geometry;
    desc->has_vector_data = node->has_vector_data;
    desc->has_vector_network_blob = node->has_vector_network_blob;
    desc->has_prototype_starting_point =
        node->has_prototype_starting_point;
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
const figma_inspection_node_t * FIGMA_CALL figma_inspection_get_child(const figma_inspection_node_t * opaque, uint32_t index)
{
    const figma_canvas_node_t * node = __figma_inspection_node_internal(opaque);
    return node != NULL && (size_t)index < node->children.size
        ? (const figma_inspection_node_t *)(const void *)FIGMA_ARRAY_CONST_PTR(
              figma_canvas_node_t, &node->children, index)
        : NULL;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_inspection_get_node_paint(const figma_inspection_node_t * opaque, uint32_t index, figma_bool_t stroke, figma_inspection_paint_desc_t * paint)
{
    const figma_canvas_node_t * node = __figma_inspection_node_internal(opaque);
    const figma_array_t * values;
    if(node == NULL || paint == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    values = stroke == FIGMA_TRUE ? &node->strokes : &node->fills;
    if((size_t)index >= values->size)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }
    __figma_inspection_make_paint(
        FIGMA_ARRAY_CONST_PTR(figma_canvas_paint_t, values, index), paint);
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_inspection_get_fill(const figma_inspection_node_t * node, uint32_t index, figma_inspection_paint_desc_t * paint)
{
    return __figma_inspection_get_node_paint(
        node, index, FIGMA_FALSE, paint);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_inspection_get_stroke(const figma_inspection_node_t * node, uint32_t index, figma_inspection_paint_desc_t * paint)
{
    return __figma_inspection_get_node_paint(node, index, FIGMA_TRUE, paint);
}

//////////////////////////////////////////////////////////////////////////
static const figma_inspection_path_t * __figma_inspection_get_node_path(const figma_inspection_node_t * opaque, uint32_t index, figma_bool_t stroke)
{
    const figma_canvas_node_t * node = __figma_inspection_node_internal(opaque);
    const figma_array_t * values;
    if(node == NULL)
    {
        return NULL;
    }
    values =
        stroke == FIGMA_TRUE ? &node->stroke_geometry : &node->fill_geometry;
    return (size_t)index < values->size
        ? (const figma_inspection_path_t *)(const void *)FIGMA_ARRAY_CONST_PTR(
              figma_canvas_path_t, values, index)
        : NULL;
}

//////////////////////////////////////////////////////////////////////////
const figma_inspection_path_t * FIGMA_CALL figma_inspection_get_fill_path(const figma_inspection_node_t * node, uint32_t index)
{
    return __figma_inspection_get_node_path(node, index, FIGMA_FALSE);
}

//////////////////////////////////////////////////////////////////////////
const figma_inspection_path_t * FIGMA_CALL figma_inspection_get_stroke_path(const figma_inspection_node_t * node, uint32_t index)
{
    return __figma_inspection_get_node_path(node, index, FIGMA_TRUE);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_inspection_get_path(const figma_inspection_path_t * opaque, figma_inspection_path_desc_t * desc)
{
    const figma_canvas_path_t * path =
        __figma_inspection_path_internal(opaque);
    if(path == NULL || desc == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    memset(desc, 0, sizeof(*desc));
    desc->winding_rule = path->winding_rule;
    desc->command_count = __figma_inspection_count(path->commands.size);
    desc->paint_count = __figma_inspection_count(path->paints.size);
    desc->commands_blob = path->commands_blob;
    desc->style_id = path->style_id;
    desc->commands_decoded = path->commands_decoded;
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_inspection_get_path_command(const figma_inspection_path_t * opaque, uint32_t index, figma_canvas_path_command_t * command)
{
    const figma_canvas_path_t * path =
        __figma_inspection_path_internal(opaque);
    if(path == NULL || command == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    if((size_t)index >= path->commands.size)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }
    *command = FIGMA_ARRAY_AT(
        figma_canvas_path_command_t, &path->commands, index);
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_inspection_get_path_paint(const figma_inspection_path_t * opaque, uint32_t index, figma_inspection_paint_desc_t * paint)
{
    const figma_canvas_path_t * path =
        __figma_inspection_path_internal(opaque);
    if(path == NULL || paint == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    if((size_t)index >= path->paints.size)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }
    __figma_inspection_make_paint(
        FIGMA_ARRAY_CONST_PTR(
            figma_canvas_paint_t, &path->paints, index),
        paint);
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
const figma_inspection_interaction_t * FIGMA_CALL figma_inspection_get_interaction(const figma_inspection_node_t * opaque, uint32_t index)
{
    const figma_canvas_node_t * node = __figma_inspection_node_internal(opaque);
    return node != NULL && (size_t)index < node->prototype_interactions.size
        ? (const figma_inspection_interaction_t *)(const void *)
              FIGMA_ARRAY_CONST_PTR(
                  figma_prototype_interaction_t,
                  &node->prototype_interactions,
                  index)
        : NULL;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_inspection_get_interaction_desc(const figma_inspection_interaction_t * opaque, figma_inspection_interaction_desc_t * desc)
{
    const figma_prototype_interaction_t * interaction =
        __figma_inspection_interaction_internal(opaque);
    if(interaction == NULL || desc == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    memset(desc, 0, sizeof(*desc));
    desc->id = figma_string_view(&interaction->id);
    desc->raw_event_type = figma_string_view(&interaction->raw_event_type);
    desc->event_type = interaction->event_type;
    desc->transition_timeout = interaction->transition_timeout;
    desc->key_code = interaction->key_code;
    desc->action_count = __figma_inspection_count(interaction->actions.size);
    desc->unsupported_field_count =
        __figma_inspection_count(interaction->unsupported_fields.size);
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_inspection_get_interaction_action(const figma_inspection_interaction_t * opaque, uint32_t index, figma_inspection_action_desc_t * action)
{
    const figma_prototype_interaction_t * interaction =
        __figma_inspection_interaction_internal(opaque);
    const figma_prototype_action_t * source;
    if(interaction == NULL || action == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    if((size_t)index >= interaction->actions.size)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }

    source = FIGMA_ARRAY_CONST_PTR(
        figma_prototype_action_t, &interaction->actions, index);
    memset(action, 0, sizeof(*action));
    action->target_node_id = figma_string_view(&source->target_node_id);
    action->raw_connection_type =
        figma_string_view(&source->raw_connection_type);
    action->raw_navigation_type =
        figma_string_view(&source->raw_navigation_type);
    action->raw_transition_type =
        figma_string_view(&source->raw_transition_type);
    action->raw_transition_direction =
        figma_string_view(&source->raw_transition_direction);
    action->raw_transition_easing =
        figma_string_view(&source->raw_transition_easing);
    action->connection_type = source->connection_type;
    action->navigation_type = source->navigation_type;
    action->transition_type = source->transition_type;
    action->transition_direction = source->transition_direction;
    action->transition_easing = source->transition_easing;
    action->transition_duration = source->transition_duration;
    action->unsupported_field_count =
        __figma_inspection_count(source->unsupported_fields.size);
    action->smart_animate = source->smart_animate;
    action->transition_preserve_scroll = source->transition_preserve_scroll;
    action->transition_reset_video_position =
        source->transition_reset_video_position;
    action->has_easing_function = source->has_easing_function;
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_inspection_get_interaction_unsupported_field(const figma_inspection_interaction_t * opaque, uint32_t index, figma_string_view_t * field)
{
    const figma_prototype_interaction_t * interaction =
        __figma_inspection_interaction_internal(opaque);
    if(interaction == NULL || field == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    if((size_t)index >= interaction->unsupported_fields.size)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }
    *field = figma_string_view(FIGMA_ARRAY_CONST_PTR(
        figma_string_t, &interaction->unsupported_fields, index));
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_inspection_get_action_unsupported_field(const figma_inspection_interaction_t * opaque, uint32_t action_index, uint32_t field_index, figma_string_view_t * field)
{
    const figma_prototype_interaction_t * interaction =
        __figma_inspection_interaction_internal(opaque);
    const figma_prototype_action_t * action;
    if(interaction == NULL || field == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    if((size_t)action_index >= interaction->actions.size)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }
    action = FIGMA_ARRAY_CONST_PTR(
        figma_prototype_action_t, &interaction->actions, action_index);
    if((size_t)field_index >= action->unsupported_fields.size)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }
    *field = figma_string_view(FIGMA_ARRAY_CONST_PTR(
        figma_string_t, &action->unsupported_fields, field_index));
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_inspection_get_render_command(const figma_render_list_t * render_list, uint32_t index, figma_inspection_render_command_desc_t * desc)
{
    const figma_render_command_t * command;
    if(render_list == NULL || desc == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    if((size_t)index >= render_list->commands.size)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }
    command = FIGMA_ARRAY_CONST_PTR(
        figma_render_command_t, &render_list->commands, index);
    memset(desc, 0, sizeof(*desc));
    desc->type = command->type;
    desc->id = figma_string_view(&command->id);
    desc->node_id = figma_string_view(&command->node_id);
    desc->asset_id = figma_string_view(&command->asset_id);
    desc->text = figma_string_view(&command->text);
    desc->font_family = figma_string_view(&command->font_family);
    desc->font_style = figma_string_view(&command->font_style);
    desc->font_postscript_name =
        figma_string_view(&command->font_postscript_name);
    desc->rect = command->rect;
    desc->color = command->color;
    desc->shape = command->shape;
    desc->text_align_horizontal = command->text_align_horizontal;
    desc->text_align_vertical = command->text_align_vertical;
    desc->blend_mode = command->blend_mode;
    desc->image_scale_mode = command->image_scale_mode;
    desc->corner_radius = command->corner_radius;
    desc->font_size = command->font_size;
    desc->line_height = command->line_height;
    desc->font_weight = command->font_weight;
    desc->stroke_width = command->stroke_width;
    desc->opacity = command->opacity;
    desc->render_layer_id = command->render_layer_id;
    desc->render_layer_opacity = command->render_layer_opacity;
    desc->arc_starting_angle = command->arc_starting_angle;
    desc->arc_ending_angle = command->arc_ending_angle;
    desc->arc_inner_radius = command->arc_inner_radius;
    memcpy(
        desc->image_transform,
        command->image_transform,
        sizeof(desc->image_transform));
    memcpy(
        desc->filter_color_adjust,
        command->filter_color_adjust,
        sizeof(desc->filter_color_adjust));
    memcpy(
        desc->paint_filter,
        command->paint_filter,
        sizeof(desc->paint_filter));
    desc->original_image_width = command->original_image_width;
    desc->original_image_height = command->original_image_height;
    desc->text_line_count = __figma_inspection_count(command->text_lines.size);
    desc->vertex_count = __figma_inspection_count(command->vertices.size);
    desc->vertices = (const figma_render_vertex_t *)command->vertices.data;
    desc->index_count = __figma_inspection_count(command->indices.size);
    desc->indices = (const uint16_t *)command->indices.data;
    desc->has_arc_data = command->has_arc_data;
    desc->has_image_transform = command->has_image_transform;
    desc->has_filter_color_adjust = command->has_filter_color_adjust;
    desc->has_paint_filter = command->has_paint_filter;
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
float FIGMA_CALL figma_inspection_get_prototype_intro_advance_time(const figma_document_t * document)
{
    const figma_canvas_node_t * frame =
        figma_document_get_prototype_start_frame(document);
    float advance = 0.0f;
    size_t interaction_index;

    if(frame == NULL)
    {
        return 0.0f;
    }

    for(interaction_index = 0u;
        interaction_index != frame->prototype_interactions.size;
        ++interaction_index)
    {
        const figma_prototype_interaction_t * interaction =
            FIGMA_ARRAY_CONST_PTR(
                figma_prototype_interaction_t,
                &frame->prototype_interactions,
                interaction_index);
        size_t action_index;
        if(interaction->event_type != FIGMA_PROTOTYPE_EVENT_AFTER_TIMEOUT)
        {
            continue;
        }
        for(action_index = 0u;
            action_index != interaction->actions.size;
            ++action_index)
        {
            const figma_prototype_action_t * action =
                FIGMA_ARRAY_CONST_PTR(
                    figma_prototype_action_t,
                    &interaction->actions,
                    action_index);
            const figma_canvas_node_t * target;
            float candidate;
            if(action->connection_type !=
                    FIGMA_PROTOTYPE_CONNECTION_INTERNAL_NODE ||
                action->target_node_id.size == 0u ||
                (action->navigation_type !=
                        FIGMA_PROTOTYPE_NAVIGATION_NAVIGATE &&
                    action->navigation_type !=
                        FIGMA_PROTOTYPE_NAVIGATION_OVERLAY))
            {
                continue;
            }
            target = figma_document_find_canvas_node(
                document, figma_string_view(&action->target_node_id));
            if(target == NULL || target->type != FIGMA_CANVAS_NODE_FRAME)
            {
                continue;
            }
            candidate = interaction->transition_timeout +
                action->transition_duration + 0.001f;
            if(candidate > advance)
            {
                advance = candidate;
            }
        }
    }
    return advance;
}

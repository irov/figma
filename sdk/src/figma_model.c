#include "figma_model.h"

#include <limits.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////////
static void __figma_string_element_destroy(figma_memory_t * memory, void * value)
{
    figma_string_destroy(memory, (figma_string_t *)value);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_path_style_override_init(figma_canvas_path_style_override_t * item)
{
    memset(item, 0, sizeof(*item));
    figma_array_init(&item->fills, sizeof(figma_canvas_paint_t));
    figma_array_init(&item->strokes, sizeof(figma_canvas_paint_t));
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_path_style_override_destroy(figma_memory_t * memory, void * value)
{
    figma_canvas_path_style_override_t * item =
        (figma_canvas_path_style_override_t *)value;

    figma_array_destroy(memory, &item->fills, &figma_canvas_paint_destroy);
    figma_array_destroy(memory, &item->strokes, &figma_canvas_paint_destroy);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_paint_array_copy(figma_memory_t * memory, figma_array_t * target, const figma_array_t * source)
{
    size_t index;

    for(index = 0u; index != source->size; ++index)
    {
        const figma_canvas_paint_t * source_item =
            FIGMA_ARRAY_CONST_PTR(figma_canvas_paint_t, source, index);
        figma_canvas_paint_t * target_item =
            (figma_canvas_paint_t *)figma_array_push_uninitialized(memory, target);

        if(target_item == NULL)
        {
            return FIGMA_FALSE;
        }

        figma_canvas_paint_init(target_item);

        if(figma_canvas_paint_copy(memory, target_item, source_item) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_string_array_copy(figma_memory_t * memory, figma_array_t * target, const figma_array_t * source)
{
    size_t index;

    for(index = 0u; index != source->size; ++index)
    {
        const figma_string_t * source_item =
            FIGMA_ARRAY_CONST_PTR(figma_string_t, source, index);
        figma_string_t * target_item =
            (figma_string_t *)figma_array_push_uninitialized(memory, target);

        if(target_item == NULL)
        {
            return FIGMA_FALSE;
        }

        figma_string_init(target_item);

        if(figma_string_copy(memory, target_item, source_item) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_text_line_init(figma_canvas_text_line_t * line)
{
    memset(line, 0, sizeof(*line));
    figma_string_init(&line->text);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_canvas_text_line_destroy(figma_memory_t * memory, void * value)
{
    figma_canvas_text_line_t * line = (figma_canvas_text_line_t *)value;
    figma_string_destroy(memory, &line->text);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_binding_destroy(figma_memory_t * memory, void * value)
{
    figma_binding_t * binding = (figma_binding_t *)value;
    figma_string_destroy(memory, &binding->node_id);
    figma_string_destroy(memory, &binding->key);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_action_destroy(figma_memory_t * memory, void * value)
{
    figma_action_t * action = (figma_action_t *)value;
    figma_string_destroy(memory, &action->node_id);
    figma_string_destroy(memory, &action->action_id);
    figma_string_destroy(memory, &action->target_frame_id);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_asset_destroy(figma_memory_t * memory, void * value)
{
    figma_asset_t * asset = (figma_asset_t *)value;
    figma_string_destroy(memory, &asset->id);
    figma_string_destroy(memory, &asset->path);
    figma_string_destroy(memory, &asset->mime);
    figma_array_destroy(memory, &asset->bytes, NULL);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_render_text_line_destroy(figma_memory_t * memory, void * value)
{
    figma_render_text_line_t * line = (figma_render_text_line_t *)value;
    figma_string_destroy(memory, &line->text);
}

//////////////////////////////////////////////////////////////////////////
void figma_canvas_paint_init(figma_canvas_paint_t * paint)
{
    size_t index;

    memset(paint, 0, sizeof(*paint));
    paint->type = FIGMA_CANVAS_PAINT_SOLID;
    paint->blend_mode = FIGMA_CANVAS_BLEND_NORMAL;
    paint->image_scale_mode = FIGMA_CANVAS_IMAGE_SCALE_FILL;
    paint->color.r = 1.0f;
    paint->color.g = 1.0f;
    paint->color.b = 1.0f;
    paint->color.a = 1.0f;
    paint->opacity = 1.0f;
    paint->visible = FIGMA_TRUE;
    paint->transform[0] = 1.0f;
    paint->transform[4] = 1.0f;
    figma_string_init(&paint->asset_id);
    figma_string_init(&paint->raw_type);
    figma_string_init(&paint->raw_blend_mode);

    for(index = 0u; index != 8u; ++index)
    {
        paint->filter_color_adjust[index] = 0.0f;
    }

    for(index = 0u; index != 10u; ++index)
    {
        paint->paint_filter[index] = 0.0f;
    }
}

//////////////////////////////////////////////////////////////////////////
void figma_canvas_paint_destroy(figma_memory_t * memory, void * value)
{
    figma_canvas_paint_t * paint = (figma_canvas_paint_t *)value;

    figma_string_destroy(memory, &paint->asset_id);
    figma_string_destroy(memory, &paint->raw_type);
    figma_string_destroy(memory, &paint->raw_blend_mode);
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_canvas_paint_copy(figma_memory_t * memory, figma_canvas_paint_t * target, const figma_canvas_paint_t * source)
{
    figma_string_t asset_id;
    figma_string_t raw_type;
    figma_string_t raw_blend_mode;

    if(memory == NULL || target == NULL || source == NULL)
    {
        return FIGMA_FALSE;
    }

    figma_string_init(&asset_id);
    figma_string_init(&raw_type);
    figma_string_init(&raw_blend_mode);

    if(figma_string_copy(memory, &asset_id, &source->asset_id) == FIGMA_FALSE ||
        figma_string_copy(memory, &raw_type, &source->raw_type) == FIGMA_FALSE ||
        figma_string_copy(memory, &raw_blend_mode, &source->raw_blend_mode) == FIGMA_FALSE)
    {
        figma_string_destroy(memory, &asset_id);
        figma_string_destroy(memory, &raw_type);
        figma_string_destroy(memory, &raw_blend_mode);
        return FIGMA_FALSE;
    }

    figma_canvas_paint_destroy(memory, target);
    *target = *source;
    target->asset_id = asset_id;
    target->raw_type = raw_type;
    target->raw_blend_mode = raw_blend_mode;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
void figma_canvas_path_init(figma_canvas_path_t * path)
{
    memset(path, 0, sizeof(*path));
    path->winding_rule = FIGMA_CANVAS_WINDING_NON_ZERO;
    figma_array_init(&path->commands, sizeof(figma_canvas_path_command_t));
    figma_array_init(&path->paints, sizeof(figma_canvas_paint_t));
}

//////////////////////////////////////////////////////////////////////////
void figma_canvas_path_destroy(figma_memory_t * memory, void * value)
{
    figma_canvas_path_t * path = (figma_canvas_path_t *)value;

    figma_array_destroy(memory, &path->commands, NULL);
    figma_array_destroy(memory, &path->paints, &figma_canvas_paint_destroy);
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_canvas_path_copy(figma_memory_t * memory, figma_canvas_path_t * target, const figma_canvas_path_t * source)
{
    figma_canvas_path_t copy;

    if(memory == NULL || target == NULL || source == NULL)
    {
        return FIGMA_FALSE;
    }

    figma_canvas_path_init(&copy);
    copy.winding_rule = source->winding_rule;
    copy.commands_blob = source->commands_blob;
    copy.style_id = source->style_id;
    copy.commands_decoded = source->commands_decoded;

    if(figma_array_reserve(memory, &copy.commands, source->commands.size) == FIGMA_FALSE ||
        figma_array_resize(memory, &copy.commands, source->commands.size, NULL) == FIGMA_FALSE ||
        __figma_canvas_paint_array_copy(memory, &copy.paints, &source->paints) == FIGMA_FALSE)
    {
        figma_canvas_path_destroy(memory, &copy);
        return FIGMA_FALSE;
    }

    if(source->commands.size != 0u)
    {
        memcpy(
            copy.commands.data,
            source->commands.data,
            source->commands.size * sizeof(figma_canvas_path_command_t));
    }

    figma_canvas_path_destroy(memory, target);
    *target = copy;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
void figma_prototype_action_init(figma_prototype_action_t * action)
{
    memset(action, 0, sizeof(*action));
    figma_string_init(&action->target_node_id);
    figma_string_init(&action->raw_connection_type);
    figma_string_init(&action->raw_navigation_type);
    figma_string_init(&action->raw_transition_type);
    figma_string_init(&action->raw_transition_direction);
    figma_string_init(&action->raw_transition_easing);
    action->connection_type = FIGMA_PROTOTYPE_CONNECTION_NONE;
    action->navigation_type = FIGMA_PROTOTYPE_NAVIGATION_NAVIGATE;
    action->transition_type = FIGMA_PROTOTYPE_TRANSITION_INSTANT;
    action->transition_direction = FIGMA_PROTOTYPE_TRANSITION_DIRECTION_NONE;
    action->transition_easing = FIGMA_ANIMATION_EASING_EASE_IN_OUT;
    figma_array_init(&action->unsupported_fields, sizeof(figma_string_t));
}

//////////////////////////////////////////////////////////////////////////
void figma_prototype_action_destroy(figma_memory_t * memory, void * value)
{
    figma_prototype_action_t * action = (figma_prototype_action_t *)value;

    figma_string_destroy(memory, &action->target_node_id);
    figma_string_destroy(memory, &action->raw_connection_type);
    figma_string_destroy(memory, &action->raw_navigation_type);
    figma_string_destroy(memory, &action->raw_transition_type);
    figma_string_destroy(memory, &action->raw_transition_direction);
    figma_string_destroy(memory, &action->raw_transition_easing);
    figma_array_destroy(memory, &action->unsupported_fields, &__figma_string_element_destroy);
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_prototype_action_copy(figma_memory_t * memory, figma_prototype_action_t * target, const figma_prototype_action_t * source)
{
    figma_prototype_action_t copy;

    if(memory == NULL || target == NULL || source == NULL)
    {
        return FIGMA_FALSE;
    }

    figma_prototype_action_init(&copy);
    copy.connection_type = source->connection_type;
    copy.navigation_type = source->navigation_type;
    copy.transition_type = source->transition_type;
    copy.transition_direction = source->transition_direction;
    copy.transition_easing = source->transition_easing;
    copy.transition_duration = source->transition_duration;
    copy.smart_animate = source->smart_animate;
    copy.transition_preserve_scroll = source->transition_preserve_scroll;
    copy.transition_reset_video_position = source->transition_reset_video_position;
    copy.has_easing_function = source->has_easing_function;

    if(figma_string_copy(memory, &copy.target_node_id, &source->target_node_id) == FIGMA_FALSE ||
        figma_string_copy(
            memory, &copy.raw_connection_type, &source->raw_connection_type) == FIGMA_FALSE ||
        figma_string_copy(
            memory, &copy.raw_navigation_type, &source->raw_navigation_type) == FIGMA_FALSE ||
        figma_string_copy(
            memory, &copy.raw_transition_type, &source->raw_transition_type) == FIGMA_FALSE ||
        figma_string_copy(
            memory, &copy.raw_transition_direction, &source->raw_transition_direction) == FIGMA_FALSE ||
        figma_string_copy(
            memory, &copy.raw_transition_easing, &source->raw_transition_easing) == FIGMA_FALSE ||
        __figma_string_array_copy(
            memory, &copy.unsupported_fields, &source->unsupported_fields) == FIGMA_FALSE)
    {
        figma_prototype_action_destroy(memory, &copy);
        return FIGMA_FALSE;
    }

    figma_prototype_action_destroy(memory, target);
    *target = copy;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
void figma_prototype_interaction_init(figma_prototype_interaction_t * interaction)
{
    memset(interaction, 0, sizeof(*interaction));
    figma_string_init(&interaction->id);
    figma_string_init(&interaction->raw_event_type);
    interaction->event_type = FIGMA_PROTOTYPE_EVENT_UNSUPPORTED;
    figma_array_init(&interaction->actions, sizeof(figma_prototype_action_t));
    figma_array_init(&interaction->unsupported_fields, sizeof(figma_string_t));
}

//////////////////////////////////////////////////////////////////////////
void figma_prototype_interaction_destroy(figma_memory_t * memory, void * value)
{
    figma_prototype_interaction_t * interaction =
        (figma_prototype_interaction_t *)value;

    figma_string_destroy(memory, &interaction->id);
    figma_string_destroy(memory, &interaction->raw_event_type);
    figma_array_destroy(
        memory, &interaction->actions, &figma_prototype_action_destroy);
    figma_array_destroy(
        memory, &interaction->unsupported_fields, &__figma_string_element_destroy);
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_prototype_interaction_copy(figma_memory_t * memory, figma_prototype_interaction_t * target, const figma_prototype_interaction_t * source)
{
    figma_prototype_interaction_t copy;
    size_t index;

    if(memory == NULL || target == NULL || source == NULL)
    {
        return FIGMA_FALSE;
    }

    figma_prototype_interaction_init(&copy);
    copy.event_type = source->event_type;
    copy.transition_timeout = source->transition_timeout;
    copy.key_code = source->key_code;

    if(figma_string_copy(memory, &copy.id, &source->id) == FIGMA_FALSE ||
        figma_string_copy(memory, &copy.raw_event_type, &source->raw_event_type) == FIGMA_FALSE ||
        __figma_string_array_copy(
            memory, &copy.unsupported_fields, &source->unsupported_fields) == FIGMA_FALSE)
    {
        figma_prototype_interaction_destroy(memory, &copy);
        return FIGMA_FALSE;
    }

    for(index = 0u; index != source->actions.size; ++index)
    {
        const figma_prototype_action_t * source_action =
            FIGMA_ARRAY_CONST_PTR(figma_prototype_action_t, &source->actions, index);
        figma_prototype_action_t * target_action =
            (figma_prototype_action_t *)figma_array_push_uninitialized(
                memory, &copy.actions);

        if(target_action == NULL)
        {
            figma_prototype_interaction_destroy(memory, &copy);
            return FIGMA_FALSE;
        }

        figma_prototype_action_init(target_action);

        if(figma_prototype_action_copy(
               memory, target_action, source_action) == FIGMA_FALSE)
        {
            figma_prototype_interaction_destroy(memory, &copy);
            return FIGMA_FALSE;
        }
    }

    figma_prototype_interaction_destroy(memory, target);
    *target = copy;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
void figma_canvas_node_init(figma_canvas_node_t * node)
{
    memset(node, 0, sizeof(*node));
    figma_string_init(&node->id);
    figma_string_init(&node->name);
    node->type = FIGMA_CANVAS_NODE_UNKNOWN;
    node->opacity = 1.0f;
    node->stroke_weight = 1.0f;
    node->font_size = 18.0f;
    node->font_weight = 400;
    node->visible = FIGMA_TRUE;
    node->mask_type = FIGMA_CANVAS_MASK_ALPHA;
    node->blend_mode = FIGMA_CANVAS_BLEND_NORMAL;
    node->stroke_align = FIGMA_CANVAS_STROKE_ALIGN_CENTER;
    node->stroke_cap = FIGMA_CANVAS_STROKE_CAP_NONE;
    node->stroke_join = FIGMA_CANVAS_STROKE_JOIN_MITER;
    node->text_align_horizontal = FIGMA_CANVAS_TEXT_ALIGN_HORIZONTAL_LEFT;
    node->text_align_vertical = FIGMA_CANVAS_TEXT_ALIGN_VERTICAL_TOP;
    figma_string_init(&node->text);
    figma_string_init(&node->font_family);
    figma_string_init(&node->font_style);
    figma_string_init(&node->font_postscript_name);
    figma_string_init(&node->prototype_start_node_id);
    figma_string_init(&node->symbol_id);
    figma_string_init(&node->fill_style_node_id);
    figma_string_init(&node->stroke_fill_style_node_id);
    figma_string_init(&node->raw_blend_mode);
    figma_array_init(&node->dash_pattern, sizeof(float));
    figma_array_init(
        &node->path_style_overrides, sizeof(figma_canvas_path_style_override_t));
    figma_array_init(&node->fill_geometry, sizeof(figma_canvas_path_t));
    figma_array_init(&node->stroke_geometry, sizeof(figma_canvas_path_t));
    figma_array_init(
        &node->prototype_interactions, sizeof(figma_prototype_interaction_t));
    figma_array_init(&node->text_lines, sizeof(figma_canvas_text_line_t));
    figma_array_init(&node->fills, sizeof(figma_canvas_paint_t));
    figma_array_init(&node->strokes, sizeof(figma_canvas_paint_t));
    figma_array_init(&node->children, sizeof(figma_canvas_node_t));
}

//////////////////////////////////////////////////////////////////////////
void figma_canvas_node_destroy(figma_memory_t * memory, void * value)
{
    figma_canvas_node_t * node = (figma_canvas_node_t *)value;

    figma_string_destroy(memory, &node->id);
    figma_string_destroy(memory, &node->name);
    figma_string_destroy(memory, &node->text);
    figma_string_destroy(memory, &node->font_family);
    figma_string_destroy(memory, &node->font_style);
    figma_string_destroy(memory, &node->font_postscript_name);
    figma_string_destroy(memory, &node->prototype_start_node_id);
    figma_string_destroy(memory, &node->symbol_id);
    figma_string_destroy(memory, &node->fill_style_node_id);
    figma_string_destroy(memory, &node->stroke_fill_style_node_id);
    figma_string_destroy(memory, &node->raw_blend_mode);
    figma_array_destroy(memory, &node->dash_pattern, NULL);
    figma_array_destroy(
        memory, &node->path_style_overrides, &__figma_canvas_path_style_override_destroy);
    figma_array_destroy(memory, &node->fill_geometry, &figma_canvas_path_destroy);
    figma_array_destroy(memory, &node->stroke_geometry, &figma_canvas_path_destroy);
    figma_array_destroy(
        memory, &node->prototype_interactions, &figma_prototype_interaction_destroy);
    figma_array_destroy(memory, &node->text_lines, &__figma_canvas_text_line_destroy);
    figma_array_destroy(memory, &node->fills, &figma_canvas_paint_destroy);
    figma_array_destroy(memory, &node->strokes, &figma_canvas_paint_destroy);
    figma_array_destroy(memory, &node->children, &figma_canvas_node_destroy);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_canvas_path_array_copy(figma_memory_t * memory, figma_array_t * target, const figma_array_t * source)
{
    size_t index;

    for(index = 0u; index != source->size; ++index)
    {
        const figma_canvas_path_t * source_item =
            FIGMA_ARRAY_CONST_PTR(figma_canvas_path_t, source, index);
        figma_canvas_path_t * target_item =
            (figma_canvas_path_t *)figma_array_push_uninitialized(memory, target);

        if(target_item == NULL)
        {
            return FIGMA_FALSE;
        }

        figma_canvas_path_init(target_item);

        if(figma_canvas_path_copy(memory, target_item, source_item) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_canvas_node_copy(figma_memory_t * memory, figma_canvas_node_t * target, const figma_canvas_node_t * source)
{
    figma_canvas_node_t copy;
    size_t index;

    if(memory == NULL || target == NULL || source == NULL)
    {
        return FIGMA_FALSE;
    }

    figma_canvas_node_init(&copy);
    copy.type = source->type;
    copy.size = source->size;
    copy.rect = source->rect;
    memcpy(copy.quad, source->quad, sizeof(copy.quad));
    copy.opacity = source->opacity;
    copy.corner_radius = source->corner_radius;
    copy.stroke_weight = source->stroke_weight;
    copy.font_size = source->font_size;
    copy.line_height = source->line_height;
    copy.font_weight = source->font_weight;
    copy.visible = source->visible;
    copy.mask = source->mask;
    copy.frame_mask_disabled = source->frame_mask_disabled;
    copy.has_fill_geometry = source->has_fill_geometry;
    copy.has_stroke_geometry = source->has_stroke_geometry;
    copy.has_vector_data = source->has_vector_data;
    copy.has_vector_network_blob = source->has_vector_network_blob;
    copy.has_prototype_starting_point = source->has_prototype_starting_point;
    copy.vector_network_blob = source->vector_network_blob;
    copy.prototype_interaction_count = source->prototype_interaction_count;
    copy.vector_normalized_size = source->vector_normalized_size;
    copy.mask_type = source->mask_type;
    copy.blend_mode = source->blend_mode;
    copy.stroke_align = source->stroke_align;
    copy.stroke_cap = source->stroke_cap;
    copy.stroke_join = source->stroke_join;
    copy.arc_data = source->arc_data;
    copy.text_align_horizontal = source->text_align_horizontal;
    copy.text_align_vertical = source->text_align_vertical;

#define FIGMA_COPY_NODE_STRING(field) \
    if(figma_string_copy(memory, &copy.field, &source->field) == FIGMA_FALSE) \
    { \
        figma_canvas_node_destroy(memory, &copy); \
        return FIGMA_FALSE; \
    }

    FIGMA_COPY_NODE_STRING(id)
    FIGMA_COPY_NODE_STRING(name)
    FIGMA_COPY_NODE_STRING(text)
    FIGMA_COPY_NODE_STRING(font_family)
    FIGMA_COPY_NODE_STRING(font_style)
    FIGMA_COPY_NODE_STRING(font_postscript_name)
    FIGMA_COPY_NODE_STRING(prototype_start_node_id)
    FIGMA_COPY_NODE_STRING(symbol_id)
    FIGMA_COPY_NODE_STRING(fill_style_node_id)
    FIGMA_COPY_NODE_STRING(stroke_fill_style_node_id)
    FIGMA_COPY_NODE_STRING(raw_blend_mode)

#undef FIGMA_COPY_NODE_STRING

    if(figma_array_resize(
           memory, &copy.dash_pattern, source->dash_pattern.size, NULL) == FIGMA_FALSE)
    {
        figma_canvas_node_destroy(memory, &copy);
        return FIGMA_FALSE;
    }

    if(source->dash_pattern.size != 0u)
    {
        memcpy(
            copy.dash_pattern.data,
            source->dash_pattern.data,
            source->dash_pattern.size * sizeof(float));
    }

    for(index = 0u; index != source->path_style_overrides.size; ++index)
    {
        const figma_canvas_path_style_override_t * source_item =
            FIGMA_ARRAY_CONST_PTR(
                figma_canvas_path_style_override_t, &source->path_style_overrides, index);
        figma_canvas_path_style_override_t * target_item =
            (figma_canvas_path_style_override_t *)figma_array_push_uninitialized(
                memory, &copy.path_style_overrides);

        if(target_item == NULL)
        {
            figma_canvas_node_destroy(memory, &copy);
            return FIGMA_FALSE;
        }

        __figma_canvas_path_style_override_init(target_item);
        target_item->style_id = source_item->style_id;

        if(__figma_canvas_paint_array_copy(
               memory, &target_item->fills, &source_item->fills) == FIGMA_FALSE ||
            __figma_canvas_paint_array_copy(
                memory, &target_item->strokes, &source_item->strokes) == FIGMA_FALSE)
        {
            figma_canvas_node_destroy(memory, &copy);
            return FIGMA_FALSE;
        }
    }

    if(__figma_canvas_path_array_copy(
           memory, &copy.fill_geometry, &source->fill_geometry) == FIGMA_FALSE ||
        __figma_canvas_path_array_copy(
            memory, &copy.stroke_geometry, &source->stroke_geometry) == FIGMA_FALSE ||
        __figma_canvas_paint_array_copy(memory, &copy.fills, &source->fills) == FIGMA_FALSE ||
        __figma_canvas_paint_array_copy(
            memory, &copy.strokes, &source->strokes) == FIGMA_FALSE)
    {
        figma_canvas_node_destroy(memory, &copy);
        return FIGMA_FALSE;
    }

    for(index = 0u; index != source->prototype_interactions.size; ++index)
    {
        const figma_prototype_interaction_t * source_item =
            FIGMA_ARRAY_CONST_PTR(
                figma_prototype_interaction_t, &source->prototype_interactions, index);
        figma_prototype_interaction_t * target_item =
            (figma_prototype_interaction_t *)figma_array_push_uninitialized(
                memory, &copy.prototype_interactions);

        if(target_item == NULL)
        {
            figma_canvas_node_destroy(memory, &copy);
            return FIGMA_FALSE;
        }

        figma_prototype_interaction_init(target_item);

        if(figma_prototype_interaction_copy(
               memory, target_item, source_item) == FIGMA_FALSE)
        {
            figma_canvas_node_destroy(memory, &copy);
            return FIGMA_FALSE;
        }
    }

    for(index = 0u; index != source->text_lines.size; ++index)
    {
        const figma_canvas_text_line_t * source_item =
            FIGMA_ARRAY_CONST_PTR(figma_canvas_text_line_t, &source->text_lines, index);
        figma_canvas_text_line_t * target_item =
            (figma_canvas_text_line_t *)figma_array_push_uninitialized(
                memory, &copy.text_lines);

        if(target_item == NULL)
        {
            figma_canvas_node_destroy(memory, &copy);
            return FIGMA_FALSE;
        }

        __figma_canvas_text_line_init(target_item);
        *target_item = *source_item;
        figma_string_init(&target_item->text);

        if(figma_string_copy(
               memory, &target_item->text, &source_item->text) == FIGMA_FALSE)
        {
            figma_canvas_node_destroy(memory, &copy);
            return FIGMA_FALSE;
        }
    }

    for(index = 0u; index != source->children.size; ++index)
    {
        const figma_canvas_node_t * source_item =
            FIGMA_ARRAY_CONST_PTR(figma_canvas_node_t, &source->children, index);
        figma_canvas_node_t * target_item =
            (figma_canvas_node_t *)figma_array_push_uninitialized(memory, &copy.children);

        if(target_item == NULL)
        {
            figma_canvas_node_destroy(memory, &copy);
            return FIGMA_FALSE;
        }

        figma_canvas_node_init(target_item);

        if(figma_canvas_node_copy(memory, target_item, source_item) == FIGMA_FALSE)
        {
            figma_canvas_node_destroy(memory, &copy);
            return FIGMA_FALSE;
        }
    }

    figma_canvas_node_destroy(memory, target);
    *target = copy;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
const figma_canvas_node_t * figma_canvas_node_find(const figma_canvas_node_t * node, figma_string_view_t id)
{
    size_t index;

    if(node == NULL || id.size == 0u)
    {
        return NULL;
    }

    if(figma_string_equal_view(&node->id, id) == FIGMA_TRUE)
    {
        return node;
    }

    for(index = 0u; index != node->children.size; ++index)
    {
        const figma_canvas_node_t * child =
            FIGMA_ARRAY_CONST_PTR(figma_canvas_node_t, &node->children, index);
        const figma_canvas_node_t * found = figma_canvas_node_find(child, id);

        if(found != NULL)
        {
            return found;
        }
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////
void figma_animation_track_init(figma_animation_track_t * track)
{
    memset(track, 0, sizeof(*track));
    figma_string_init(&track->node_id);
    figma_string_init(&track->target_node_id);
    track->type = FIGMA_ANIMATION_TRACK_OPACITY;
}

//////////////////////////////////////////////////////////////////////////
void figma_animation_track_destroy(figma_memory_t * memory, void * value)
{
    figma_animation_track_t * track = (figma_animation_track_t *)value;
    figma_string_destroy(memory, &track->node_id);
    figma_string_destroy(memory, &track->target_node_id);
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_animation_track_copy(figma_memory_t * memory, figma_animation_track_t * target, const figma_animation_track_t * source)
{
    figma_animation_track_t copy;

    figma_animation_track_init(&copy);
    copy = *source;
    figma_string_init(&copy.node_id);
    figma_string_init(&copy.target_node_id);

    if(figma_string_copy(memory, &copy.node_id, &source->node_id) == FIGMA_FALSE ||
        figma_string_copy(
            memory, &copy.target_node_id, &source->target_node_id) == FIGMA_FALSE)
    {
        figma_animation_track_destroy(memory, &copy);
        return FIGMA_FALSE;
    }

    figma_animation_track_destroy(memory, target);
    *target = copy;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
void figma_animation_clip_init(figma_animation_clip_t * clip)
{
    memset(clip, 0, sizeof(*clip));
    figma_string_init(&clip->id);
    figma_string_init(&clip->source_frame_id);
    figma_string_init(&clip->target_frame_id);
    figma_string_init(&clip->source_node_id);
    clip->source = FIGMA_ANIMATION_SOURCE_NONE;
    clip->transition_type = FIGMA_PROTOTYPE_TRANSITION_INSTANT;
    clip->transition_direction = FIGMA_PROTOTYPE_TRANSITION_DIRECTION_NONE;
    clip->easing = FIGMA_ANIMATION_EASING_EASE_IN_OUT;
    figma_array_init(&clip->tracks, sizeof(figma_animation_track_t));
}

//////////////////////////////////////////////////////////////////////////
void figma_animation_clip_destroy(figma_memory_t * memory, figma_animation_clip_t * clip)
{
    figma_string_destroy(memory, &clip->id);
    figma_string_destroy(memory, &clip->source_frame_id);
    figma_string_destroy(memory, &clip->target_frame_id);
    figma_string_destroy(memory, &clip->source_node_id);
    figma_array_destroy(memory, &clip->tracks, &figma_animation_track_destroy);
}

//////////////////////////////////////////////////////////////////////////
void figma_player_animation_state_init(figma_player_animation_state_t * state)
{
    memset(state, 0, sizeof(*state));
    figma_animation_clip_init(&state->clip);
}

//////////////////////////////////////////////////////////////////////////
void figma_player_animation_state_destroy(figma_memory_t * memory, figma_player_animation_state_t * state)
{
    figma_animation_clip_destroy(memory, &state->clip);
}

//////////////////////////////////////////////////////////////////////////
void figma_render_command_init(figma_render_command_t * command, figma_render_command_type_t type)
{
    memset(command, 0, sizeof(*command));
    command->type = type;
    figma_string_init(&command->id);
    figma_string_init(&command->node_id);
    figma_string_init(&command->asset_id);
    figma_string_init(&command->text);
    figma_string_init(&command->font_family);
    figma_string_init(&command->font_style);
    figma_string_init(&command->font_postscript_name);
    command->color.r = 1.0f;
    command->color.g = 1.0f;
    command->color.b = 1.0f;
    command->color.a = 1.0f;
    command->shape = FIGMA_RENDER_SHAPE_RECTANGLE;
    command->text_align_horizontal = FIGMA_RENDER_TEXT_ALIGN_HORIZONTAL_LEFT;
    command->text_align_vertical = FIGMA_RENDER_TEXT_ALIGN_VERTICAL_TOP;
    command->blend_mode = FIGMA_RENDER_BLEND_NORMAL;
    command->image_scale_mode = FIGMA_RENDER_IMAGE_SCALE_FILL;
    command->font_size = 18.0f;
    command->font_weight = 400;
    command->stroke_width = 1.0f;
    command->opacity = 1.0f;
    command->render_layer_opacity = 1.0f;
    command->image_transform[0] = 1.0f;
    command->image_transform[4] = 1.0f;
    figma_array_init(&command->text_lines, sizeof(figma_render_text_line_t));
    figma_array_init(&command->vertices, sizeof(figma_render_vertex_t));
    figma_array_init(&command->indices, sizeof(uint16_t));
}

//////////////////////////////////////////////////////////////////////////
void figma_render_command_destroy(figma_memory_t * memory, void * value)
{
    figma_render_command_t * command = (figma_render_command_t *)value;

    figma_string_destroy(memory, &command->id);
    figma_string_destroy(memory, &command->node_id);
    figma_string_destroy(memory, &command->asset_id);
    figma_string_destroy(memory, &command->text);
    figma_string_destroy(memory, &command->font_family);
    figma_string_destroy(memory, &command->font_style);
    figma_string_destroy(memory, &command->font_postscript_name);
    figma_array_destroy(memory, &command->text_lines, &__figma_render_text_line_destroy);
    figma_array_destroy(memory, &command->vertices, NULL);
    figma_array_destroy(memory, &command->indices, NULL);
}

//////////////////////////////////////////////////////////////////////////
void figma_render_list_init(figma_render_list_t * render_list, figma_memory_t * memory)
{
    render_list->memory = memory;
    figma_array_init(&render_list->commands, sizeof(figma_render_command_t));
}

//////////////////////////////////////////////////////////////////////////
void figma_render_list_destroy(figma_render_list_t * render_list)
{
    figma_array_destroy(
        render_list->memory, &render_list->commands, &figma_render_command_destroy);
    render_list->memory = NULL;
}

//////////////////////////////////////////////////////////////////////////
void figma_render_list_clear(figma_render_list_t * render_list)
{
    figma_array_clear(
        render_list->memory, &render_list->commands, &figma_render_command_destroy);
}

//////////////////////////////////////////////////////////////////////////
figma_render_command_t * figma_render_list_add(figma_render_list_t * render_list, figma_render_command_type_t type)
{
    figma_render_command_t * command =
        (figma_render_command_t *)figma_array_push_uninitialized(
            render_list->memory, &render_list->commands);

    if(command != NULL)
    {
        figma_render_command_init(command, type);
    }

    return command;
}

//////////////////////////////////////////////////////////////////////////
void figma_render_list_remove_last(figma_render_list_t * render_list)
{
    figma_array_pop(
        render_list->memory, &render_list->commands, &figma_render_command_destroy);
}

//////////////////////////////////////////////////////////////////////////
void figma_document_init(figma_document_t * document, figma_runtime_t * runtime, figma_memory_t * memory)
{
    memset(document, 0, sizeof(*document));
    document->runtime = runtime;
    document->memory = memory;
    figma_string_init(&document->path);
    figma_string_init(&document->file_name);
    figma_canvas_node_init(&document->canvas_root);
    figma_string_init(&document->prototype_start_node_id);
    figma_array_init(&document->assets, sizeof(figma_asset_t));
    figma_array_init(&document->canvas_bytes, sizeof(uint8_t));
    figma_array_init(&document->bindings, sizeof(figma_binding_t));
    figma_array_init(&document->actions, sizeof(figma_action_t));
    figma_diagnostics_init(&document->diagnostics, memory);
}

//////////////////////////////////////////////////////////////////////////
void figma_document_deinit(figma_document_t * document)
{
    figma_memory_t * memory;

    if(document == NULL)
    {
        return;
    }

    memory = document->memory;
    figma_string_destroy(memory, &document->path);
    figma_string_destroy(memory, &document->file_name);
    figma_canvas_node_destroy(memory, &document->canvas_root);
    figma_string_destroy(memory, &document->prototype_start_node_id);
    figma_array_destroy(memory, &document->assets, &__figma_asset_destroy);
    figma_array_destroy(memory, &document->canvas_bytes, NULL);
    figma_array_destroy(memory, &document->bindings, &__figma_binding_destroy);
    figma_array_destroy(memory, &document->actions, &__figma_action_destroy);
    figma_diagnostics_destroy(&document->diagnostics);
    memset(document, 0, sizeof(*document));
}

//////////////////////////////////////////////////////////////////////////
const figma_canvas_node_t * figma_document_find_canvas_node(const figma_document_t * document, figma_string_view_t id)
{
    if(document == NULL || document->has_canvas_root == FIGMA_FALSE)
    {
        return NULL;
    }

    return figma_canvas_node_find(&document->canvas_root, id);
}

//////////////////////////////////////////////////////////////////////////
const figma_canvas_node_t * figma_document_get_prototype_start_frame(const figma_document_t * document)
{
    if(document == NULL || document->prototype_start_node_id.size == 0u)
    {
        return NULL;
    }

    return figma_document_find_canvas_node(
        document, figma_string_view(&document->prototype_start_node_id));
}

//////////////////////////////////////////////////////////////////////////
const figma_asset_t * figma_document_find_asset_internal(const figma_document_t * document, figma_string_view_t asset_id)
{
    size_t index;

    if(document == NULL)
    {
        return NULL;
    }

    for(index = 0u; index != document->assets.size; ++index)
    {
        const figma_asset_t * asset =
            FIGMA_ARRAY_CONST_PTR(figma_asset_t, &document->assets, index);

        if(figma_string_equal_view(&asset->id, asset_id) == FIGMA_TRUE ||
            figma_string_equal_view(&asset->path, asset_id) == FIGMA_TRUE)
        {
            return asset;
        }
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////
static figma_render_batch_type_t __figma_render_batch_type(const figma_render_command_t * command)
{
    if(command->type == FIGMA_RENDER_COMMAND_CLIP_BEGIN)
    {
        return FIGMA_RENDER_BATCH_CLIP_BEGIN;
    }

    if(command->type == FIGMA_RENDER_COMMAND_CLIP_END)
    {
        return FIGMA_RENDER_BATCH_CLIP_END;
    }

    return FIGMA_RENDER_BATCH_GEOMETRY;
}

//////////////////////////////////////////////////////////////////////////
static figma_render_shader_type_t __figma_render_shader_type(const figma_render_command_t * command)
{
    if(command->type == FIGMA_RENDER_COMMAND_IMAGE ||
        command->type == FIGMA_RENDER_COMMAND_TEXT)
    {
        return FIGMA_RENDER_SHADER_TEXTURE;
    }

    if(command->type == FIGMA_RENDER_COMMAND_DEBUG_HOTSPOT)
    {
        return FIGMA_RENDER_SHADER_DEBUG;
    }

    return FIGMA_RENDER_SHADER_COLOR;
}

//////////////////////////////////////////////////////////////////////////
static figma_render_texture_type_t __figma_render_texture_type(const figma_render_command_t * command)
{
    if(command->type == FIGMA_RENDER_COMMAND_IMAGE)
    {
        return FIGMA_RENDER_TEXTURE_ASSET;
    }

    if(command->type == FIGMA_RENDER_COMMAND_TEXT)
    {
        return FIGMA_RENDER_TEXTURE_GENERATED;
    }

    return FIGMA_RENDER_TEXTURE_NONE;
}

//////////////////////////////////////////////////////////////////////////
uint32_t FIGMA_CALL figma_render_list_get_batch_count(const figma_render_list_t * render_list)
{
    if(render_list == NULL || render_list->commands.size > UINT32_MAX)
    {
        return 0u;
    }

    return (uint32_t)render_list->commands.size;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_render_list_get_batch(const figma_render_list_t * render_list, uint32_t index, figma_render_batch_desc_t * batch)
{
    const figma_render_command_t * command;

    if(render_list == NULL || batch == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    if((size_t)index >= render_list->commands.size)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }

    command = FIGMA_ARRAY_CONST_PTR(
        figma_render_command_t, &render_list->commands, index);
    memset(batch, 0, sizeof(*batch));
    batch->batch_type = __figma_render_batch_type(command);
    batch->shader_type = __figma_render_shader_type(command);
    batch->texture_type = __figma_render_texture_type(command);
    batch->texture_key =
        command->type == FIGMA_RENDER_COMMAND_IMAGE
            ? figma_string_view(&command->asset_id)
            : command->type == FIGMA_RENDER_COMMAND_TEXT
                ? figma_string_view(&command->node_id)
                : figma_string_view_cstr(NULL);
    batch->blend_mode = command->blend_mode;
    batch->opacity = command->opacity;
    batch->render_layer_id = command->render_layer_id;
    batch->render_layer_opacity = command->render_layer_opacity;
    memcpy(
        batch->filter_color_adjust,
        command->filter_color_adjust,
        sizeof(batch->filter_color_adjust));
    memcpy(batch->paint_filter, command->paint_filter, sizeof(batch->paint_filter));
    batch->clip_rect = command->rect;
    batch->vertex_count =
        command->vertices.size <= UINT32_MAX ? (uint32_t)command->vertices.size : 0u;
    batch->vertices = (const figma_render_vertex_t *)command->vertices.data;
    batch->index_count =
        command->indices.size <= UINT32_MAX ? (uint32_t)command->indices.size : 0u;
    batch->indices = (const uint16_t *)command->indices.data;
    batch->has_filter_color_adjust = command->has_filter_color_adjust;
    batch->has_paint_filter = command->has_paint_filter;

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_render_list_get_generated_texture(const figma_render_list_t * render_list, uint32_t index, figma_render_generated_texture_desc_t * desc)
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

    if(command->type != FIGMA_RENDER_COMMAND_TEXT)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    memset(desc, 0, sizeof(*desc));
    desc->key = figma_string_view(&command->node_id);
    desc->text = figma_string_view(&command->text);
    desc->font_family = figma_string_view(&command->font_family);
    desc->font_style = figma_string_view(&command->font_style);
    desc->font_postscript_name = figma_string_view(&command->font_postscript_name);
    desc->rect = command->rect;
    desc->color = command->color;
    desc->text_align_horizontal = command->text_align_horizontal;
    desc->text_align_vertical = command->text_align_vertical;
    desc->font_size = command->font_size;
    desc->line_height = command->line_height;
    desc->font_weight = command->font_weight;
    desc->text_line_count =
        command->text_lines.size <= UINT32_MAX ? (uint32_t)command->text_lines.size : 0u;

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_render_list_get_generated_texture_text_line(const figma_render_list_t * render_list, uint32_t index, uint32_t line_index, figma_render_generated_text_line_desc_t * line)
{
    const figma_render_command_t * command;
    const figma_render_text_line_t * source;

    if(render_list == NULL || line == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    if((size_t)index >= render_list->commands.size)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }

    command = FIGMA_ARRAY_CONST_PTR(
        figma_render_command_t, &render_list->commands, index);

    if(command->type != FIGMA_RENDER_COMMAND_TEXT)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    if((size_t)line_index >= command->text_lines.size)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }

    source = FIGMA_ARRAY_CONST_PTR(
        figma_render_text_line_t, &command->text_lines, line_index);
    line->text = figma_string_view(&source->text);
    line->x = source->x;
    line->y = source->y;
    line->width = source->width;
    line->line_height = source->line_height;
    line->line_ascent = source->line_ascent;

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
void FIGMA_CALL figma_document_destroy(figma_document_t * document)
{
    figma_memory_t * memory;

    if(document == NULL)
    {
        return;
    }

    memory = document->memory;
    figma_document_deinit(document);
    figma_memory_deallocate(memory, document);
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t FIGMA_CALL figma_document_get_frame_rect(const figma_document_t * document, figma_string_view_t node_id, figma_rectf_t * rect)
{
    const figma_canvas_node_t * node;

    if(document == NULL || rect == NULL)
    {
        return FIGMA_FALSE;
    }

    node = figma_document_find_canvas_node(document, node_id);

    if(node == NULL)
    {
        return FIGMA_FALSE;
    }

    *rect = node->rect;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t FIGMA_CALL figma_document_get_prototype_start_frame_rect(const figma_document_t * document, figma_rectf_t * rect)
{
    const figma_canvas_node_t * node;

    if(document == NULL || rect == NULL)
    {
        return FIGMA_FALSE;
    }

    node = figma_document_get_prototype_start_frame(document);

    if(node == NULL)
    {
        return FIGMA_FALSE;
    }

    *rect = node->rect;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t FIGMA_CALL figma_document_find_asset(const figma_document_t * document, figma_string_view_t asset_id, figma_asset_desc_t * asset)
{
    const figma_asset_t * source;

    if(document == NULL || asset == NULL)
    {
        return FIGMA_FALSE;
    }

    source = figma_document_find_asset_internal(document, asset_id);

    if(source == NULL)
    {
        return FIGMA_FALSE;
    }

    asset->id = figma_string_view(&source->id);
    asset->path = figma_string_view(&source->path);
    asset->mime = figma_string_view(&source->mime);
    asset->bytes.data = (const uint8_t *)source->bytes.data;
    asset->bytes.size = source->bytes.size;
    asset->width = source->width;
    asset->height = source->height;
    asset->color_type = source->color_type;

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
const figma_diagnostics_t * FIGMA_CALL figma_document_get_diagnostics(const figma_document_t * document)
{
    return document != NULL ? &document->diagnostics : NULL;
}

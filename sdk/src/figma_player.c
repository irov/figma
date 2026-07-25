#include "figma_player.h"

#include "graphics/graphics.h"

#include <stddef.h>
#include <math.h>
#include <string.h>

//////////////////////////////////////////////////////////////////////////
static float __figma_player_maxf(float left, float right)
{
    return left > right ? left : right;
}

//////////////////////////////////////////////////////////////////////////
static float __figma_player_minf(float left, float right)
{
    return left < right ? left : right;
}

//////////////////////////////////////////////////////////////////////////
static float __figma_player_clamp01(float value)
{
    return __figma_player_maxf(0.0f, __figma_player_minf(1.0f, value));
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_contains_rect(const figma_rectf_t * rect, float x, float y)
{
    return x >= rect->x && y >= rect->y && x <= rect->x + rect->w &&
        y <= rect->y + rect->h;
}

//////////////////////////////////////////////////////////////////////////
static float __figma_player_cross(const figma_vec2f_t * left, const figma_vec2f_t * right, float x, float y)
{
    return (right->x - left->x) * (y - left->y) -
        (right->y - left->y) * (x - left->x);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_contains_quad(const figma_vec2f_t quad[4], float x, float y)
{
    figma_bool_t positive = FIGMA_FALSE;
    figma_bool_t negative = FIGMA_FALSE;
    size_t index;

    for(index = 0u; index != 4u; ++index)
    {
        const float value =
            __figma_player_cross(&quad[index], &quad[(index + 1u) % 4u], x, y);

        if(value > 0.0001f)
        {
            positive = FIGMA_TRUE;
        }

        if(value < -0.0001f)
        {
            negative = FIGMA_TRUE;
        }
    }

    return positive == FIGMA_TRUE && negative == FIGMA_TRUE
        ? FIGMA_FALSE
        : FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_contains_hotspot(const figma_player_hotspot_t * hotspot, float x, float y)
{
    if(hotspot->has_clip == FIGMA_TRUE &&
        __figma_player_contains_rect(&hotspot->clip, x, y) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    return __figma_player_contains_rect(&hotspot->rect, x, y) == FIGMA_TRUE &&
        __figma_player_contains_quad(hotspot->quad, x, y) == FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_trigger_type_t __figma_player_trigger_type(figma_prototype_event_type_t type)
{
    switch(type)
    {
    case FIGMA_PROTOTYPE_EVENT_CLICK:
        return FIGMA_TRIGGER_CLICK;
    case FIGMA_PROTOTYPE_EVENT_HOVER_ENTER:
        return FIGMA_TRIGGER_HOVER_ENTER;
    case FIGMA_PROTOTYPE_EVENT_HOVER_LEAVE:
        return FIGMA_TRIGGER_HOVER_LEAVE;
    case FIGMA_PROTOTYPE_EVENT_PRESS:
        return FIGMA_TRIGGER_PRESS;
    case FIGMA_PROTOTYPE_EVENT_POINTER_DOWN:
        return FIGMA_TRIGGER_POINTER_DOWN;
    case FIGMA_PROTOTYPE_EVENT_POINTER_UP:
        return FIGMA_TRIGGER_POINTER_UP;
    case FIGMA_PROTOTYPE_EVENT_AFTER_TIMEOUT:
        return FIGMA_TRIGGER_AFTER_TIMEOUT;
    case FIGMA_PROTOTYPE_EVENT_KEY_DOWN:
        return FIGMA_TRIGGER_KEY_DOWN;
    case FIGMA_PROTOTYPE_EVENT_UNSUPPORTED:
        break;
    }

    return FIGMA_TRIGGER_UNSUPPORTED;
}

//////////////////////////////////////////////////////////////////////////
static figma_connection_type_t __figma_player_connection_type(figma_prototype_connection_type_t type)
{
    switch(type)
    {
    case FIGMA_PROTOTYPE_CONNECTION_NONE:
        return FIGMA_CONNECTION_NONE;
    case FIGMA_PROTOTYPE_CONNECTION_INTERNAL_NODE:
        return FIGMA_CONNECTION_INTERNAL_NODE;
    case FIGMA_PROTOTYPE_CONNECTION_BACK:
        return FIGMA_CONNECTION_BACK;
    case FIGMA_PROTOTYPE_CONNECTION_CLOSE:
        return FIGMA_CONNECTION_CLOSE;
    case FIGMA_PROTOTYPE_CONNECTION_UNSUPPORTED:
        break;
    }

    return FIGMA_CONNECTION_UNSUPPORTED;
}

//////////////////////////////////////////////////////////////////////////
static figma_navigation_type_t __figma_player_navigation_type(figma_prototype_navigation_type_t type)
{
    switch(type)
    {
    case FIGMA_PROTOTYPE_NAVIGATION_NAVIGATE:
        return FIGMA_NAVIGATION_NAVIGATE;
    case FIGMA_PROTOTYPE_NAVIGATION_OVERLAY:
        return FIGMA_NAVIGATION_OVERLAY;
    case FIGMA_PROTOTYPE_NAVIGATION_SWAP:
        return FIGMA_NAVIGATION_SWAP;
    case FIGMA_PROTOTYPE_NAVIGATION_SCROLL_TO:
        return FIGMA_NAVIGATION_SCROLL_TO;
    case FIGMA_PROTOTYPE_NAVIGATION_UNSUPPORTED:
        break;
    }

    return FIGMA_NAVIGATION_UNSUPPORTED;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_string_element_destroy(figma_memory_t * memory, void * value)
{
    figma_string_destroy(memory, (figma_string_t *)value);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_hotspot_init(figma_player_hotspot_t * hotspot)
{
    memset(hotspot, 0, sizeof(*hotspot));
    figma_string_init(&hotspot->node_id);
    figma_string_init(&hotspot->action_id);
    figma_string_init(&hotspot->target_frame_id);
    hotspot->event_type = FIGMA_PROTOTYPE_EVENT_CLICK;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_hotspot_destroy(figma_memory_t * memory, void * value)
{
    figma_player_hotspot_t * hotspot = (figma_player_hotspot_t *)value;
    figma_string_destroy(memory, &hotspot->node_id);
    figma_string_destroy(memory, &hotspot->action_id);
    figma_string_destroy(memory, &hotspot->target_frame_id);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_capture_destroy(figma_memory_t * memory, void * value)
{
    figma_player_pointer_capture_t * capture =
        (figma_player_pointer_capture_t *)value;
    figma_string_destroy(memory, &capture->node_id);
    figma_string_destroy(memory, &capture->interaction_id);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_swap_destroy(figma_memory_t * memory, void * value)
{
    figma_player_node_swap_t * swap = (figma_player_node_swap_t *)value;
    figma_string_destroy(memory, &swap->source_node_id);
    figma_string_destroy(memory, &swap->current_node_id);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_local_animation_destroy(figma_memory_t * memory, void * value)
{
    figma_player_local_animation_t * animation =
        (figma_player_local_animation_t *)value;
    figma_string_destroy(memory, &animation->source_node_id);
    figma_string_destroy(memory, &animation->from_node_id);
    figma_string_destroy(memory, &animation->target_node_id);
    figma_array_destroy(memory, &animation->tracks, &figma_animation_track_destroy);
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_override_destroy(figma_memory_t * memory, void * value)
{
    figma_binding_override_t * override = (figma_binding_override_t *)value;
    figma_string_destroy(memory, &override->key);
    figma_string_destroy(memory, &override->string_value);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_string_array_contains(const figma_array_t * values, figma_string_view_t value)
{
    size_t index;

    for(index = 0u; index != values->size; ++index)
    {
        const figma_string_t * item =
            FIGMA_ARRAY_CONST_PTR(figma_string_t, values, index);
        if(figma_string_equal_view(item, value) == FIGMA_TRUE)
        {
            return FIGMA_TRUE;
        }
    }

    return FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_string_array_add(figma_player_t * player, figma_array_t * values, figma_string_view_t value)
{
    figma_string_t * item;

    if(__figma_player_string_array_contains(values, value) == FIGMA_TRUE)
    {
        return FIGMA_TRUE;
    }

    item = (figma_string_t *)figma_array_push_uninitialized(
        player->memory, values);
    if(item == NULL)
    {
        return FIGMA_FALSE;
    }

    figma_string_init(item);
    if(figma_string_assign(player->memory, item, value) == FIGMA_FALSE)
    {
        figma_array_pop(
            player->memory, values, &__figma_player_string_element_destroy);
        return FIGMA_FALSE;
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_player_node_swap_t * __figma_player_find_swap(const figma_player_t * player, figma_string_view_t source_node_id)
{
    size_t index;

    for(index = 0u; index != player->node_swaps.size; ++index)
    {
        figma_player_node_swap_t * swap =
            FIGMA_ARRAY_PTR(figma_player_node_swap_t, &player->node_swaps, index);
        if(figma_string_equal_view(&swap->source_node_id, source_node_id) ==
            FIGMA_TRUE)
        {
            return swap;
        }
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////
static figma_player_local_animation_t * __figma_player_find_local_animation(const figma_player_t * player, figma_string_view_t source_node_id)
{
    size_t index;

    for(index = 0u; index != player->local_animations.size; ++index)
    {
        figma_player_local_animation_t * animation =
            FIGMA_ARRAY_PTR(
                figma_player_local_animation_t, &player->local_animations, index);
        if(animation->active == FIGMA_TRUE &&
            figma_string_equal_view(
                &animation->source_node_id, source_node_id) == FIGMA_TRUE)
        {
            return animation;
        }
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////
static figma_binding_override_t * __figma_player_find_override(const figma_player_t * player, figma_string_view_t key)
{
    size_t index;

    for(index = 0u; index != player->overrides.size; ++index)
    {
        figma_binding_override_t * override =
            FIGMA_ARRAY_PTR(figma_binding_override_t, &player->overrides, index);
        if(figma_string_equal_view(&override->key, key) == FIGMA_TRUE)
        {
            return override;
        }
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////
static figma_player_pointer_capture_t * __figma_player_find_capture(const figma_player_t * player, uint32_t pointer_id, size_t * found_index)
{
    size_t index;

    for(index = 0u; index != player->pointer_captures.size; ++index)
    {
        figma_player_pointer_capture_t * capture = FIGMA_ARRAY_PTR(
            figma_player_pointer_capture_t, &player->pointer_captures, index);
        if(capture->pointer_id == pointer_id)
        {
            if(found_index != NULL)
            {
                *found_index = index;
            }
            return capture;
        }
    }

    return NULL;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_contains_node(const figma_canvas_node_t * root, const figma_canvas_node_t * node)
{
    size_t index;

    if(root == node)
    {
        return FIGMA_TRUE;
    }

    for(index = 0u; index != root->children.size; ++index)
    {
        if(__figma_player_contains_node(
               FIGMA_ARRAY_CONST_PTR(
                   figma_canvas_node_t, &root->children, index),
               node) == FIGMA_TRUE)
        {
            return FIGMA_TRUE;
        }
    }

    return FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_binding_value_t __figma_player_resolve_binding(figma_player_t * player, const figma_binding_t * binding)
{
    figma_binding_value_t value;
    figma_binding_override_t * override;

    memset(&value, 0, sizeof(value));
    value.type = FIGMA_BINDING_VALUE_NONE;

    override = __figma_player_find_override(
        player, figma_string_view(&binding->key));
    if(override != NULL)
    {
        value.type = override->type;
        value.string_value = figma_string_view(&override->string_value);
        value.number_value = override->number_value;
        value.bool_value = override->bool_value;
        return value;
    }

    if(player->data_context.get_binding_value != NULL)
    {
        figma_binding_value_t external;
        memset(&external, 0, sizeof(external));
        external.type = FIGMA_BINDING_VALUE_NONE;
        if(player->data_context.get_binding_value(
               player->data_context.user_data,
               figma_string_view(&binding->key),
               &external) == FIGMA_TRUE)
        {
            return external;
        }
    }

    return value;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_is_node_property_true(figma_player_t * player, const figma_canvas_node_t * node, figma_binding_property_t property)
{
    size_t index;

    for(index = 0u; index != player->document->bindings.size; ++index)
    {
        const figma_binding_t * binding =
            FIGMA_ARRAY_CONST_PTR(
                figma_binding_t, &player->document->bindings, index);
        figma_binding_value_t value;

        if(binding->property != property ||
            figma_string_equal(&binding->node_id, &node->id) == FIGMA_FALSE)
        {
            continue;
        }

        value = __figma_player_resolve_binding(player, binding);
        if(value.type == FIGMA_BINDING_VALUE_NONE)
        {
            return FIGMA_TRUE;
        }

        if(value.type == FIGMA_BINDING_VALUE_BOOLEAN)
        {
            return value.bool_value;
        }

        return FIGMA_TRUE;
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_player_is_node_visible(figma_player_t * player, const figma_canvas_node_t * node)
{
    return __figma_player_is_node_property_true(
        player, node, FIGMA_BINDING_PROPERTY_VISIBLE);
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_player_is_node_enabled(figma_player_t * player, const figma_canvas_node_t * node)
{
    return __figma_player_is_node_property_true(
        player, node, FIGMA_BINDING_PROPERTY_ENABLED);
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_player_resolve_text(figma_player_t * player, const figma_canvas_node_t * node, figma_binding_value_t * value)
{
    size_t index;

    for(index = 0u; index != player->document->bindings.size; ++index)
    {
        const figma_binding_t * binding =
            FIGMA_ARRAY_CONST_PTR(
                figma_binding_t, &player->document->bindings, index);

        if(binding->property != FIGMA_BINDING_PROPERTY_TEXT ||
            figma_string_equal(&binding->node_id, &node->id) == FIGMA_FALSE)
        {
            continue;
        }

        if(node->type != FIGMA_CANVAS_NODE_TEXT)
        {
            FIGMA_DIAGNOSTIC_ADD(
                &player->diagnostics,
                FIGMA_DIAGNOSTIC_WARNING,
                "ux_binding_text_target_invalid",
                "Text binding target is not a decoded text node; command skipped",
                node->id.data);
            return FIGMA_FALSE;
        }

        *value = __figma_player_resolve_binding(player, binding);
        return value->type != FIGMA_BINDING_VALUE_NONE
            ? FIGMA_TRUE
            : FIGMA_FALSE;
    }

    return FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
figma_bool_t figma_player_resolve_image(figma_player_t * player, const figma_canvas_node_t * node, figma_string_view_t * asset_id)
{
    size_t index;

    for(index = 0u; index != player->document->bindings.size; ++index)
    {
        const figma_binding_t * binding =
            FIGMA_ARRAY_CONST_PTR(
                figma_binding_t, &player->document->bindings, index);
        figma_binding_value_t value;

        if(binding->property != FIGMA_BINDING_PROPERTY_IMAGE ||
            figma_string_equal(&binding->node_id, &node->id) == FIGMA_FALSE)
        {
            continue;
        }

        value = __figma_player_resolve_binding(player, binding);
        if(value.type == FIGMA_BINDING_VALUE_NONE)
        {
            return FIGMA_FALSE;
        }

        if(value.type != FIGMA_BINDING_VALUE_IMAGE &&
            value.type != FIGMA_BINDING_VALUE_TEXT)
        {
            FIGMA_DIAGNOSTIC_ADD(
                &player->diagnostics,
                FIGMA_DIAGNOSTIC_WARNING,
                "ux_binding_image_value_invalid",
                "Image binding did not resolve to an asset id string; decoded image command kept unchanged",
                node->id.data);
            return FIGMA_FALSE;
        }

        *asset_id = value.string_value;
        return asset_id->size != 0u ? FIGMA_TRUE : FIGMA_FALSE;
    }

    return FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_set_current_frame(figma_player_t * player, const figma_canvas_node_t * frame)
{
    figma_string_t previous;

    figma_string_init(&previous);
    if(figma_string_copy(
           player->memory, &previous, &player->current_frame_id) == FIGMA_FALSE)
    {
        return;
    }

    player->current_frame = frame;
    figma_string_clear(&player->current_frame_id);
    if(frame != NULL)
    {
        if(figma_string_copy(
               player->memory, &player->current_frame_id, &frame->id) ==
            FIGMA_FALSE)
        {
            player->current_frame = NULL;
        }
    }

    player->hotspots_dirty = FIGMA_TRUE;
    figma_array_clear(
        player->memory,
        &player->fired_timer_ids,
        &__figma_player_string_element_destroy);

    if(player->action_router.on_frame_changed != NULL &&
        figma_string_equal(&previous, &player->current_frame_id) == FIGMA_FALSE)
    {
        player->action_router.on_frame_changed(
            player->action_router.user_data,
            figma_string_view(&previous),
            figma_string_view(&player->current_frame_id));
    }

    figma_string_destroy(player->memory, &previous);
}

//////////////////////////////////////////////////////////////////////////
static const figma_canvas_node_t * __figma_player_resolve_initial_frame(const figma_player_t * player)
{
    if(player->start_frame_id.size != 0u)
    {
        return figma_document_find_canvas_node(
            player->document, figma_string_view(&player->start_frame_id));
    }

    return figma_document_get_prototype_start_frame(player->document);
}

//////////////////////////////////////////////////////////////////////////
static const figma_player_hotspot_t * __figma_player_find_hotspot(const figma_player_t * player, float x, float y, figma_prototype_event_type_t event_type)
{
    size_t index = player->hotspots.size;

    while(index != 0u)
    {
        const figma_player_hotspot_t * hotspot =
            FIGMA_ARRAY_CONST_PTR(
                figma_player_hotspot_t, &player->hotspots, index - 1u);
        --index;

        if(hotspot->event_type == event_type &&
            __figma_player_contains_hotspot(hotspot, x, y) == FIGMA_TRUE)
        {
            return hotspot;
        }
    }

    return NULL;
}

static figma_result_t __figma_player_navigate_internal(figma_player_t * player, figma_string_view_t target_frame_id, const figma_prototype_action_t * action, figma_string_view_t source_node_id, float initial_elapsed);

static figma_result_t __figma_player_swap_state(figma_player_t * player, figma_string_view_t source_node_id, figma_string_view_t from_node_id, const figma_prototype_action_t * action, float initial_elapsed);

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_execute_prototype_action(figma_player_t * player, const figma_prototype_action_t * action, figma_string_view_t source_node_id, float initial_elapsed)
{
    if(action->connection_type == FIGMA_PROTOTYPE_CONNECTION_BACK)
    {
        return figma_player_go_back(player);
    }

    if(action->connection_type == FIGMA_PROTOTYPE_CONNECTION_CLOSE)
    {
        return figma_player_close_overlay(player);
    }

    if(action->navigation_type == FIGMA_PROTOTYPE_NAVIGATION_OVERLAY)
    {
        return figma_player_open_overlay(
            player, figma_string_view(&action->target_node_id));
    }

    if(action->navigation_type == FIGMA_PROTOTYPE_NAVIGATION_SWAP)
    {
        return __figma_player_swap_state(
            player, source_node_id, source_node_id, action, initial_elapsed);
    }

    if(action->navigation_type == FIGMA_PROTOTYPE_NAVIGATION_SCROLL_TO ||
        action->navigation_type == FIGMA_PROTOTYPE_NAVIGATION_UNSUPPORTED)
    {
        return FIGMA_RESULT_INVALID_STATE;
    }

    return __figma_player_navigate_internal(
        player,
        figma_string_view(&action->target_node_id),
        action,
        source_node_id,
        initial_elapsed);
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_execute_action_response(figma_player_t * player, const figma_action_response_t * response, const figma_prototype_action_t * action, figma_string_view_t source_node_id, figma_string_view_t default_target_frame_id, float initial_elapsed)
{
    figma_string_view_t target_frame_id = response->target_frame_id.size != 0u
        ? response->target_frame_id
        : default_target_frame_id;

    if(response->result == FIGMA_ACTION_RESULT_CONSUME)
    {
        return FIGMA_RESULT_OK;
    }

    if(response->result == FIGMA_ACTION_RESULT_CLOSE_OVERLAY)
    {
        return figma_player_close_overlay(player);
    }

    if(response->result == FIGMA_ACTION_RESULT_OPEN_OVERLAY)
    {
        return figma_player_open_overlay(player, target_frame_id);
    }

    if(response->result == FIGMA_ACTION_RESULT_NAVIGATE_FRAME)
    {
        return __figma_player_navigate_internal(
            player, target_frame_id, NULL, source_node_id, initial_elapsed);
    }

    if(action != NULL)
    {
        return __figma_player_execute_prototype_action(
            player, action, source_node_id, initial_elapsed);
    }

    if(target_frame_id.size != 0u)
    {
        return __figma_player_navigate_internal(
            player, target_frame_id, NULL, source_node_id, initial_elapsed);
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_route_prototype_action(figma_player_t * player, const figma_player_hotspot_t * hotspot, const figma_prototype_action_t * action, figma_action_input_kind_t input_kind, const figma_pointer_event_t * pointer, const figma_key_event_t * key, float initial_elapsed)
{
    figma_action_response_t response;

    memset(&response, 0, sizeof(response));
    response.result = FIGMA_ACTION_RESULT_ALLOW_DEFAULT;

    if(player->action_router.route_action != NULL)
    {
        const char * action_id = "figma.prototype.navigate";
        figma_action_event_t event;
        figma_result_t result;

        if(action->connection_type == FIGMA_PROTOTYPE_CONNECTION_BACK)
        {
            action_id = "figma.prototype.back";
        }
        else if(action->connection_type == FIGMA_PROTOTYPE_CONNECTION_CLOSE)
        {
            action_id = "figma.prototype.close";
        }
        else if(action->navigation_type == FIGMA_PROTOTYPE_NAVIGATION_OVERLAY)
        {
            action_id = "figma.prototype.overlay";
        }
        else if(action->navigation_type == FIGMA_PROTOTYPE_NAVIGATION_SWAP)
        {
            action_id = "figma.prototype.swap";
        }

        memset(&event, 0, sizeof(event));
        event.input_kind = input_kind;
        event.trigger_type = __figma_player_trigger_type(hotspot->event_type);
        event.connection_type =
            __figma_player_connection_type(action->connection_type);
        event.navigation_type =
            __figma_player_navigation_type(action->navigation_type);
        event.action_id = figma_string_view_cstr(action_id);
        if(hotspot->interaction != NULL)
        {
            event.interaction_id =
                figma_string_view(&hotspot->interaction->id);
        }
        event.source_node_id = figma_string_view(&hotspot->node_id);
        event.current_frame_id = figma_string_view(&player->current_frame_id);
        event.target_frame_id = figma_string_view(&action->target_node_id);
        if(pointer != NULL)
        {
            event.pointer = *pointer;
        }
        if(key != NULL)
        {
            event.key = *key;
        }
        event.player_user_data = player->desc.user_data;

        result = player->action_router.route_action(
            player->action_router.user_data, &event, &response);
        if(result != FIGMA_RESULT_OK)
        {
            return result;
        }
    }

    return __figma_player_execute_action_response(
        player,
        &response,
        action,
        figma_string_view(&hotspot->node_id),
        figma_string_view(&action->target_node_id),
        initial_elapsed);
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_route_hotspot(figma_player_t * player, const figma_player_hotspot_t * hotspot, figma_action_input_kind_t input_kind, const figma_pointer_event_t * pointer, const figma_key_event_t * key, float initial_elapsed)
{
    size_t index;

    if(player->action_router.route_trigger != NULL)
    {
        figma_trigger_event_t trigger;
        figma_result_t result;

        memset(&trigger, 0, sizeof(trigger));
        trigger.input_kind = input_kind;
        trigger.trigger_type = __figma_player_trigger_type(hotspot->event_type);
        if(hotspot->interaction != NULL)
        {
            trigger.interaction_id =
                figma_string_view(&hotspot->interaction->id);
        }
        trigger.source_node_id = figma_string_view(&hotspot->node_id);
        trigger.current_frame_id =
            figma_string_view(&player->current_frame_id);
        if(pointer != NULL)
        {
            trigger.pointer = *pointer;
        }
        if(key != NULL)
        {
            trigger.key = *key;
        }
        trigger.player_user_data = player->desc.user_data;

        result = player->action_router.route_trigger(
            player->action_router.user_data, &trigger);
        if(result != FIGMA_RESULT_OK)
        {
            return result;
        }
    }

    if(hotspot->ux_action == FIGMA_TRUE)
    {
        figma_action_response_t response;
        memset(&response, 0, sizeof(response));
        response.result = FIGMA_ACTION_RESULT_ALLOW_DEFAULT;

        if(player->action_router.route_action != NULL)
        {
            figma_action_event_t event;
            figma_result_t result;

            memset(&event, 0, sizeof(event));
            event.input_kind = input_kind;
            event.trigger_type = __figma_player_trigger_type(hotspot->event_type);
            event.action_id = figma_string_view(&hotspot->action_id);
            event.source_node_id = figma_string_view(&hotspot->node_id);
            event.current_frame_id =
                figma_string_view(&player->current_frame_id);
            event.target_frame_id =
                figma_string_view(&hotspot->target_frame_id);
            if(pointer != NULL)
            {
                event.pointer = *pointer;
            }
            if(key != NULL)
            {
                event.key = *key;
            }
            event.player_user_data = player->desc.user_data;

            result = player->action_router.route_action(
                player->action_router.user_data, &event, &response);
            if(result != FIGMA_RESULT_OK)
            {
                return result;
            }
        }

        return __figma_player_execute_action_response(
            player,
            &response,
            NULL,
            figma_string_view(&hotspot->node_id),
            figma_string_view(&hotspot->target_frame_id),
            initial_elapsed);
    }

    if(hotspot->interaction == NULL)
    {
        return FIGMA_RESULT_OK;
    }

    for(index = 0u; index != hotspot->interaction->actions.size; ++index)
    {
        figma_result_t result = __figma_player_route_prototype_action(
            player,
            hotspot,
            FIGMA_ARRAY_CONST_PTR(
                figma_prototype_action_t,
                &hotspot->interaction->actions,
                index),
            input_kind,
            pointer,
            key,
            initial_elapsed);
        if(result != FIGMA_RESULT_OK)
        {
            return result;
        }
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_add_hotspot(figma_player_t * player, const figma_canvas_node_t * node, const figma_prototype_interaction_t * interaction, const figma_rectf_t * rect, const figma_vec2f_t quad[4], const figma_rectf_t * clip)
{
    figma_player_hotspot_t * hotspot =
        (figma_player_hotspot_t *)figma_array_push_uninitialized(
            player->memory, &player->hotspots);

    if(hotspot == NULL)
    {
        return FIGMA_FALSE;
    }

    __figma_player_hotspot_init(hotspot);
    hotspot->rect = *rect;
    memcpy(hotspot->quad, quad, sizeof(hotspot->quad));
    if(clip != NULL)
    {
        hotspot->clip = *clip;
        hotspot->has_clip = FIGMA_TRUE;
    }
    hotspot->interaction = interaction;
    hotspot->event_type = interaction->event_type;
    hotspot->key_code = interaction->key_code;

    if(figma_string_copy(player->memory, &hotspot->node_id, &node->id) ==
        FIGMA_FALSE)
    {
        figma_array_pop(
            player->memory, &player->hotspots, &__figma_player_hotspot_destroy);
        return FIGMA_FALSE;
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_append_prototype_hotspots(figma_player_t * player, const figma_canvas_node_t * node, float offset_x, float offset_y, const figma_rectf_t * clip)
{
    const figma_player_node_swap_t * swap;
    figma_rectf_t node_rect;
    figma_rectf_t child_clip;
    const figma_rectf_t * effective_clip = clip;
    figma_vec2f_t node_quad[4];
    figma_bool_t has_quad = FIGMA_FALSE;
    size_t index;

    if(node->visible == FIGMA_FALSE ||
        figma_player_is_node_visible(player, node) == FIGMA_FALSE ||
        figma_player_is_node_enabled(player, node) == FIGMA_FALSE)
    {
        return FIGMA_TRUE;
    }

    swap = __figma_player_find_swap(player, figma_string_view(&node->id));
    if(swap != NULL)
    {
        const figma_canvas_node_t * swapped_node =
            figma_document_find_canvas_node(
                player->document, figma_string_view(&swap->current_node_id));
        if(swapped_node != NULL)
        {
            return __figma_player_append_prototype_hotspots(
                player,
                swapped_node,
                offset_x + node->rect.x - swapped_node->rect.x,
                offset_y + node->rect.y - swapped_node->rect.y,
                clip);
        }
        return FIGMA_TRUE;
    }

    node_rect.x = node->rect.x + offset_x;
    node_rect.y = node->rect.y + offset_y;
    node_rect.w = node->rect.w;
    node_rect.h = node->rect.h;

    for(index = 0u; index != 4u; ++index)
    {
        node_quad[index].x = node->quad[index].x + offset_x;
        node_quad[index].y = node->quad[index].y + offset_y;
        if(node->quad[index].x != 0.0f || node->quad[index].y != 0.0f)
        {
            has_quad = FIGMA_TRUE;
        }
    }

    if(has_quad == FIGMA_FALSE)
    {
        node_quad[0].x = node_rect.x;
        node_quad[0].y = node_rect.y;
        node_quad[1].x = node_rect.x + node_rect.w;
        node_quad[1].y = node_rect.y;
        node_quad[2].x = node_rect.x + node_rect.w;
        node_quad[2].y = node_rect.y + node_rect.h;
        node_quad[3].x = node_rect.x;
        node_quad[3].y = node_rect.y + node_rect.h;
    }

    {
        float left = node_quad[0].x;
        float top = node_quad[0].y;
        float right = node_quad[0].x;
        float bottom = node_quad[0].y;

        for(index = 1u; index != 4u; ++index)
        {
            left = __figma_player_minf(left, node_quad[index].x);
            top = __figma_player_minf(top, node_quad[index].y);
            right = __figma_player_maxf(right, node_quad[index].x);
            bottom = __figma_player_maxf(bottom, node_quad[index].y);
        }

        node_rect.x = left;
        node_rect.y = top;
        node_rect.w = right - left;
        node_rect.h = bottom - top;
    }

    if(node->type == FIGMA_CANVAS_NODE_FRAME &&
        node->frame_mask_disabled == FIGMA_FALSE)
    {
        child_clip = node_rect;
        if(clip != NULL)
        {
            const float left = __figma_player_maxf(child_clip.x, clip->x);
            const float top = __figma_player_maxf(child_clip.y, clip->y);
            const float right = __figma_player_minf(
                child_clip.x + child_clip.w, clip->x + clip->w);
            const float bottom = __figma_player_minf(
                child_clip.y + child_clip.h, clip->y + clip->h);
            child_clip.x = left;
            child_clip.y = top;
            child_clip.w = __figma_player_maxf(0.0f, right - left);
            child_clip.h = __figma_player_maxf(0.0f, bottom - top);
        }
        effective_clip = &child_clip;
    }

    for(index = 0u; index != node->prototype_interactions.size; ++index)
    {
        const figma_prototype_interaction_t * interaction =
            FIGMA_ARRAY_CONST_PTR(
                figma_prototype_interaction_t,
                &node->prototype_interactions,
                index);

        if(interaction->event_type == FIGMA_PROTOTYPE_EVENT_AFTER_TIMEOUT ||
            interaction->event_type == FIGMA_PROTOTYPE_EVENT_UNSUPPORTED)
        {
            continue;
        }

        if(__figma_player_add_hotspot(
               player,
               node,
               interaction,
               &node_rect,
               node_quad,
               effective_clip) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    index = node->children.size;
    while(index != 0u)
    {
        --index;
        if(__figma_player_append_prototype_hotspots(
               player,
               FIGMA_ARRAY_CONST_PTR(
                   figma_canvas_node_t, &node->children, index),
               offset_x,
               offset_y,
               effective_clip) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_build_hotspots(figma_player_t * player)
{
    figma_rectf_t viewport_clip;
    const figma_canvas_node_t * prototype_frame;
    size_t index;

    figma_array_clear(
        player->memory, &player->hotspots, &__figma_player_hotspot_destroy);
    player->hotspots_dirty = FIGMA_FALSE;
    prototype_frame = player->current_frame;
    if(prototype_frame == NULL)
    {
        return FIGMA_RESULT_OK;
    }

    viewport_clip.x = 0.0f;
    viewport_clip.y = 0.0f;
    viewport_clip.w = player->desc.viewport.width;
    viewport_clip.h = player->desc.viewport.height;

    if(__figma_player_append_prototype_hotspots(
           player,
           prototype_frame,
           -prototype_frame->rect.x,
           -prototype_frame->rect.y,
           &viewport_clip) == FIGMA_FALSE)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    for(index = 0u; index != player->overlay_frames.size; ++index)
    {
        const figma_canvas_node_t * overlay =
            FIGMA_ARRAY_AT(
                const figma_canvas_node_t *, &player->overlay_frames, index);
        const float offset_x = -overlay->rect.x +
            (player->desc.viewport.width - overlay->rect.w) * 0.5f;
        const float offset_y = -overlay->rect.y +
            (player->desc.viewport.height - overlay->rect.h) * 0.5f;

        if(__figma_player_append_prototype_hotspots(
               player, overlay, offset_x, offset_y, &viewport_clip) ==
            FIGMA_FALSE)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }
    }

    for(index = 0u; index != player->document->actions.size; ++index)
    {
        const figma_action_t * action =
            FIGMA_ARRAY_CONST_PTR(
                figma_action_t, &player->document->actions, index);
        const figma_canvas_node_t * node;
        const figma_canvas_node_t * owner_frame = NULL;
        figma_player_hotspot_t * hotspot;
        float offset_x = 0.0f;
        float offset_y = 0.0f;
        figma_bool_t has_quad = FIGMA_FALSE;
        size_t quad_index;

        if(action->event_type == FIGMA_PROTOTYPE_EVENT_UNSUPPORTED)
        {
            continue;
        }

        node = figma_document_find_canvas_node(
            player->document, figma_string_view(&action->node_id));
        if(node == NULL)
        {
            FIGMA_DIAGNOSTIC_ADD(
                &player->diagnostics,
                FIGMA_DIAGNOSTIC_WARNING,
                "ux_action_node_missing",
                "Action target node was not found in decoded document",
                action->node_id.data);
            continue;
        }

        if(__figma_player_contains_node(prototype_frame, node) == FIGMA_TRUE)
        {
            owner_frame = prototype_frame;
            offset_x = -prototype_frame->rect.x;
            offset_y = -prototype_frame->rect.y;
        }

        for(quad_index = 0u;
            quad_index != player->overlay_frames.size;
            ++quad_index)
        {
            const figma_canvas_node_t * overlay =
                FIGMA_ARRAY_AT(
                    const figma_canvas_node_t *,
                    &player->overlay_frames,
                    quad_index);
            if(__figma_player_contains_node(overlay, node) == FIGMA_TRUE)
            {
                owner_frame = overlay;
                offset_x = -overlay->rect.x +
                    (player->desc.viewport.width - overlay->rect.w) * 0.5f;
                offset_y = -overlay->rect.y +
                    (player->desc.viewport.height - overlay->rect.h) * 0.5f;
            }
        }

        if(owner_frame == NULL || node->visible == FIGMA_FALSE ||
            figma_player_is_node_visible(player, node) == FIGMA_FALSE ||
            figma_player_is_node_enabled(player, node) == FIGMA_FALSE)
        {
            continue;
        }

        hotspot = (figma_player_hotspot_t *)figma_array_push_uninitialized(
            player->memory, &player->hotspots);
        if(hotspot == NULL)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }

        __figma_player_hotspot_init(hotspot);
        hotspot->rect.x = node->rect.x + offset_x;
        hotspot->rect.y = node->rect.y + offset_y;
        hotspot->rect.w = node->rect.w;
        hotspot->rect.h = node->rect.h;
        for(quad_index = 0u; quad_index != 4u; ++quad_index)
        {
            hotspot->quad[quad_index].x =
                node->quad[quad_index].x + offset_x;
            hotspot->quad[quad_index].y =
                node->quad[quad_index].y + offset_y;
            if(node->quad[quad_index].x != 0.0f ||
                node->quad[quad_index].y != 0.0f)
            {
                has_quad = FIGMA_TRUE;
            }
        }
        if(has_quad == FIGMA_FALSE)
        {
            hotspot->quad[0].x = hotspot->rect.x;
            hotspot->quad[0].y = hotspot->rect.y;
            hotspot->quad[1].x = hotspot->rect.x + hotspot->rect.w;
            hotspot->quad[1].y = hotspot->rect.y;
            hotspot->quad[2].x = hotspot->rect.x + hotspot->rect.w;
            hotspot->quad[2].y = hotspot->rect.y + hotspot->rect.h;
            hotspot->quad[3].x = hotspot->rect.x;
            hotspot->quad[3].y = hotspot->rect.y + hotspot->rect.h;
        }

        hotspot->clip = viewport_clip;
        hotspot->has_clip = FIGMA_TRUE;
        hotspot->event_type = action->event_type;
        hotspot->key_code = action->key_code;
        hotspot->ux_action = FIGMA_TRUE;
        if(figma_string_copy(
               player->memory, &hotspot->node_id, &action->node_id) ==
                FIGMA_FALSE ||
            figma_string_copy(
                player->memory, &hotspot->action_id, &action->action_id) ==
                FIGMA_FALSE ||
            figma_string_copy(
                player->memory,
                &hotspot->target_frame_id,
                &action->target_frame_id) == FIGMA_FALSE)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t figma_player_rebuild_hotspots(figma_player_t * player)
{
    figma_array_t previous = player->hotspots;
    const figma_bool_t previous_dirty = player->hotspots_dirty;
    figma_result_t result;

    figma_array_init(
        &player->hotspots, sizeof(figma_player_hotspot_t));
    result = __figma_player_build_hotspots(player);
    if(result == FIGMA_RESULT_OK)
    {
        figma_array_destroy(
            player->memory, &previous, &__figma_player_hotspot_destroy);
        return FIGMA_RESULT_OK;
    }

    figma_array_destroy(
        player->memory,
        &player->hotspots,
        &__figma_player_hotspot_destroy);
    player->hotspots = previous;
    player->hotspots_dirty = previous_dirty;
    return result;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_copy_document_diagnostics(figma_player_t * player)
{
    figma_diagnostics_clear(&player->diagnostics);
    return figma_diagnostics_copy(
               &player->diagnostics, &player->document->diagnostics) ==
            FIGMA_TRUE
        ? FIGMA_RESULT_OK
        : FIGMA_RESULT_OUT_OF_MEMORY;
}

static figma_bool_t __figma_player_collect_smart_tracks(figma_player_t * player, const figma_canvas_node_t * source, const figma_canvas_node_t * target, figma_array_t * tracks);

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_begin_prototype_animation(figma_player_t * player, const figma_canvas_node_t * source_frame, const figma_canvas_node_t * target_frame, const figma_prototype_action_t * action, figma_string_view_t source_node_id, float initial_elapsed)
{
    const float duration = __figma_player_maxf(0.0f, action->transition_duration);
    figma_animation_clip_t * clip = &player->animation_state.clip;

    initial_elapsed = __figma_player_maxf(0.0f, initial_elapsed);
    if(duration <= 0.0001f || initial_elapsed >= duration)
    {
        __figma_player_set_current_frame(player, target_frame);
        figma_array_clear(
            player->memory, &player->node_swaps, &__figma_player_swap_destroy);
        figma_array_clear(
            player->memory,
            &player->local_animations,
            &__figma_player_local_animation_destroy);
        figma_array_clear(
            player->memory,
            &player->hovered_node_ids,
            &__figma_player_string_element_destroy);
        player->time = __figma_player_maxf(0.0f, initial_elapsed - duration);
        return FIGMA_RESULT_OK;
    }

    figma_player_animation_state_destroy(
        player->memory, &player->animation_state);
    figma_player_animation_state_init(&player->animation_state);
    player->animation_state.active = FIGMA_TRUE;
    player->animation_state.elapsed =
        __figma_player_minf(duration, initial_elapsed);
    player->animation_state.progress = __figma_player_clamp01(
        player->animation_state.elapsed /
        __figma_player_maxf(0.0001f, duration));
    clip->duration = duration;
    clip->smart_animate =
        action->smart_animate == FIGMA_TRUE ||
            action->transition_type == FIGMA_PROTOTYPE_TRANSITION_SMART_ANIMATE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
    clip->source = clip->smart_animate == FIGMA_TRUE
        ? FIGMA_ANIMATION_SOURCE_SMART_ANIMATE
        : FIGMA_ANIMATION_SOURCE_PROTOTYPE_TRANSITION;
    clip->transition_type = action->transition_type;
    clip->transition_direction = action->transition_direction;
    clip->easing = action->transition_easing;

    if(figma_string_copy(
           player->memory, &clip->source_frame_id, &source_frame->id) ==
            FIGMA_FALSE ||
        figma_string_copy(
            player->memory, &clip->target_frame_id, &target_frame->id) ==
            FIGMA_FALSE ||
        figma_string_assign(
            player->memory, &clip->source_node_id, source_node_id) ==
            FIGMA_FALSE ||
        figma_string_copy(player->memory, &clip->id, &source_frame->id) ==
            FIGMA_FALSE ||
        figma_string_append_cstr(player->memory, &clip->id, "->") ==
            FIGMA_FALSE ||
        figma_string_append(
            player->memory, &clip->id, figma_string_view(&target_frame->id)) ==
            FIGMA_FALSE)
    {
        figma_player_animation_state_destroy(
            player->memory, &player->animation_state);
        figma_player_animation_state_init(&player->animation_state);
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    if(clip->smart_animate == FIGMA_TRUE &&
        __figma_player_collect_smart_tracks(
            player, source_frame, target_frame, &clip->tracks) ==
            FIGMA_FALSE)
    {
        figma_player_animation_state_destroy(
            player->memory, &player->animation_state);
        figma_player_animation_state_init(&player->animation_state);
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    player->time = 0.0f;
    player->hotspots_dirty = FIGMA_TRUE;
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_smart_target_used(const figma_array_t * used_targets, const figma_canvas_node_t * target)
{
    size_t index;
    for(index = 0u; index != used_targets->size; ++index)
    {
        if(FIGMA_ARRAY_AT(
               const figma_canvas_node_t *, used_targets, index) == target)
        {
            return FIGMA_TRUE;
        }
    }
    return FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_smart_layer_match(const figma_canvas_node_t * source, const figma_canvas_node_t * target)
{
    return source->type == target->type &&
            source->name.size != 0u &&
            figma_string_equal(&source->name, &target->name) == FIGMA_TRUE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

typedef struct figma_player_dissolve_pair
{
    const figma_canvas_node_t * source;
    const figma_canvas_node_t * target;
} figma_player_dissolve_pair_t;

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_almost_equal(float left, float right, float tolerance)
{
    return fabsf(left - right) <= tolerance ? FIGMA_TRUE : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_vec2_equal(figma_vec2f_t left, figma_vec2f_t right, float tolerance)
{
    return __figma_player_almost_equal(left.x, right.x, tolerance) ==
            FIGMA_TRUE &&
            __figma_player_almost_equal(left.y, right.y, tolerance) ==
                FIGMA_TRUE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_rect_equal(figma_rectf_t left, figma_rectf_t right, float tolerance)
{
    return __figma_player_almost_equal(left.x, right.x, tolerance) ==
            FIGMA_TRUE &&
            __figma_player_almost_equal(left.y, right.y, tolerance) ==
                FIGMA_TRUE &&
            __figma_player_almost_equal(left.w, right.w, tolerance) ==
                FIGMA_TRUE &&
            __figma_player_almost_equal(left.h, right.h, tolerance) ==
                FIGMA_TRUE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_color_equal(figma_colorf_t left, figma_colorf_t right)
{
    return __figma_player_almost_equal(left.r, right.r, 0.0001f) ==
            FIGMA_TRUE &&
            __figma_player_almost_equal(left.g, right.g, 0.0001f) ==
                FIGMA_TRUE &&
            __figma_player_almost_equal(left.b, right.b, 0.0001f) ==
                FIGMA_TRUE &&
            __figma_player_almost_equal(left.a, right.a, 0.0001f) ==
                FIGMA_TRUE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_float_array_equal(const figma_array_t * left, const figma_array_t * right)
{
    size_t index;
    if(left->size != right->size)
    {
        return FIGMA_FALSE;
    }
    for(index = 0u; index != left->size; ++index)
    {
        if(__figma_player_almost_equal(
               FIGMA_ARRAY_AT(float, left, index),
               FIGMA_ARRAY_AT(float, right, index),
               0.0001f) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_paint_equal(const figma_canvas_paint_t * left, const figma_canvas_paint_t * right)
{
    size_t index;
    if(left->type != right->type ||
        left->blend_mode != right->blend_mode ||
        left->image_scale_mode != right->image_scale_mode ||
        left->visible != right->visible ||
        figma_string_equal(&left->asset_id, &right->asset_id) == FIGMA_FALSE ||
        left->has_transform != right->has_transform ||
        left->has_filter_color_adjust != right->has_filter_color_adjust ||
        left->has_paint_filter != right->has_paint_filter ||
        left->original_image_width != right->original_image_width ||
        left->original_image_height != right->original_image_height ||
        __figma_player_almost_equal(left->opacity, right->opacity, 0.0001f) ==
            FIGMA_FALSE ||
        __figma_player_color_equal(left->color, right->color) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }
    for(index = 0u; index != 6u; ++index)
    {
        if(__figma_player_almost_equal(
               left->transform[index], right->transform[index], 0.0001f) ==
            FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }
    for(index = 0u; index != 8u; ++index)
    {
        if(__figma_player_almost_equal(
               left->filter_color_adjust[index],
               right->filter_color_adjust[index],
               0.0001f) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }
    for(index = 0u; index != 10u; ++index)
    {
        if(__figma_player_almost_equal(
               left->paint_filter[index],
               right->paint_filter[index],
               0.0001f) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_paint_array_equal(const figma_array_t * left, const figma_array_t * right)
{
    size_t index;
    if(left->size != right->size)
    {
        return FIGMA_FALSE;
    }
    for(index = 0u; index != left->size; ++index)
    {
        if(__figma_player_paint_equal(
               FIGMA_ARRAY_CONST_PTR(figma_canvas_paint_t, left, index),
               FIGMA_ARRAY_CONST_PTR(figma_canvas_paint_t, right, index)) ==
            FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_path_command_equal(const figma_canvas_path_command_t * left, const figma_canvas_path_command_t * right)
{
    return left->type == right->type &&
            __figma_player_vec2_equal(left->p0, right->p0, 0.0001f) ==
                FIGMA_TRUE &&
            __figma_player_vec2_equal(left->p1, right->p1, 0.0001f) ==
                FIGMA_TRUE &&
            __figma_player_vec2_equal(left->p2, right->p2, 0.0001f) ==
                FIGMA_TRUE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_path_equal(const figma_canvas_path_t * left, const figma_canvas_path_t * right)
{
    size_t index;
    if(left->winding_rule != right->winding_rule ||
        left->commands_decoded != right->commands_decoded ||
        left->commands.size != right->commands.size ||
        __figma_player_paint_array_equal(&left->paints, &right->paints) ==
            FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }
    for(index = 0u; index != left->commands.size; ++index)
    {
        if(__figma_player_path_command_equal(
               FIGMA_ARRAY_CONST_PTR(
                   figma_canvas_path_command_t, &left->commands, index),
               FIGMA_ARRAY_CONST_PTR(
                   figma_canvas_path_command_t, &right->commands, index)) ==
            FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_path_array_equal(const figma_array_t * left, const figma_array_t * right)
{
    size_t index;
    if(left->size != right->size)
    {
        return FIGMA_FALSE;
    }
    for(index = 0u; index != left->size; ++index)
    {
        if(__figma_player_path_equal(
               FIGMA_ARRAY_CONST_PTR(figma_canvas_path_t, left, index),
               FIGMA_ARRAY_CONST_PTR(figma_canvas_path_t, right, index)) ==
            FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_text_lines_equal(const figma_array_t * left, const figma_array_t * right)
{
    size_t index;
    if(left->size != right->size)
    {
        return FIGMA_FALSE;
    }
    for(index = 0u; index != left->size; ++index)
    {
        const figma_canvas_text_line_t * left_line =
            FIGMA_ARRAY_CONST_PTR(figma_canvas_text_line_t, left, index);
        const figma_canvas_text_line_t * right_line =
            FIGMA_ARRAY_CONST_PTR(figma_canvas_text_line_t, right, index);
        if(figma_string_equal(&left_line->text, &right_line->text) ==
                FIGMA_FALSE ||
            __figma_player_almost_equal(
                left_line->x, right_line->x, 0.0001f) == FIGMA_FALSE ||
            __figma_player_almost_equal(
                left_line->y, right_line->y, 0.0001f) == FIGMA_FALSE ||
            __figma_player_almost_equal(
                left_line->width, right_line->width, 0.0001f) ==
                FIGMA_FALSE ||
            __figma_player_almost_equal(
                left_line->line_height,
                right_line->line_height,
                0.0001f) == FIGMA_FALSE ||
            __figma_player_almost_equal(
                left_line->line_ascent,
                right_line->line_ascent,
                0.0001f) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_persistent_leaf(const figma_canvas_node_t * source, const figma_canvas_node_t * target, figma_rectf_t source_frame_rect, figma_rectf_t target_frame_rect)
{
    figma_rectf_t source_rect;
    figma_rectf_t target_rect;
    const float source_offset_x =
        target_frame_rect.x - source_frame_rect.x;
    const float source_offset_y =
        target_frame_rect.y - source_frame_rect.y;
    size_t index;
    const figma_bool_t stable_id =
        source->id.size != 0u &&
            figma_string_equal(&source->id, &target->id) == FIGMA_TRUE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
    const figma_bool_t layer_name =
        source->name.size != 0u &&
            figma_string_equal(&source->name, &target->name) == FIGMA_TRUE
        ? FIGMA_TRUE
        : FIGMA_FALSE;

    if(source->children.size != 0u || target->children.size != 0u ||
        source->type != target->type ||
        (stable_id == FIGMA_FALSE && layer_name == FIGMA_FALSE) ||
        __figma_player_vec2_equal(source->size, target->size, 0.001f) ==
            FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    source_rect = (figma_rectf_t){
        source->rect.x - source_frame_rect.x,
        source->rect.y - source_frame_rect.y,
        source->rect.w,
        source->rect.h};
    target_rect = (figma_rectf_t){
        target->rect.x - target_frame_rect.x,
        target->rect.y - target_frame_rect.y,
        target->rect.w,
        target->rect.h};
    if(__figma_player_rect_equal(source_rect, target_rect, 0.001f) ==
        FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }
    for(index = 0u; index != 4u; ++index)
    {
        figma_vec2f_t source_quad = {
            source->quad[index].x + source_offset_x,
            source->quad[index].y + source_offset_y};
        if(__figma_player_vec2_equal(
               source_quad, target->quad[index], 0.001f) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    if(source->visible != target->visible ||
        source->mask != target->mask ||
        source->has_fill_geometry != target->has_fill_geometry ||
        source->has_stroke_geometry != target->has_stroke_geometry ||
        source->has_vector_data != target->has_vector_data ||
        source->has_vector_network_blob != target->has_vector_network_blob ||
        source->blend_mode != target->blend_mode ||
        source->stroke_align != target->stroke_align ||
        source->stroke_cap != target->stroke_cap ||
        source->stroke_join != target->stroke_join ||
        source->text_align_horizontal != target->text_align_horizontal ||
        source->text_align_vertical != target->text_align_vertical ||
        figma_string_equal(&source->text, &target->text) == FIGMA_FALSE ||
        figma_string_equal(&source->font_family, &target->font_family) ==
            FIGMA_FALSE ||
        figma_string_equal(&source->font_style, &target->font_style) ==
            FIGMA_FALSE ||
        figma_string_equal(
            &source->font_postscript_name,
            &target->font_postscript_name) == FIGMA_FALSE ||
        source->font_weight != target->font_weight ||
        __figma_player_almost_equal(
            source->opacity, target->opacity, 0.0001f) == FIGMA_FALSE ||
        __figma_player_almost_equal(
            source->corner_radius, target->corner_radius, 0.0001f) ==
            FIGMA_FALSE ||
        __figma_player_almost_equal(
            source->stroke_weight, target->stroke_weight, 0.0001f) ==
            FIGMA_FALSE ||
        __figma_player_almost_equal(
            source->font_size, target->font_size, 0.0001f) == FIGMA_FALSE ||
        __figma_player_almost_equal(
            source->line_height, target->line_height, 0.0001f) ==
            FIGMA_FALSE ||
        source->arc_data.valid != target->arc_data.valid ||
        __figma_player_almost_equal(
            source->arc_data.starting_angle,
            target->arc_data.starting_angle,
            0.0001f) == FIGMA_FALSE ||
        __figma_player_almost_equal(
            source->arc_data.ending_angle,
            target->arc_data.ending_angle,
            0.0001f) == FIGMA_FALSE ||
        __figma_player_almost_equal(
            source->arc_data.inner_radius,
            target->arc_data.inner_radius,
            0.0001f) == FIGMA_FALSE ||
        __figma_player_float_array_equal(
            &source->dash_pattern, &target->dash_pattern) == FIGMA_FALSE ||
        __figma_player_paint_array_equal(&source->fills, &target->fills) ==
            FIGMA_FALSE ||
        __figma_player_paint_array_equal(&source->strokes, &target->strokes) ==
            FIGMA_FALSE ||
        __figma_player_path_array_equal(
            &source->fill_geometry, &target->fill_geometry) == FIGMA_FALSE ||
        __figma_player_path_array_equal(
            &source->stroke_geometry, &target->stroke_geometry) ==
            FIGMA_FALSE ||
        __figma_player_text_lines_equal(
            &source->text_lines, &target->text_lines) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_dissolve_identity(const figma_canvas_node_t * source, const figma_canvas_node_t * target)
{
    if(source->symbol_id.size != 0u &&
        figma_string_equal(&source->symbol_id, &target->symbol_id) ==
            FIGMA_TRUE)
    {
        return FIGMA_TRUE;
    }
    return source->id.size != 0u &&
            figma_string_equal(&source->id, &target->id) == FIGMA_TRUE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_dissolve_match(const figma_canvas_node_t * source, const figma_canvas_node_t * target, figma_rectf_t source_frame_rect, figma_rectf_t target_frame_rect)
{
    const figma_rectf_t source_rect = {
        source->rect.x - source_frame_rect.x,
        source->rect.y - source_frame_rect.y,
        source->rect.w,
        source->rect.h};
    const figma_rectf_t target_rect = {
        target->rect.x - target_frame_rect.x,
        target->rect.y - target_frame_rect.y,
        target->rect.w,
        target->rect.h};
    if(__figma_player_persistent_leaf(
           source, target, source_frame_rect, target_frame_rect) == FIGMA_TRUE)
    {
        return FIGMA_TRUE;
    }
    return source->type == target->type &&
            __figma_player_dissolve_identity(source, target) == FIGMA_TRUE &&
            __figma_player_rect_equal(source_rect, target_rect, 1.0f) ==
                FIGMA_TRUE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_dissolve_target_blocked(const figma_canvas_node_t * node, const figma_array_t * used_targets)
{
    size_t index;
    for(index = 0u; index != used_targets->size; ++index)
    {
        const figma_canvas_node_t * used =
            FIGMA_ARRAY_AT(
                const figma_canvas_node_t *, used_targets, index);
        if(__figma_player_contains_node(used, node) == FIGMA_TRUE)
        {
            return FIGMA_TRUE;
        }
    }
    return FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static const figma_canvas_node_t * __figma_player_find_dissolve_target(const figma_canvas_node_t * source, const figma_canvas_node_t * target, figma_rectf_t source_frame_rect, figma_rectf_t target_frame_rect, const figma_array_t * used_targets)
{
    size_t index;
    if(__figma_player_dissolve_target_blocked(target, used_targets) ==
        FIGMA_TRUE)
    {
        return NULL;
    }
    if(__figma_player_dissolve_match(
           source, target, source_frame_rect, target_frame_rect) == FIGMA_TRUE)
    {
        return target;
    }
    for(index = 0u; index != target->children.size; ++index)
    {
        const figma_canvas_node_t * found =
            __figma_player_find_dissolve_target(
                source,
                FIGMA_ARRAY_CONST_PTR(
                    figma_canvas_node_t, &target->children, index),
                source_frame_rect,
                target_frame_rect,
                used_targets);
        if(found != NULL)
        {
            return found;
        }
    }
    return NULL;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_collect_dissolve_pairs(figma_player_t * player, const figma_canvas_node_t * source, const figma_canvas_node_t * target_frame, figma_rectf_t source_frame_rect, figma_rectf_t target_frame_rect, figma_array_t * used_targets, figma_array_t * pairs)
{
    size_t index;
    for(index = 0u; index != source->children.size; ++index)
    {
        const figma_canvas_node_t * source_child =
            FIGMA_ARRAY_CONST_PTR(
                figma_canvas_node_t, &source->children, index);
        const figma_canvas_node_t * target =
            __figma_player_find_dissolve_target(
                source_child,
                target_frame,
                source_frame_rect,
                target_frame_rect,
                used_targets);
        if(target != NULL)
        {
            const figma_player_dissolve_pair_t pair = {
                source_child, target};
            if(figma_array_push_copy(
                   player->memory, used_targets, &target) == FIGMA_FALSE ||
                figma_array_push_copy(player->memory, pairs, &pair) ==
                    FIGMA_FALSE)
            {
                return FIGMA_FALSE;
            }
            continue;
        }
        if(__figma_player_collect_dissolve_pairs(
               player,
               source_child,
               target_frame,
               source_frame_rect,
               target_frame_rect,
               used_targets,
               pairs) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static const figma_canvas_node_t * __figma_player_find_smart_child_match(const figma_canvas_node_t * source, const figma_canvas_node_t * target_parent, size_t source_index, const figma_array_t * used_targets)
{
    size_t index;
    if(source->id.size != 0u)
    {
        for(index = 0u; index != target_parent->children.size; ++index)
        {
            const figma_canvas_node_t * target = FIGMA_ARRAY_CONST_PTR(
                figma_canvas_node_t, &target_parent->children, index);
            if(figma_string_equal(&source->id, &target->id) == FIGMA_TRUE &&
                __figma_player_smart_target_used(used_targets, target) ==
                    FIGMA_FALSE)
            {
                return target;
            }
        }
    }

    if(source_index < target_parent->children.size)
    {
        const figma_canvas_node_t * target = FIGMA_ARRAY_CONST_PTR(
            figma_canvas_node_t, &target_parent->children, source_index);
        if(__figma_player_smart_target_used(used_targets, target) ==
                FIGMA_FALSE &&
            __figma_player_smart_layer_match(source, target) == FIGMA_TRUE)
        {
            return target;
        }
    }

    for(index = 0u; index != target_parent->children.size; ++index)
    {
        const figma_canvas_node_t * target = FIGMA_ARRAY_CONST_PTR(
            figma_canvas_node_t, &target_parent->children, index);
        if(__figma_player_smart_target_used(used_targets, target) ==
                FIGMA_FALSE &&
            __figma_player_smart_layer_match(source, target) == FIGMA_TRUE)
        {
            return target;
        }
    }
    return NULL;
}

//////////////////////////////////////////////////////////////////////////
static figma_animation_track_t * __figma_player_add_animation_track(figma_player_t * player, figma_array_t * tracks, const figma_canvas_node_t * source, const figma_canvas_node_t * target, figma_animation_track_type_t type)
{
    figma_animation_track_t * track =
        (figma_animation_track_t *)figma_array_push_uninitialized(
            player->memory, tracks);
    if(track == NULL)
    {
        return NULL;
    }

    figma_animation_track_init(track);
    track->type = type;
    if(figma_string_copy(player->memory, &track->node_id, &source->id) ==
            FIGMA_FALSE ||
        figma_string_copy(
            player->memory, &track->target_node_id, &target->id) ==
            FIGMA_FALSE)
    {
        figma_array_pop(
            player->memory, tracks, &figma_animation_track_destroy);
        return NULL;
    }
    return track;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_collect_smart_tracks_for_pair(figma_player_t * player, const figma_canvas_node_t * source, const figma_canvas_node_t * target, figma_rectf_t source_frame_rect, figma_rectf_t target_frame_rect, figma_array_t * tracks)
{
    figma_array_t used_targets;
    size_t source_index;

    if(source->id.size != 0u && target->id.size != 0u)
    {
        const figma_bool_t persistent = __figma_player_persistent_leaf(
            source, target, source_frame_rect, target_frame_rect);
        figma_animation_track_t * rect =
            __figma_player_add_animation_track(
                player,
                tracks,
                source,
                target,
                FIGMA_ANIMATION_TRACK_RECT);
        figma_animation_track_t * opacity;
        size_t index;
        const float source_offset_x =
            target_frame_rect.x - source_frame_rect.x;
        const float source_offset_y =
            target_frame_rect.y - source_frame_rect.y;
        if(rect == NULL)
        {
            return FIGMA_FALSE;
        }

        rect->from[0] =
            target_frame_rect.x + source->rect.x - source_frame_rect.x;
        rect->from[1] =
            target_frame_rect.y + source->rect.y - source_frame_rect.y;
        rect->from[2] = source->rect.w;
        rect->from[3] = source->rect.h;
        rect->to[0] = target->rect.x;
        rect->to[1] = target->rect.y;
        rect->to[2] = target->rect.w;
        rect->to[3] = target->rect.h;
        for(index = 0u; index != 4u; ++index)
        {
            rect->from_quad[index].x =
                source->quad[index].x + source_offset_x;
            rect->from_quad[index].y =
                source->quad[index].y + source_offset_y;
            rect->to_quad[index] = target->quad[index];
        }
        rect->has_quad = FIGMA_TRUE;
        rect->persistent = persistent;
        rect->persistent_source_node =
            persistent == FIGMA_TRUE ? source : NULL;

        opacity = __figma_player_add_animation_track(
            player,
            tracks,
            source,
            target,
            FIGMA_ANIMATION_TRACK_OPACITY);
        if(opacity == NULL)
        {
            return FIGMA_FALSE;
        }
        opacity->from[0] = source->opacity;
        opacity->to[0] = target->opacity;
        opacity->persistent = persistent;
        opacity->persistent_source_node =
            persistent == FIGMA_TRUE ? source : NULL;

        if(source->arc_data.valid == FIGMA_TRUE &&
            target->arc_data.valid == FIGMA_TRUE)
        {
            figma_animation_track_t * arc =
                __figma_player_add_animation_track(
                    player,
                    tracks,
                    source,
                    target,
                    FIGMA_ANIMATION_TRACK_ARC);
            if(arc == NULL)
            {
                return FIGMA_FALSE;
            }
            arc->from[0] = source->arc_data.starting_angle;
            arc->from[1] = source->arc_data.ending_angle;
            arc->from[2] = source->arc_data.inner_radius;
            arc->to[0] = target->arc_data.starting_angle;
            arc->to[1] = target->arc_data.ending_angle;
            arc->to[2] = target->arc_data.inner_radius;
            arc->persistent = persistent;
            arc->persistent_source_node =
                persistent == FIGMA_TRUE ? source : NULL;
        }
    }

    figma_array_init(
        &used_targets, sizeof(const figma_canvas_node_t *));
    for(source_index = 0u;
        source_index != source->children.size;
        ++source_index)
    {
        const figma_canvas_node_t * source_child =
            FIGMA_ARRAY_CONST_PTR(
                figma_canvas_node_t, &source->children, source_index);
        const figma_canvas_node_t * target_child =
            __figma_player_find_smart_child_match(
                source_child, target, source_index, &used_targets);
        if(target_child == NULL)
        {
            continue;
        }
        if(figma_array_push_copy(
               player->memory, &used_targets, &target_child) == FIGMA_FALSE ||
            __figma_player_collect_smart_tracks_for_pair(
                player,
                source_child,
                target_child,
                source_frame_rect,
                target_frame_rect,
                tracks) == FIGMA_FALSE)
        {
            figma_array_destroy(player->memory, &used_targets, NULL);
            return FIGMA_FALSE;
        }
    }
    figma_array_destroy(player->memory, &used_targets, NULL);
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_collect_smart_tracks(figma_player_t * player, const figma_canvas_node_t * source, const figma_canvas_node_t * target, figma_array_t * tracks)
{
    figma_array_t used_targets;
    size_t source_index;
    figma_array_init(
        &used_targets, sizeof(const figma_canvas_node_t *));

    for(source_index = 0u;
        source_index != source->children.size;
        ++source_index)
    {
        const figma_canvas_node_t * source_child =
            FIGMA_ARRAY_CONST_PTR(
                figma_canvas_node_t, &source->children, source_index);
        const figma_canvas_node_t * target_child =
            __figma_player_find_smart_child_match(
                source_child, target, source_index, &used_targets);
        if(target_child == NULL)
        {
            continue;
        }
        if(figma_array_push_copy(
               player->memory, &used_targets, &target_child) == FIGMA_FALSE ||
            __figma_player_collect_smart_tracks_for_pair(
                player,
                source_child,
                target_child,
                source->rect,
                target->rect,
                tracks) == FIGMA_FALSE)
        {
            figma_array_destroy(player->memory, &used_targets, NULL);
            return FIGMA_FALSE;
        }
    }

    figma_array_destroy(player->memory, &used_targets, NULL);
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_begin_local_animation(figma_player_t * player, figma_string_view_t source_node_id, figma_string_view_t from_node_id, const figma_canvas_node_t * target_node, const figma_prototype_action_t * action, float initial_elapsed)
{
    const figma_canvas_node_t * from_node =
        figma_document_find_canvas_node(player->document, from_node_id);
    figma_player_local_animation_t * animation;
    size_t index;

    if(from_node == NULL)
    {
        from_node =
            figma_document_find_canvas_node(player->document, source_node_id);
    }

    if(from_node == NULL)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }

    index = 0u;
    while(index != player->local_animations.size)
    {
        figma_player_local_animation_t * existing =
            FIGMA_ARRAY_PTR(
                figma_player_local_animation_t,
                &player->local_animations,
                index);
        if(figma_string_equal_view(
               &existing->source_node_id, source_node_id) == FIGMA_TRUE)
        {
            figma_array_remove(
                player->memory,
                &player->local_animations,
                index,
                &__figma_player_local_animation_destroy);
            continue;
        }
        ++index;
    }

    animation =
        (figma_player_local_animation_t *)figma_array_push_uninitialized(
            player->memory, &player->local_animations);
    if(animation == NULL)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    memset(animation, 0, sizeof(*animation));
    figma_string_init(&animation->source_node_id);
    figma_string_init(&animation->from_node_id);
    figma_string_init(&animation->target_node_id);
    figma_array_init(&animation->tracks, sizeof(figma_animation_track_t));
    animation->transition_type = action->transition_type;
    animation->easing = action->transition_easing;
    animation->smart_animate =
        action->smart_animate == FIGMA_TRUE ||
            action->transition_type == FIGMA_PROTOTYPE_TRANSITION_SMART_ANIMATE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
    animation->duration =
        __figma_player_maxf(0.0f, action->transition_duration);
    animation->elapsed = __figma_player_maxf(
        0.0f, __figma_player_minf(animation->duration, initial_elapsed));
    animation->progress = __figma_player_clamp01(
        animation->elapsed / __figma_player_maxf(0.0001f, animation->duration));
    animation->active = FIGMA_TRUE;

    if(figma_string_assign(
           player->memory, &animation->source_node_id, source_node_id) ==
            FIGMA_FALSE ||
        figma_string_copy(
            player->memory, &animation->from_node_id, &from_node->id) ==
            FIGMA_FALSE ||
        figma_string_copy(
            player->memory, &animation->target_node_id, &target_node->id) ==
            FIGMA_FALSE)
    {
        figma_array_pop(
            player->memory,
            &player->local_animations,
            &__figma_player_local_animation_destroy);
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    if(animation->smart_animate == FIGMA_TRUE &&
        __figma_player_collect_smart_tracks(
            player, from_node, target_node, &animation->tracks) ==
            FIGMA_FALSE)
    {
        figma_array_pop(
            player->memory,
            &player->local_animations,
            &__figma_player_local_animation_destroy);
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_swap_state(figma_player_t * player, figma_string_view_t source_node_id, figma_string_view_t from_node_id, const figma_prototype_action_t * action, float initial_elapsed)
{
    const figma_canvas_node_t * target;
    figma_player_node_swap_t * swap;
    figma_string_view_t previous_state = from_node_id;
    const float duration =
        __figma_player_maxf(0.0f, action->transition_duration);

    if(source_node_id.size == 0u || action->target_node_id.size == 0u)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    target = figma_document_find_canvas_node(
        player->document, figma_string_view(&action->target_node_id));
    if(target == NULL)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }

    initial_elapsed = __figma_player_maxf(0.0f, initial_elapsed);
    if(duration > 0.0001f &&
        action->transition_easing != FIGMA_ANIMATION_EASING_UNSUPPORTED &&
        initial_elapsed < duration)
    {
        return __figma_player_begin_local_animation(
            player,
            source_node_id,
            from_node_id,
            target,
            action,
            initial_elapsed);
    }

    swap = __figma_player_find_swap(player, source_node_id);
    if(swap == NULL)
    {
        swap = (figma_player_node_swap_t *)figma_array_push_uninitialized(
            player->memory, &player->node_swaps);
        if(swap == NULL)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }

        memset(swap, 0, sizeof(*swap));
        figma_string_init(&swap->source_node_id);
        figma_string_init(&swap->current_node_id);
        if(figma_string_assign(
               player->memory, &swap->source_node_id, source_node_id) ==
            FIGMA_FALSE)
        {
            figma_array_pop(
                player->memory, &player->node_swaps, &__figma_player_swap_destroy);
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }
    }
    else
    {
        previous_state = figma_string_view(&swap->current_node_id);
    }

    if(figma_string_copy(
           player->memory, &swap->current_node_id, &target->id) == FIGMA_FALSE)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }
    swap->started_at = player->time -
        __figma_player_maxf(0.0f, initial_elapsed - duration);
    player->hotspots_dirty = FIGMA_TRUE;

    if(player->action_router.on_state_changed != NULL)
    {
        player->action_router.on_state_changed(
            player->action_router.user_data,
            source_node_id,
            previous_state,
            figma_string_view(&target->id));
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_navigate_internal(figma_player_t * player, figma_string_view_t target_frame_id, const figma_prototype_action_t * action, figma_string_view_t source_node_id, float initial_elapsed)
{
    const figma_canvas_node_t * target;
    const figma_canvas_node_t * source;

    if(target_frame_id.size == 0u)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    if(action != NULL &&
        action->navigation_type == FIGMA_PROTOTYPE_NAVIGATION_SWAP)
    {
        return __figma_player_swap_state(
            player, source_node_id, source_node_id, action, 0.0f);
    }

    target =
        figma_document_find_canvas_node(player->document, target_frame_id);
    if(target == NULL)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }

    source = player->current_frame;
    if(source != NULL && source != target &&
        figma_array_push_copy(
            player->memory, &player->navigation_history, &source) == FIGMA_FALSE)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    while(player->overlay_frames.size != 0u)
    {
        const figma_canvas_node_t * overlay =
            FIGMA_ARRAY_AT(
                const figma_canvas_node_t *,
                &player->overlay_frames,
                player->overlay_frames.size - 1u);
        if(player->action_router.on_overlay_closed != NULL)
        {
            player->action_router.on_overlay_closed(
                player->action_router.user_data,
                figma_string_view(&overlay->id));
        }
        figma_array_pop(player->memory, &player->overlay_frames, NULL);
        figma_array_pop(player->memory, &player->overlay_start_times, NULL);
    }

    if(source != NULL && action != NULL &&
        (action->smart_animate == FIGMA_TRUE ||
            action->transition_type ==
                FIGMA_PROTOTYPE_TRANSITION_SMART_ANIMATE ||
            action->transition_duration > 0.0f))
    {
        return __figma_player_begin_prototype_animation(
            player,
            source,
            target,
            action,
            source_node_id,
            initial_elapsed);
    }

    __figma_player_set_current_frame(player, target);
    figma_array_clear(
        player->memory, &player->node_swaps, &__figma_player_swap_destroy);
    figma_array_clear(
        player->memory,
        &player->local_animations,
        &__figma_player_local_animation_destroy);
    figma_array_clear(
        player->memory,
        &player->hovered_node_ids,
        &__figma_player_string_element_destroy);
    figma_array_clear(
        player->memory,
        &player->pointer_captures,
        &__figma_player_capture_destroy);
    player->time = 0.0f;

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_complete_animation(figma_player_t * player)
{
    const figma_canvas_node_t * target;

    if(player->animation_state.active == FIGMA_FALSE)
    {
        return;
    }

    target = figma_document_find_canvas_node(
        player->document,
        figma_string_view(&player->animation_state.clip.target_frame_id));
    __figma_player_set_current_frame(player, target);
    figma_player_animation_state_destroy(
        player->memory, &player->animation_state);
    figma_player_animation_state_init(&player->animation_state);
    figma_array_clear(
        player->memory, &player->node_swaps, &__figma_player_swap_destroy);
    figma_array_clear(
        player->memory,
        &player->local_animations,
        &__figma_player_local_animation_destroy);
    figma_array_clear(
        player->memory,
        &player->hovered_node_ids,
        &__figma_player_string_element_destroy);
    player->time = 0.0f;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_complete_local_animation(figma_player_t * player, figma_player_local_animation_t * animation)
{
    figma_player_node_swap_t * swap;
    figma_string_view_t previous_state;

    if(animation == NULL || animation->active == FIGMA_FALSE)
    {
        return FIGMA_RESULT_OK;
    }

    swap = __figma_player_find_swap(
        player, figma_string_view(&animation->source_node_id));
    if(swap == NULL)
    {
        swap = (figma_player_node_swap_t *)figma_array_push_uninitialized(
            player->memory, &player->node_swaps);
        if(swap == NULL)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }
        memset(swap, 0, sizeof(*swap));
        figma_string_init(&swap->source_node_id);
        figma_string_init(&swap->current_node_id);
        if(figma_string_copy(
               player->memory,
               &swap->source_node_id,
               &animation->source_node_id) == FIGMA_FALSE)
        {
            figma_array_pop(
                player->memory, &player->node_swaps, &__figma_player_swap_destroy);
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }
    }

    previous_state = figma_string_view(&animation->from_node_id);
    if(figma_string_copy(
           player->memory,
           &swap->current_node_id,
           &animation->target_node_id) == FIGMA_FALSE)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }
    swap->started_at = player->time;
    animation->active = FIGMA_FALSE;
    player->hotspots_dirty = FIGMA_TRUE;

    if(player->action_router.on_state_changed != NULL)
    {
        player->action_router.on_state_changed(
            player->action_router.user_data,
            figma_string_view(&animation->source_node_id),
            previous_state,
            figma_string_view(&animation->target_node_id));
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_update_animations(figma_player_t * player, float dt)
{
    size_t index;

    for(index = 0u; index != player->local_animations.size; ++index)
    {
        figma_player_local_animation_t * animation =
            FIGMA_ARRAY_PTR(
                figma_player_local_animation_t,
                &player->local_animations,
                index);
        if(animation->active == FIGMA_FALSE)
        {
            continue;
        }
        animation->elapsed += __figma_player_maxf(0.0f, dt);
        animation->progress = __figma_player_clamp01(
            animation->elapsed /
            __figma_player_maxf(0.0001f, animation->duration));
        if(animation->progress >= 1.0f)
        {
            figma_result_t result =
                __figma_player_complete_local_animation(player, animation);
            if(result != FIGMA_RESULT_OK)
            {
                return result;
            }
        }
    }

    index = 0u;
    while(index != player->local_animations.size)
    {
        const figma_player_local_animation_t * animation =
            FIGMA_ARRAY_CONST_PTR(
                figma_player_local_animation_t,
                &player->local_animations,
                index);
        if(animation->active == FIGMA_FALSE)
        {
            figma_array_remove(
                player->memory,
                &player->local_animations,
                index,
                &__figma_player_local_animation_destroy);
            continue;
        }
        ++index;
    }

    if(player->animation_state.active == FIGMA_TRUE)
    {
        player->animation_state.elapsed += __figma_player_maxf(0.0f, dt);
        player->animation_state.progress = __figma_player_clamp01(
            player->animation_state.elapsed /
            __figma_player_maxf(
                0.0001f, player->animation_state.clip.duration));
        if(player->animation_state.progress >= 1.0f)
        {
            __figma_player_complete_animation(player);
        }
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_update_timers_for_node(figma_player_t * player, const figma_canvas_node_t * node, figma_string_view_t source_node_id, float started_at, float dt, figma_bool_t * fired)
{
    const figma_canvas_node_t * current_node = node;
    const figma_player_node_swap_t * swap;
    size_t index;

    if(__figma_player_find_local_animation(player, source_node_id) != NULL)
    {
        return FIGMA_RESULT_OK;
    }

    swap = __figma_player_find_swap(player, source_node_id);
    if(swap != NULL)
    {
        current_node = figma_document_find_canvas_node(
            player->document, figma_string_view(&swap->current_node_id));
        if(current_node == NULL)
        {
            return FIGMA_RESULT_OK;
        }
        started_at = swap->started_at;
    }

    for(index = 0u; index != current_node->prototype_interactions.size; ++index)
    {
        const figma_prototype_interaction_t * interaction =
            FIGMA_ARRAY_CONST_PTR(
                figma_prototype_interaction_t,
                &current_node->prototype_interactions,
                index);
        const float timeout =
            __figma_player_maxf(0.0f, interaction->transition_timeout);
        figma_string_t timer_id;
        figma_player_hotspot_t hotspot;
        figma_result_t result;

        if(interaction->event_type != FIGMA_PROTOTYPE_EVENT_AFTER_TIMEOUT ||
            player->time - started_at < timeout)
        {
            continue;
        }

        figma_string_init(&timer_id);
        if(figma_string_assign(
               player->memory, &timer_id, source_node_id) == FIGMA_FALSE ||
            figma_string_append_cstr(player->memory, &timer_id, ":") ==
                FIGMA_FALSE ||
            figma_string_append(
                player->memory,
                &timer_id,
                figma_string_view(&interaction->id)) == FIGMA_FALSE)
        {
            figma_string_destroy(player->memory, &timer_id);
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }

        if(__figma_player_string_array_contains(
               &player->fired_timer_ids, figma_string_view(&timer_id)) ==
            FIGMA_TRUE)
        {
            figma_string_destroy(player->memory, &timer_id);
            continue;
        }

        if(__figma_player_string_array_add(
               player,
               &player->fired_timer_ids,
               figma_string_view(&timer_id)) == FIGMA_FALSE)
        {
            figma_string_destroy(player->memory, &timer_id);
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }
        figma_string_destroy(player->memory, &timer_id);

        __figma_player_hotspot_init(&hotspot);
        hotspot.interaction = interaction;
        hotspot.event_type = FIGMA_PROTOTYPE_EVENT_AFTER_TIMEOUT;
        if(figma_string_assign(
               player->memory, &hotspot.node_id, source_node_id) ==
            FIGMA_FALSE)
        {
            __figma_player_hotspot_destroy(player->memory, &hotspot);
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }

        {
            const float trigger_time = started_at + timeout;
            const float previous_time = __figma_player_maxf(
                0.0f, player->time - __figma_player_maxf(0.0f, dt));
            const float initial_elapsed =
                previous_time < trigger_time
                ? player->time - trigger_time
                : 0.0f;
            result = __figma_player_route_hotspot(
                player,
                &hotspot,
                FIGMA_ACTION_INPUT_TIMER,
                NULL,
                NULL,
                initial_elapsed);
        }
        __figma_player_hotspot_destroy(player->memory, &hotspot);
        *fired = FIGMA_TRUE;
        return result;
    }

    for(index = 0u; index != current_node->children.size; ++index)
    {
        const figma_canvas_node_t * child =
            FIGMA_ARRAY_CONST_PTR(
                figma_canvas_node_t, &current_node->children, index);
        figma_result_t result = __figma_player_update_timers_for_node(
            player,
            child,
            figma_string_view(&child->id),
            started_at,
            dt,
            fired);
        if(result != FIGMA_RESULT_OK || *fired == FIGMA_TRUE)
        {
            return result;
        }
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_update_timers(figma_player_t * player, float dt)
{
    const figma_canvas_node_t * frame;
    float frame_started_at;
    size_t index;

    if(player->overlay_frames.size != 0u)
    {
        frame = FIGMA_ARRAY_AT(
            const figma_canvas_node_t *,
            &player->overlay_frames,
            player->overlay_frames.size - 1u);
        frame_started_at = FIGMA_ARRAY_AT(
            float,
            &player->overlay_start_times,
            player->overlay_start_times.size - 1u);
    }
    else
    {
        frame = player->current_frame;
        frame_started_at = 0.0f;
    }

    if(frame == NULL)
    {
        return FIGMA_RESULT_OK;
    }

    for(index = 0u; index != frame->prototype_interactions.size; ++index)
    {
        const figma_prototype_interaction_t * interaction =
            FIGMA_ARRAY_CONST_PTR(
                figma_prototype_interaction_t,
                &frame->prototype_interactions,
                index);
        const float timeout =
            __figma_player_maxf(0.0f, interaction->transition_timeout);
        figma_string_t timer_id;
        figma_player_hotspot_t hotspot;
        figma_result_t result;

        if(interaction->event_type != FIGMA_PROTOTYPE_EVENT_AFTER_TIMEOUT ||
            player->time - frame_started_at < timeout)
        {
            continue;
        }

        figma_string_init(&timer_id);
        if(figma_string_copy(player->memory, &timer_id, &frame->id) ==
                FIGMA_FALSE ||
            figma_string_append_cstr(player->memory, &timer_id, ":") ==
                FIGMA_FALSE ||
            figma_string_append(
                player->memory,
                &timer_id,
                figma_string_view(&interaction->id)) == FIGMA_FALSE)
        {
            figma_string_destroy(player->memory, &timer_id);
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }

        if(__figma_player_string_array_contains(
               &player->fired_timer_ids, figma_string_view(&timer_id)) ==
            FIGMA_TRUE)
        {
            figma_string_destroy(player->memory, &timer_id);
            continue;
        }
        if(__figma_player_string_array_add(
               player,
               &player->fired_timer_ids,
               figma_string_view(&timer_id)) == FIGMA_FALSE)
        {
            figma_string_destroy(player->memory, &timer_id);
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }
        figma_string_destroy(player->memory, &timer_id);

        __figma_player_hotspot_init(&hotspot);
        hotspot.interaction = interaction;
        hotspot.event_type = FIGMA_PROTOTYPE_EVENT_AFTER_TIMEOUT;
        if(figma_string_copy(player->memory, &hotspot.node_id, &frame->id) ==
            FIGMA_FALSE)
        {
            __figma_player_hotspot_destroy(player->memory, &hotspot);
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }
        {
            const float trigger_time = frame_started_at + timeout;
            const float previous_time = __figma_player_maxf(
                0.0f, player->time - __figma_player_maxf(0.0f, dt));
            const float initial_elapsed =
                previous_time < trigger_time
                ? player->time - trigger_time
                : 0.0f;
            result = __figma_player_route_hotspot(
                player,
                &hotspot,
                FIGMA_ACTION_INPUT_TIMER,
                NULL,
                NULL,
                initial_elapsed);
        }
        __figma_player_hotspot_destroy(player->memory, &hotspot);
        return result;
    }

    for(index = 0u; index != frame->children.size; ++index)
    {
        const figma_canvas_node_t * child =
            FIGMA_ARRAY_CONST_PTR(
                figma_canvas_node_t, &frame->children, index);
        figma_bool_t fired = FIGMA_FALSE;
        figma_result_t result = __figma_player_update_timers_for_node(
            player,
            child,
            figma_string_view(&child->id),
            frame_started_at,
            dt,
            &fired);
        if(result != FIGMA_RESULT_OK || fired == FIGMA_TRUE)
        {
            return result;
        }
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_deinit(figma_player_t * player)
{
    figma_player_animation_state_destroy(
        player->memory, &player->animation_state);
    figma_render_list_destroy(&player->render_list);
    figma_diagnostics_destroy(&player->diagnostics);
    figma_array_destroy(
        player->memory, &player->hotspots, &__figma_player_hotspot_destroy);
    figma_array_destroy(
        player->memory,
        &player->hovered_node_ids,
        &__figma_player_string_element_destroy);
    figma_array_destroy(
        player->memory,
        &player->fired_timer_ids,
        &__figma_player_string_element_destroy);
    figma_array_destroy(
        player->memory, &player->node_swaps, &__figma_player_swap_destroy);
    figma_array_destroy(
        player->memory,
        &player->local_animations,
        &__figma_player_local_animation_destroy);
    figma_array_destroy(
        player->memory, &player->overrides, &__figma_player_override_destroy);
    figma_array_destroy(
        player->memory,
        &player->pointer_captures,
        &__figma_player_capture_destroy);
    figma_array_destroy(player->memory, &player->navigation_history, NULL);
    figma_array_destroy(player->memory, &player->overlay_frames, NULL);
    figma_array_destroy(player->memory, &player->overlay_start_times, NULL);
    figma_string_destroy(player->memory, &player->start_frame_id);
    figma_string_destroy(player->memory, &player->current_frame_id);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_runtime_create_player(figma_runtime_t * runtime, figma_document_t * document, const figma_player_desc_t * desc, figma_player_t ** output)
{
    figma_player_t * player;
    figma_result_t result;

    if(output == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    *output = NULL;

    if(runtime == NULL || document == NULL || desc == NULL ||
        document->runtime != runtime)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    if(desc->start_frame_id.size != 0u &&
        (desc->start_frame_id.data == NULL ||
            figma_document_find_canvas_node(
                document, desc->start_frame_id) == NULL))
    {
        return FIGMA_RESULT_NOT_FOUND;
    }

    player =
        (figma_player_t *)figma_memory_allocate(&runtime->memory, sizeof(*player));
    if(player == NULL)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    memset(player, 0, sizeof(*player));
    player->runtime = runtime;
    player->document = document;
    player->memory = &runtime->memory;
    player->desc = *desc;
    figma_string_init(&player->start_frame_id);
    figma_string_init(&player->current_frame_id);
    figma_render_list_init(&player->render_list, player->memory);
    figma_diagnostics_init(&player->diagnostics, player->memory);
    figma_array_init(&player->hotspots, sizeof(figma_player_hotspot_t));
    figma_array_init(&player->hovered_node_ids, sizeof(figma_string_t));
    figma_array_init(&player->fired_timer_ids, sizeof(figma_string_t));
    figma_array_init(&player->node_swaps, sizeof(figma_player_node_swap_t));
    figma_array_init(
        &player->local_animations, sizeof(figma_player_local_animation_t));
    figma_array_init(&player->overrides, sizeof(figma_binding_override_t));
    figma_array_init(
        &player->pointer_captures, sizeof(figma_player_pointer_capture_t));
    figma_array_init(
        &player->navigation_history, sizeof(const figma_canvas_node_t *));
    figma_array_init(
        &player->overlay_frames, sizeof(const figma_canvas_node_t *));
    figma_array_init(&player->overlay_start_times, sizeof(float));
    figma_player_animation_state_init(&player->animation_state);
    player->hotspots_dirty = FIGMA_TRUE;

    if(desc->start_frame_id.size != 0u &&
        figma_string_assign(
            player->memory, &player->start_frame_id, desc->start_frame_id) ==
            FIGMA_FALSE)
    {
        __figma_player_deinit(player);
        figma_memory_deallocate(player->memory, player);
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }
    player->desc.start_frame_id = figma_string_view(&player->start_frame_id);

    __figma_player_set_current_frame(
        player, __figma_player_resolve_initial_frame(player));
    result = figma_player_update(player, 0.0f);
    if(result != FIGMA_RESULT_OK)
    {
        __figma_player_deinit(player);
        figma_memory_deallocate(player->memory, player);
        return result;
    }

    *output = player;
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
void FIGMA_CALL figma_player_destroy(figma_player_t * player)
{
    figma_memory_t * memory;

    if(player == NULL)
    {
        return;
    }

    memory = player->memory;
    __figma_player_deinit(player);
    figma_memory_deallocate(memory, player);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_set_action_router(figma_player_t * player, const figma_action_router_t * router)
{
    if(player == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    if(router != NULL)
    {
        player->action_router = *router;
    }
    else
    {
        memset(&player->action_router, 0, sizeof(player->action_router));
    }
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_set_data_context(figma_player_t * player, const figma_data_context_t * context)
{
    if(player == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    if(context != NULL)
    {
        player->data_context = *context;
    }
    else
    {
        memset(&player->data_context, 0, sizeof(player->data_context));
    }
    player->hotspots_dirty = FIGMA_TRUE;
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_set_viewport(figma_player_t * player, const figma_viewport_desc_t * viewport)
{
    figma_result_t result;

    if(player == NULL || viewport == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    player->desc.viewport = *viewport;
    player->hotspots_dirty = FIGMA_TRUE;
    result = figma_player_rebuild_hotspots(player);
    if(result != FIGMA_RESULT_OK)
    {
        return result;
    }
    return figma_player_rebuild_render_list(player);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_hit_test(const figma_player_t * player, float x, float y, figma_bool_t * hit)
{
    size_t index;

    if(player == NULL || hit == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    *hit = FIGMA_FALSE;
    if(player->pointer_captures.size != 0u)
    {
        *hit = FIGMA_TRUE;
        return FIGMA_RESULT_OK;
    }

    index = player->hotspots.size;
    while(index != 0u)
    {
        --index;
        if(__figma_player_contains_hotspot(
               FIGMA_ARRAY_CONST_PTR(
                   figma_player_hotspot_t, &player->hotspots, index),
               x,
               y) == FIGMA_TRUE)
        {
            *hit = FIGMA_TRUE;
            break;
        }
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_input_pointer(figma_player_t * player, const figma_pointer_event_t * event, figma_input_dispatch_result_t * output_dispatch)
{
    figma_input_dispatch_result_t dispatch;
    figma_player_pointer_capture_t * capture;
    size_t capture_index = 0u;
    figma_result_t result = FIGMA_RESULT_OK;

    if(player == NULL || event == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    memset(&dispatch, 0, sizeof(dispatch));
    figma_player_hit_test(player, event->x, event->y, &dispatch.hit);
    capture =
        __figma_player_find_capture(player, event->pointer_id, &capture_index);
    dispatch.captured = capture != NULL ? FIGMA_TRUE : FIGMA_FALSE;

    if(player->animation_state.active == FIGMA_TRUE)
    {
        if(output_dispatch != NULL)
        {
            *output_dispatch = dispatch;
        }
        return FIGMA_RESULT_OK;
    }

    if(event->type == FIGMA_POINTER_EVENT_MOVE)
    {
        const figma_player_hotspot_t * hotspot = __figma_player_find_hotspot(
            player, event->x, event->y, FIGMA_PROTOTYPE_EVENT_HOVER_ENTER);
        figma_array_t hovered_now;
        size_t index;

        figma_array_init(&hovered_now, sizeof(figma_string_t));
        if(hotspot != NULL)
        {
            if(__figma_player_string_array_add(
                   player,
                   &hovered_now,
                   figma_string_view(&hotspot->node_id)) == FIGMA_FALSE)
            {
                figma_array_destroy(
                    player->memory,
                    &hovered_now,
                    &__figma_player_string_element_destroy);
                return FIGMA_RESULT_OUT_OF_MEMORY;
            }
            if(__figma_player_string_array_contains(
                   &player->hovered_node_ids,
                   figma_string_view(&hotspot->node_id)) == FIGMA_FALSE)
            {
                result = __figma_player_route_hotspot(
                    player,
                    hotspot,
                    FIGMA_ACTION_INPUT_POINTER,
                    event,
                    NULL,
                    0.0f);
                dispatch.handled =
                    result == FIGMA_RESULT_OK ? FIGMA_TRUE : FIGMA_FALSE;
            }
        }

        for(index = 0u; index != player->hovered_node_ids.size; ++index)
        {
            const figma_string_t * node_id = FIGMA_ARRAY_CONST_PTR(
                figma_string_t, &player->hovered_node_ids, index);
            size_t hotspot_index = player->hotspots.size;
            if(__figma_player_string_array_contains(
                   &hovered_now, figma_string_view(node_id)) == FIGMA_TRUE)
            {
                continue;
            }
            while(hotspot_index != 0u)
            {
                const figma_player_hotspot_t * leave;
                --hotspot_index;
                leave = FIGMA_ARRAY_CONST_PTR(
                    figma_player_hotspot_t,
                    &player->hotspots,
                    hotspot_index);
                if(leave->event_type == FIGMA_PROTOTYPE_EVENT_HOVER_LEAVE &&
                    figma_string_equal(&leave->node_id, node_id) == FIGMA_TRUE)
                {
                    result = __figma_player_route_hotspot(
                        player,
                        leave,
                        FIGMA_ACTION_INPUT_POINTER,
                        event,
                        NULL,
                        0.0f);
                    dispatch.handled =
                        result == FIGMA_RESULT_OK ? FIGMA_TRUE : FIGMA_FALSE;
                    break;
                }
            }
        }

        figma_array_destroy(
            player->memory,
            &player->hovered_node_ids,
            &__figma_player_string_element_destroy);
        player->hovered_node_ids = hovered_now;
    }
    else if(event->type == FIGMA_POINTER_EVENT_DOWN)
    {
        const figma_player_hotspot_t * down_hotspot =
            __figma_player_find_hotspot(
                player,
                event->x,
                event->y,
                FIGMA_PROTOTYPE_EVENT_POINTER_DOWN);
        const figma_player_hotspot_t * click_hotspot;
        const figma_player_hotspot_t * capture_hotspot;

        if(down_hotspot == NULL)
        {
            down_hotspot = __figma_player_find_hotspot(
                player,
                event->x,
                event->y,
                FIGMA_PROTOTYPE_EVENT_PRESS);
        }
        click_hotspot = __figma_player_find_hotspot(
            player, event->x, event->y, FIGMA_PROTOTYPE_EVENT_CLICK);
        capture_hotspot =
            click_hotspot != NULL ? click_hotspot : down_hotspot;

        if(capture_hotspot != NULL)
        {
            figma_player_pointer_capture_t * new_capture;
            if(capture != NULL)
            {
                figma_array_remove(
                    player->memory,
                    &player->pointer_captures,
                    capture_index,
                    &__figma_player_capture_destroy);
            }
            new_capture =
                (figma_player_pointer_capture_t *)figma_array_push_uninitialized(
                    player->memory, &player->pointer_captures);
            if(new_capture == NULL)
            {
                return FIGMA_RESULT_OUT_OF_MEMORY;
            }
            memset(new_capture, 0, sizeof(*new_capture));
            figma_string_init(&new_capture->node_id);
            figma_string_init(&new_capture->interaction_id);
            new_capture->pointer_id = event->pointer_id;
            new_capture->button = event->button;
            if(figma_string_copy(
                   player->memory,
                   &new_capture->node_id,
                   &capture_hotspot->node_id) == FIGMA_FALSE ||
                (capture_hotspot->interaction != NULL
                        ? figma_string_copy(
                              player->memory,
                              &new_capture->interaction_id,
                              &capture_hotspot->interaction->id)
                        : figma_string_copy(
                              player->memory,
                              &new_capture->interaction_id,
                              &capture_hotspot->action_id)) == FIGMA_FALSE)
            {
                figma_array_pop(
                    player->memory,
                    &player->pointer_captures,
                    &__figma_player_capture_destroy);
                return FIGMA_RESULT_OUT_OF_MEMORY;
            }
            dispatch.captured = FIGMA_TRUE;
            dispatch.handled = FIGMA_TRUE;
        }

        if(down_hotspot != NULL)
        {
            result = __figma_player_route_hotspot(
                player,
                down_hotspot,
                FIGMA_ACTION_INPUT_POINTER,
                event,
                NULL,
                0.0f);
            dispatch.handled =
                result == FIGMA_RESULT_OK ? FIGMA_TRUE : FIGMA_FALSE;
        }
    }
    else if(event->type == FIGMA_POINTER_EVENT_UP)
    {
        const figma_player_hotspot_t * up_hotspot =
            __figma_player_find_hotspot(
                player,
                event->x,
                event->y,
                FIGMA_PROTOTYPE_EVENT_POINTER_UP);
        if(up_hotspot != NULL)
        {
            result = __figma_player_route_hotspot(
                player,
                up_hotspot,
                FIGMA_ACTION_INPUT_POINTER,
                event,
                NULL,
                0.0f);
            dispatch.handled =
                result == FIGMA_RESULT_OK ? FIGMA_TRUE : FIGMA_FALSE;
        }

        if(capture != NULL)
        {
            figma_player_pointer_capture_t saved;
            size_t index = player->hotspots.size;

            memset(&saved, 0, sizeof(saved));
            figma_string_init(&saved.node_id);
            figma_string_init(&saved.interaction_id);
            saved.button = capture->button;
            if(figma_string_copy(
                   player->memory, &saved.node_id, &capture->node_id) ==
                    FIGMA_FALSE ||
                figma_string_copy(
                    player->memory,
                    &saved.interaction_id,
                    &capture->interaction_id) == FIGMA_FALSE)
            {
                __figma_player_capture_destroy(player->memory, &saved);
                return FIGMA_RESULT_OUT_OF_MEMORY;
            }
            figma_array_remove(
                player->memory,
                &player->pointer_captures,
                capture_index,
                &__figma_player_capture_destroy);
            dispatch.handled = FIGMA_TRUE;
            dispatch.captured = FIGMA_FALSE;

            if(saved.button == event->button)
            {
                while(index != 0u)
                {
                    const figma_player_hotspot_t * click_hotspot;
                    --index;
                    click_hotspot = FIGMA_ARRAY_CONST_PTR(
                        figma_player_hotspot_t, &player->hotspots, index);
                    if(click_hotspot->event_type !=
                            FIGMA_PROTOTYPE_EVENT_CLICK ||
                        figma_string_equal(
                            &click_hotspot->node_id, &saved.node_id) ==
                            FIGMA_FALSE ||
                        __figma_player_contains_hotspot(
                            click_hotspot, event->x, event->y) == FIGMA_FALSE)
                    {
                        continue;
                    }
                    result = __figma_player_route_hotspot(
                        player,
                        click_hotspot,
                        FIGMA_ACTION_INPUT_POINTER,
                        event,
                        NULL,
                        0.0f);
                    dispatch.handled =
                        result == FIGMA_RESULT_OK ? FIGMA_TRUE : FIGMA_FALSE;
                    if(result != FIGMA_RESULT_OK)
                    {
                        break;
                    }
                }
            }
            __figma_player_capture_destroy(player->memory, &saved);
        }
    }
    else if(event->type == FIGMA_POINTER_EVENT_CANCEL && capture != NULL)
    {
        figma_array_remove(
            player->memory,
            &player->pointer_captures,
            capture_index,
            &__figma_player_capture_destroy);
        dispatch.handled = FIGMA_TRUE;
        dispatch.captured = FIGMA_FALSE;
    }

    if(output_dispatch != NULL)
    {
        *output_dispatch = dispatch;
    }
    return result;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_input_key(figma_player_t * player, const figma_key_event_t * event, figma_input_dispatch_result_t * output_dispatch)
{
    figma_input_dispatch_result_t dispatch;
    size_t index;

    if(player == NULL || event == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    memset(&dispatch, 0, sizeof(dispatch));
    if(player->animation_state.active == FIGMA_FALSE &&
        event->type == FIGMA_KEY_EVENT_DOWN)
    {
        index = player->hotspots.size;
        while(index != 0u)
        {
            const figma_player_hotspot_t * hotspot;
            figma_result_t result;
            --index;
            hotspot = FIGMA_ARRAY_CONST_PTR(
                figma_player_hotspot_t, &player->hotspots, index);
            if(hotspot->event_type != FIGMA_PROTOTYPE_EVENT_KEY_DOWN ||
                (hotspot->key_code != 0u &&
                    hotspot->key_code != event->key_code))
            {
                continue;
            }
            result = __figma_player_route_hotspot(
                player,
                hotspot,
                FIGMA_ACTION_INPUT_KEY,
                NULL,
                event,
                0.0f);
            dispatch.handled =
                result == FIGMA_RESULT_OK ? FIGMA_TRUE : FIGMA_FALSE;
            if(output_dispatch != NULL)
            {
                *output_dispatch = dispatch;
            }
            return result;
        }
    }

    if(output_dispatch != NULL)
    {
        *output_dispatch = dispatch;
    }
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_update(figma_player_t * player, float dt)
{
    figma_result_t result;

    if(player == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    dt = __figma_player_maxf(0.0f, dt);
    player->time += dt;
    result = __figma_player_copy_document_diagnostics(player);
    if(result != FIGMA_RESULT_OK)
    {
        return result;
    }
    result = __figma_player_update_animations(player, dt);
    if(result != FIGMA_RESULT_OK)
    {
        return result;
    }
    if(player->animation_state.active == FIGMA_FALSE)
    {
        result = __figma_player_update_timers(player, dt);
        if(result != FIGMA_RESULT_OK)
        {
            return result;
        }
    }
    if(player->hotspots_dirty == FIGMA_TRUE)
    {
        result = figma_player_rebuild_hotspots(player);
        if(result != FIGMA_RESULT_OK)
        {
            return result;
        }
    }
    return figma_player_rebuild_render_list(player);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_restart(figma_player_t * player)
{
    if(player == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    __figma_player_set_current_frame(
        player, __figma_player_resolve_initial_frame(player));
    figma_player_animation_state_destroy(
        player->memory, &player->animation_state);
    figma_player_animation_state_init(&player->animation_state);
    figma_array_clear(
        player->memory,
        &player->hovered_node_ids,
        &__figma_player_string_element_destroy);
    figma_array_clear(
        player->memory,
        &player->fired_timer_ids,
        &__figma_player_string_element_destroy);
    figma_array_clear(
        player->memory,
        &player->pointer_captures,
        &__figma_player_capture_destroy);
    figma_array_clear(
        player->memory, &player->node_swaps, &__figma_player_swap_destroy);
    figma_array_clear(
        player->memory,
        &player->local_animations,
        &__figma_player_local_animation_destroy);
    figma_array_clear(player->memory, &player->navigation_history, NULL);
    figma_array_clear(player->memory, &player->overlay_frames, NULL);
    figma_array_clear(player->memory, &player->overlay_start_times, NULL);
    player->hotspots_dirty = FIGMA_TRUE;
    player->time = 0.0f;
    return figma_player_update(player, 0.0f);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_navigate_to_frame(figma_player_t * player, figma_string_view_t target_frame_id)
{
    if(player == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    return __figma_player_navigate_internal(
        player,
        target_frame_id,
        NULL,
        (figma_string_view_t){NULL, 0u},
        0.0f);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_open_overlay(figma_player_t * player, figma_string_view_t target_frame_id)
{
    const figma_canvas_node_t * target;

    if(player == NULL || target_frame_id.size == 0u ||
        target_frame_id.data == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    target =
        figma_document_find_canvas_node(player->document, target_frame_id);
    if(target == NULL)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }

    if(figma_array_push_copy(
           player->memory, &player->overlay_frames, &target) == FIGMA_FALSE ||
        figma_array_push_copy(
            player->memory, &player->overlay_start_times, &player->time) ==
            FIGMA_FALSE)
    {
        if(player->overlay_frames.size > player->overlay_start_times.size)
        {
            figma_array_pop(player->memory, &player->overlay_frames, NULL);
        }
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }
    figma_array_clear(
        player->memory,
        &player->pointer_captures,
        &__figma_player_capture_destroy);
    figma_array_clear(
        player->memory,
        &player->hovered_node_ids,
        &__figma_player_string_element_destroy);
    player->hotspots_dirty = FIGMA_TRUE;
    if(figma_player_rebuild_hotspots(player) != FIGMA_RESULT_OK ||
        figma_player_rebuild_render_list(player) != FIGMA_RESULT_OK)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    if(player->action_router.on_overlay_opened != NULL)
    {
        player->action_router.on_overlay_opened(
            player->action_router.user_data, figma_string_view(&target->id));
    }
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_close_overlay(figma_player_t * player)
{
    const figma_canvas_node_t * overlay;

    if(player == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    if(player->overlay_frames.size == 0u)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }

    overlay = FIGMA_ARRAY_AT(
        const figma_canvas_node_t *,
        &player->overlay_frames,
        player->overlay_frames.size - 1u);
    figma_array_pop(player->memory, &player->overlay_frames, NULL);
    figma_array_pop(player->memory, &player->overlay_start_times, NULL);
    figma_array_clear(
        player->memory,
        &player->pointer_captures,
        &__figma_player_capture_destroy);
    figma_array_clear(
        player->memory,
        &player->hovered_node_ids,
        &__figma_player_string_element_destroy);
    player->hotspots_dirty = FIGMA_TRUE;
    if(figma_player_rebuild_hotspots(player) != FIGMA_RESULT_OK ||
        figma_player_rebuild_render_list(player) != FIGMA_RESULT_OK)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    if(player->action_router.on_overlay_closed != NULL)
    {
        player->action_router.on_overlay_closed(
            player->action_router.user_data, figma_string_view(&overlay->id));
    }
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_go_back(figma_player_t * player)
{
    const figma_canvas_node_t * frame;

    if(player == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    if(player->overlay_frames.size != 0u)
    {
        return figma_player_close_overlay(player);
    }
    if(player->navigation_history.size == 0u)
    {
        return FIGMA_RESULT_NOT_FOUND;
    }

    frame = FIGMA_ARRAY_AT(
        const figma_canvas_node_t *,
        &player->navigation_history,
        player->navigation_history.size - 1u);
    figma_array_pop(player->memory, &player->navigation_history, NULL);
    __figma_player_set_current_frame(player, frame);
    figma_array_clear(
        player->memory, &player->node_swaps, &__figma_player_swap_destroy);
    figma_array_clear(
        player->memory,
        &player->local_animations,
        &__figma_player_local_animation_destroy);
    figma_array_clear(
        player->memory,
        &player->pointer_captures,
        &__figma_player_capture_destroy);
    figma_array_clear(
        player->memory,
        &player->hovered_node_ids,
        &__figma_player_string_element_destroy);
    player->time = 0.0f;
    return figma_player_update(player, 0.0f);
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_set_override(figma_player_t * player, figma_string_view_t key, const figma_binding_value_t * value)
{
    figma_binding_override_t * override;

    if(player == NULL || value == NULL || key.data == NULL || key.size == 0u ||
        (value->string_value.size != 0u &&
            value->string_value.data == NULL))
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    override = __figma_player_find_override(player, key);
    if(override == NULL)
    {
        override =
            (figma_binding_override_t *)figma_array_push_uninitialized(
                player->memory, &player->overrides);
        if(override == NULL)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }
        memset(override, 0, sizeof(*override));
        figma_string_init(&override->key);
        figma_string_init(&override->string_value);
        if(figma_string_assign(player->memory, &override->key, key) ==
            FIGMA_FALSE)
        {
            figma_array_pop(
                player->memory,
                &player->overrides,
                &__figma_player_override_destroy);
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }
    }

    if(figma_string_assign(
           player->memory,
           &override->string_value,
           value->string_value) == FIGMA_FALSE)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }
    override->type = value->type;
    override->number_value = value->number_value;
    override->bool_value = value->bool_value;
    player->hotspots_dirty = FIGMA_TRUE;
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_set_text(figma_player_t * player, figma_string_view_t key, figma_string_view_t value)
{
    figma_binding_value_t binding;
    memset(&binding, 0, sizeof(binding));
    binding.type = FIGMA_BINDING_VALUE_TEXT;
    binding.string_value = value;
    return __figma_player_set_override(player, key, &binding);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_set_number(figma_player_t * player, figma_string_view_t key, double value)
{
    figma_binding_value_t binding;
    memset(&binding, 0, sizeof(binding));
    binding.type = FIGMA_BINDING_VALUE_NUMBER;
    binding.number_value = value;
    return __figma_player_set_override(player, key, &binding);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_set_visible(figma_player_t * player, figma_string_view_t key, figma_bool_t value)
{
    figma_binding_value_t binding;
    memset(&binding, 0, sizeof(binding));
    binding.type = FIGMA_BINDING_VALUE_BOOLEAN;
    binding.bool_value = value;
    return __figma_player_set_override(player, key, &binding);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_set_enabled(figma_player_t * player, figma_string_view_t key, figma_bool_t value)
{
    return figma_player_set_visible(player, key, value);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_set_image(figma_player_t * player, figma_string_view_t key, figma_string_view_t asset_id)
{
    figma_binding_value_t binding;
    memset(&binding, 0, sizeof(binding));
    binding.type = FIGMA_BINDING_VALUE_IMAGE;
    binding.string_value = asset_id;
    return __figma_player_set_override(player, key, &binding);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_set_state(figma_player_t * player, figma_string_view_t key, figma_bool_t value)
{
    return figma_player_set_visible(player, key, value);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_set_binding_value(figma_player_t * player, figma_string_view_t key, const figma_binding_value_t * value)
{
    return __figma_player_set_override(player, key, value);
}

//////////////////////////////////////////////////////////////////////////
figma_result_t FIGMA_CALL figma_player_clear_binding_value(figma_player_t * player, figma_string_view_t key)
{
    size_t index;

    if(player == NULL || key.data == NULL || key.size == 0u)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }

    for(index = 0u; index != player->overrides.size; ++index)
    {
        figma_binding_override_t * override =
            FIGMA_ARRAY_PTR(figma_binding_override_t, &player->overrides, index);
        if(figma_string_equal_view(&override->key, key) == FIGMA_TRUE)
        {
            figma_array_remove(
                player->memory,
                &player->overrides,
                index,
                &__figma_player_override_destroy);
            break;
        }
    }
    player->hotspots_dirty = FIGMA_TRUE;
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
const figma_render_list_t * FIGMA_CALL figma_player_get_render_list(const figma_player_t * player)
{
    return player != NULL ? &player->render_list : NULL;
}

//////////////////////////////////////////////////////////////////////////
const figma_diagnostics_t * FIGMA_CALL figma_player_get_diagnostics(const figma_player_t * player)
{
    return player != NULL ? &player->diagnostics : NULL;
}

typedef struct figma_graphics_memory_context
{
    figma_memory_t * memory;
} figma_graphics_memory_context_t;

typedef struct figma_path_paint_pair
{
    const figma_canvas_path_t * path;
    const figma_canvas_paint_t * paint;
} figma_path_paint_pair_t;

//////////////////////////////////////////////////////////////////////////
static void * __figma_player_graphics_alloc(gp_size_t size, void * user_data)
{
    figma_graphics_memory_context_t * context =
        (figma_graphics_memory_context_t *)user_data;
    return context != NULL && context->memory != NULL
        ? figma_memory_allocate(context->memory, (size_t)size)
        : NULL;
}

//////////////////////////////////////////////////////////////////////////
static void * __figma_player_graphics_realloc(void * ptr, gp_size_t size, void * user_data)
{
    figma_graphics_memory_context_t * context =
        (figma_graphics_memory_context_t *)user_data;
    return context != NULL && context->memory != NULL
        ? figma_memory_reallocate(context->memory, ptr, (size_t)size)
        : NULL;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_graphics_free(void * ptr, void * user_data)
{
    figma_graphics_memory_context_t * context =
        (figma_graphics_memory_context_t *)user_data;
    if(context != NULL && context->memory != NULL)
    {
        figma_memory_deallocate(context->memory, ptr);
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_assign_command_string(figma_player_t * player, figma_string_t * output, const char * value)
{
    return figma_string_assign_cstr(player->memory, output, value);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_render_graphics(figma_player_t * player, gp_canvas_t * canvas, figma_render_command_t * command)
{
    gp_mesh_t mesh;
    figma_array_t colors;
    size_t index;

    memset(&mesh, 0, sizeof(mesh));
    if(gp_calculate_mesh_size(canvas, &mesh) == GP_FAILURE ||
        mesh.vertex_count == 0u || mesh.index_count == 0u)
    {
        return FIGMA_FALSE;
    }

    if(figma_array_resize(
           player->memory,
           &command->vertices,
           (size_t)mesh.vertex_count,
           NULL) == FIGMA_FALSE ||
        figma_array_resize(
            player->memory,
            &command->indices,
            (size_t)mesh.index_count,
            NULL) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    figma_array_init(&colors, sizeof(gp_argb_t));
    if(figma_array_resize(
           player->memory, &colors, (size_t)mesh.vertex_count, NULL) ==
        FIGMA_FALSE)
    {
        figma_array_destroy(player->memory, &colors, NULL);
        return FIGMA_FALSE;
    }

    mesh.color.r = 1.0f;
    mesh.color.g = 1.0f;
    mesh.color.b = 1.0f;
    mesh.color.a = 1.0f;
    mesh.positions_buffer = command->vertices.data;
    mesh.positions_offset = offsetof(figma_render_vertex_t, x);
    mesh.positions_stride = sizeof(figma_render_vertex_t);
    mesh.colors_buffer = colors.data;
    mesh.colors_offset = 0u;
    mesh.colors_stride = sizeof(gp_argb_t);
    mesh.uv_buffer = command->vertices.data;
    mesh.uv_offset = offsetof(figma_render_vertex_t, u);
    mesh.uv_stride = sizeof(figma_render_vertex_t);
    mesh.indices_buffer = command->indices.data;
    mesh.indices_offset = 0u;
    mesh.indices_stride = sizeof(uint16_t);

    if(gp_render(canvas, &mesh) == GP_FAILURE)
    {
        figma_array_clear(player->memory, &command->vertices, NULL);
        figma_array_clear(player->memory, &command->indices, NULL);
        figma_array_destroy(player->memory, &colors, NULL);
        return FIGMA_FALSE;
    }

    for(index = 0u; index != command->vertices.size; ++index)
    {
        const gp_argb_t argb = FIGMA_ARRAY_AT(gp_argb_t, &colors, index);
        figma_render_vertex_t * vertex =
            FIGMA_ARRAY_PTR(figma_render_vertex_t, &command->vertices, index);
        vertex->color.r = (float)((argb >> 16u) & 0xffu) / 255.0f;
        vertex->color.g = (float)((argb >> 8u) & 0xffu) / 255.0f;
        vertex->color.b = (float)(argb & 0xffu) / 255.0f;
        vertex->color.a = (float)((argb >> 24u) & 0xffu) / 255.0f;
    }

    figma_array_destroy(player->memory, &colors, NULL);
    command->opacity = 1.0f;
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_rectf_t __figma_player_aligned_stroke_rect(figma_rectf_t rect, float stroke_width, figma_canvas_stroke_align_t stroke_align)
{
    if(stroke_align == FIGMA_CANVAS_STROKE_ALIGN_INSIDE)
    {
        const float inset = stroke_width * 0.5f;
        rect.x += inset;
        rect.y += inset;
        rect.w = __figma_player_maxf(0.0f, rect.w - stroke_width);
        rect.h = __figma_player_maxf(0.0f, rect.h - stroke_width);
    }
    else if(stroke_align == FIGMA_CANVAS_STROKE_ALIGN_OUTSIDE)
    {
        const float outset = stroke_width * 0.5f;
        rect.x -= outset;
        rect.y -= outset;
        rect.w += stroke_width;
        rect.h += stroke_width;
    }
    return rect;
}

//////////////////////////////////////////////////////////////////////////
static float __figma_player_aligned_stroke_corner_radius(float radius, float stroke_width, figma_canvas_stroke_align_t stroke_align)
{
    if(stroke_align == FIGMA_CANVAS_STROKE_ALIGN_INSIDE)
    {
        return __figma_player_maxf(0.0f, radius - stroke_width * 0.5f);
    }
    if(stroke_align == FIGMA_CANVAS_STROKE_ALIGN_OUTSIDE)
    {
        return radius + stroke_width * 0.5f;
    }
    return radius;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_build_shape_mesh(figma_player_t * player, figma_render_command_t * command, figma_bool_t fill, figma_canvas_stroke_align_t stroke_align)
{
    figma_graphics_memory_context_t context;
    gp_canvas_t * canvas = NULL;
    figma_rectf_t rect;
    float corner_radius;
    float alpha;
    float stroke_width;
    figma_bool_t result;

    context.memory = player->memory;
    if(gp_canvas_create(
           &canvas,
           &__figma_player_graphics_alloc,
           &__figma_player_graphics_realloc,
           &__figma_player_graphics_free,
           &context) == GP_FAILURE ||
        canvas == NULL)
    {
        return FIGMA_FALSE;
    }

    alpha = __figma_player_clamp01(command->color.a * command->opacity);
    stroke_width = __figma_player_maxf(0.0f, command->stroke_width);
    gp_set_color(
        canvas,
        command->color.r,
        command->color.g,
        command->color.b,
        alpha);
    gp_set_thickness(canvas, stroke_width);
    gp_set_penumbra(
        canvas,
        fill == FIGMA_FALSE && stroke_width > 0.0f
            ? __figma_player_minf(0.25f, stroke_width * 0.25f)
            : 0.0f);
    gp_set_curve_quality(canvas, 24);
    gp_set_ellipse_quality(canvas, 64);
    gp_set_rect_quality(canvas, 16);

    if(fill == FIGMA_TRUE)
    {
        gp_begin_fill(canvas);
    }

    rect = fill == FIGMA_TRUE
        ? command->rect
        : __figma_player_aligned_stroke_rect(
              command->rect, stroke_width, stroke_align);
    corner_radius = fill == FIGMA_TRUE
        ? command->corner_radius
        : __figma_player_aligned_stroke_corner_radius(
              command->corner_radius, stroke_width, stroke_align);

    if(command->shape == FIGMA_RENDER_SHAPE_ELLIPSE)
    {
        const float center_x = rect.x + rect.w * 0.5f;
        const float center_y = rect.y + rect.h * 0.5f;
        const float radius_x = rect.w * 0.5f;
        const float radius_y = rect.h * 0.5f;
        if(command->has_arc_data == FIGMA_TRUE)
        {
            gp_set_inner_radius(
                canvas, __figma_player_clamp01(command->arc_inner_radius));
            if(fill == FIGMA_FALSE)
            {
                gp_set_inner_radius(canvas, 0.0f);
            }
            gp_ellipse_arc(
                canvas,
                center_x,
                center_y,
                radius_x,
                radius_y,
                command->arc_starting_angle,
                command->arc_ending_angle);
        }
        else
        {
            gp_ellipse(canvas, center_x, center_y, radius_x, radius_y);
        }
    }
    else if(command->shape == FIGMA_RENDER_SHAPE_ROUNDED_RECTANGLE &&
        corner_radius > 0.0f)
    {
        gp_rounded_rect(
            canvas,
            rect.x,
            rect.y,
            rect.w,
            rect.h,
            __figma_player_minf(
                corner_radius, __figma_player_minf(rect.w, rect.h) * 0.5f));
    }
    else
    {
        gp_rect(canvas, rect.x, rect.y, rect.w, rect.h);
    }

    if(fill == FIGMA_TRUE)
    {
        gp_end_fill(canvas);
    }

    result = __figma_player_render_graphics(player, canvas, command);
    gp_canvas_destroy(canvas);
    return result;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_same_point(figma_vec2f_t left, figma_vec2f_t right)
{
    return fabsf(left.x - right.x) <= 0.001f &&
        fabsf(left.y - right.y) <= 0.001f;
}

//////////////////////////////////////////////////////////////////////////
static figma_vec2f_t __figma_player_local_path_point(figma_rectf_t rect, figma_vec2f_t point)
{
    point.x += rect.x;
    point.y += rect.y;
    return point;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_append_path(gp_canvas_t * canvas, figma_rectf_t rect, const figma_canvas_path_t * path, figma_bool_t fill)
{
    figma_bool_t has_contour = FIGMA_FALSE;
    figma_vec2f_t start = {0.0f, 0.0f};
    figma_vec2f_t current = {0.0f, 0.0f};
    size_t index;

    if(path->commands_decoded == FIGMA_FALSE || path->commands.size == 0u)
    {
        return FIGMA_FALSE;
    }

    for(index = 0u; index != path->commands.size; ++index)
    {
        const figma_canvas_path_command_t * command =
            FIGMA_ARRAY_CONST_PTR(
                figma_canvas_path_command_t, &path->commands, index);
        switch(command->type)
        {
        case FIGMA_CANVAS_PATH_MOVE_TO:
        {
            const figma_vec2f_t point =
                __figma_player_local_path_point(rect, command->p0);
            if(gp_point_move_to(canvas, point.x, point.y) == GP_FAILURE)
            {
                return FIGMA_FALSE;
            }
            start = point;
            current = point;
            has_contour = FIGMA_TRUE;
        }
        break;
        case FIGMA_CANVAS_PATH_LINE_TO:
        {
            const figma_vec2f_t point =
                __figma_player_local_path_point(rect, command->p0);
            figma_bool_t next_closes;
            if(has_contour == FIGMA_FALSE)
            {
                return FIGMA_FALSE;
            }
            next_closes =
                index + 1u < path->commands.size &&
                    FIGMA_ARRAY_CONST_PTR(
                        figma_canvas_path_command_t,
                        &path->commands,
                        index + 1u)
                            ->type == FIGMA_CANVAS_PATH_CLOSE
                ? FIGMA_TRUE
                : FIGMA_FALSE;
            if(fill == FIGMA_TRUE && next_closes == FIGMA_TRUE &&
                __figma_player_same_point(point, start) == FIGMA_TRUE)
            {
                current = point;
                break;
            }
            if(gp_point_line_to(canvas, point.x, point.y) == GP_FAILURE)
            {
                return FIGMA_FALSE;
            }
            current = point;
        }
        break;
        case FIGMA_CANVAS_PATH_QUADRATIC_TO:
        {
            const figma_vec2f_t p0 =
                __figma_player_local_path_point(rect, command->p0);
            const figma_vec2f_t p1 =
                __figma_player_local_path_point(rect, command->p1);
            if(has_contour == FIGMA_FALSE ||
                gp_point_quadratic_curve_to(
                    canvas, p0.x, p0.y, p1.x, p1.y) == GP_FAILURE)
            {
                return FIGMA_FALSE;
            }
            current = p1;
        }
        break;
        case FIGMA_CANVAS_PATH_CUBIC_TO:
        {
            const figma_vec2f_t p0 =
                __figma_player_local_path_point(rect, command->p0);
            const figma_vec2f_t p1 =
                __figma_player_local_path_point(rect, command->p1);
            const figma_vec2f_t p2 =
                __figma_player_local_path_point(rect, command->p2);
            if(has_contour == FIGMA_FALSE ||
                gp_point_bezier_curve_to(
                    canvas, p0.x, p0.y, p1.x, p1.y, p2.x, p2.y) ==
                    GP_FAILURE)
            {
                return FIGMA_FALSE;
            }
            current = p2;
        }
        break;
        case FIGMA_CANVAS_PATH_CLOSE:
            if(has_contour == FIGMA_TRUE)
            {
                if(fill == FIGMA_FALSE &&
                    __figma_player_same_point(current, start) == FIGMA_FALSE &&
                    gp_point_line_to(canvas, start.x, start.y) == GP_FAILURE)
                {
                    return FIGMA_FALSE;
                }
                has_contour = FIGMA_FALSE;
            }
            break;
        }
    }

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_build_path_mesh(figma_player_t * player, figma_render_command_t * command, const figma_canvas_path_t * paths, size_t path_count, figma_bool_t fill)
{
    figma_graphics_memory_context_t context;
    gp_canvas_t * canvas = NULL;
    figma_bool_t appended = FIGMA_FALSE;
    figma_bool_t result = FIGMA_FALSE;
    size_t index;

    context.memory = player->memory;
    if(gp_canvas_create(
           &canvas,
           &__figma_player_graphics_alloc,
           &__figma_player_graphics_realloc,
           &__figma_player_graphics_free,
           &context) == GP_FAILURE ||
        canvas == NULL)
    {
        return FIGMA_FALSE;
    }

    gp_set_color(
        canvas,
        command->color.r,
        command->color.g,
        command->color.b,
        __figma_player_clamp01(command->color.a * command->opacity));
    gp_set_penumbra(canvas, 0.0f);
    gp_set_thickness(canvas, __figma_player_maxf(0.0f, command->stroke_width));
    gp_set_curve_quality(canvas, 24);
    gp_set_ellipse_quality(canvas, 64);
    gp_set_rect_quality(canvas, 16);
    if(fill == FIGMA_TRUE)
    {
        gp_begin_fill(canvas);
    }

    for(index = 0u; index != path_count; ++index)
    {
        if(paths[index].commands_decoded == FIGMA_FALSE ||
            paths[index].commands.size == 0u)
        {
            continue;
        }
        if(__figma_player_append_path(
               canvas, command->rect, &paths[index], fill) == FIGMA_FALSE)
        {
            gp_canvas_destroy(canvas);
            return FIGMA_FALSE;
        }
        appended = FIGMA_TRUE;
    }
    if(fill == FIGMA_TRUE)
    {
        gp_end_fill(canvas);
    }
    if(appended == FIGMA_TRUE)
    {
        result = __figma_player_render_graphics(player, canvas, command);
    }
    gp_canvas_destroy(canvas);
    return result;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_build_grouped_path_mesh(figma_player_t * player, figma_render_command_t * command, const figma_array_t * pairs, float opacity)
{
    figma_graphics_memory_context_t context;
    gp_canvas_t * canvas = NULL;
    figma_bool_t appended = FIGMA_FALSE;
    figma_bool_t result = FIGMA_FALSE;
    size_t index;

    context.memory = player->memory;
    if(gp_canvas_create(
           &canvas,
           &__figma_player_graphics_alloc,
           &__figma_player_graphics_realloc,
           &__figma_player_graphics_free,
           &context) == GP_FAILURE ||
        canvas == NULL)
    {
        return FIGMA_FALSE;
    }
    gp_set_penumbra(canvas, 0.0f);
    gp_set_thickness(canvas, __figma_player_maxf(0.0f, command->stroke_width));
    gp_set_curve_quality(canvas, 24);
    gp_set_ellipse_quality(canvas, 64);
    gp_set_rect_quality(canvas, 16);

    for(index = 0u; index != pairs->size; ++index)
    {
        const figma_path_paint_pair_t * pair =
            FIGMA_ARRAY_CONST_PTR(figma_path_paint_pair_t, pairs, index);
        if(pair->path == NULL || pair->paint == NULL ||
            pair->path->commands_decoded == FIGMA_FALSE ||
            pair->path->commands.size == 0u)
        {
            continue;
        }
        gp_set_color(
            canvas,
            pair->paint->color.r,
            pair->paint->color.g,
            pair->paint->color.b,
            __figma_player_clamp01(
                pair->paint->color.a * opacity *
                __figma_player_clamp01(pair->paint->opacity)));
        gp_begin_fill(canvas);
        if(__figma_player_append_path(
               canvas, command->rect, pair->path, FIGMA_TRUE) == FIGMA_FALSE)
        {
            gp_canvas_destroy(canvas);
            return FIGMA_FALSE;
        }
        gp_end_fill(canvas);
        appended = FIGMA_TRUE;
    }
    if(appended == FIGMA_TRUE)
    {
        result = __figma_player_render_graphics(player, canvas, command);
    }
    gp_canvas_destroy(canvas);
    return result;
}

//////////////////////////////////////////////////////////////////////////
static figma_render_shape_type_t __figma_player_render_shape(figma_canvas_node_type_t type)
{
    if(type == FIGMA_CANVAS_NODE_ELLIPSE)
    {
        return FIGMA_RENDER_SHAPE_ELLIPSE;
    }
    if(type == FIGMA_CANVAS_NODE_ROUNDED_RECTANGLE)
    {
        return FIGMA_RENDER_SHAPE_ROUNDED_RECTANGLE;
    }
    return FIGMA_RENDER_SHAPE_RECTANGLE;
}

//////////////////////////////////////////////////////////////////////////
static figma_render_blend_mode_t __figma_player_render_blend(figma_canvas_blend_mode_t mode)
{
    return mode <= FIGMA_CANVAS_BLEND_UNSUPPORTED
        ? (figma_render_blend_mode_t)mode
        : FIGMA_RENDER_BLEND_UNSUPPORTED;
}

//////////////////////////////////////////////////////////////////////////
static figma_render_image_scale_mode_t __figma_player_render_image_scale(figma_canvas_image_scale_mode_t mode)
{
    return mode <= FIGMA_CANVAS_IMAGE_SCALE_UNKNOWN
        ? (figma_render_image_scale_mode_t)mode
        : FIGMA_RENDER_IMAGE_SCALE_UNKNOWN;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_assign_paint_metadata(const figma_canvas_paint_t * paint, figma_render_command_t * command)
{
    command->blend_mode = __figma_player_render_blend(paint->blend_mode);
    command->image_scale_mode =
        __figma_player_render_image_scale(paint->image_scale_mode);
    command->original_image_width = paint->original_image_width;
    command->original_image_height = paint->original_image_height;
    command->has_image_transform = paint->has_transform;
    command->has_filter_color_adjust = paint->has_filter_color_adjust;
    command->has_paint_filter = paint->has_paint_filter;
    memcpy(
        command->image_transform,
        paint->transform,
        sizeof(command->image_transform));
    memcpy(
        command->filter_color_adjust,
        paint->filter_color_adjust,
        sizeof(command->filter_color_adjust));
    memcpy(
        command->paint_filter,
        paint->paint_filter,
        sizeof(command->paint_filter));
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_apply_node_blend(const figma_canvas_node_t * node, figma_render_command_t * command)
{
    if(node->blend_mode != FIGMA_CANVAS_BLEND_NORMAL &&
        node->blend_mode != FIGMA_CANVAS_BLEND_PASS_THROUGH &&
        (command->blend_mode == FIGMA_RENDER_BLEND_NORMAL ||
            command->blend_mode == FIGMA_RENDER_BLEND_PASS_THROUGH))
    {
        command->blend_mode = __figma_player_render_blend(node->blend_mode);
    }
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_assign_arc(const figma_canvas_arc_data_t * arc, figma_render_command_t * command)
{
    if(arc->valid == FIGMA_TRUE)
    {
        command->has_arc_data = FIGMA_TRUE;
        command->arc_starting_angle = arc->starting_angle;
        command->arc_ending_angle = arc->ending_angle;
        command->arc_inner_radius = arc->inner_radius;
    }
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_make_render_quad(figma_rectf_t node_rect, const figma_vec2f_t node_quad[4], figma_rectf_t render_rect, figma_vec2f_t render_quad[4])
{
    size_t index;

    if(fabsf(node_rect.w) <= 0.0001f || fabsf(node_rect.h) <= 0.0001f)
    {
        render_quad[0] = (figma_vec2f_t){render_rect.x, render_rect.y};
        render_quad[1] =
            (figma_vec2f_t){render_rect.x + render_rect.w, render_rect.y};
        render_quad[2] = (figma_vec2f_t){
            render_rect.x + render_rect.w,
            render_rect.y + render_rect.h};
        render_quad[3] =
            (figma_vec2f_t){render_rect.x, render_rect.y + render_rect.h};
        return;
    }

    for(index = 0u; index != 4u; ++index)
    {
        const float normalized_x =
            (node_quad[index].x - node_rect.x) / node_rect.w;
        const float normalized_y =
            (node_quad[index].y - node_rect.y) / node_rect.h;
        render_quad[index].x = render_rect.x + normalized_x * render_rect.w;
        render_quad[index].y = render_rect.y + normalized_y * render_rect.h;
    }
}

//////////////////////////////////////////////////////////////////////////
static float __figma_player_lerp(float from, float to, float progress)
{
    return from + (to - from) * progress;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_apply_quad(figma_render_command_t * command, figma_rectf_t node_rect, const figma_vec2f_t node_quad[4], figma_bool_t path, figma_vec2f_t node_size)
{
    figma_vec2f_t quad[4];
    size_t index;
    const float width = path == FIGMA_TRUE ? node_size.x : command->rect.w;
    const float height = path == FIGMA_TRUE ? node_size.y : command->rect.h;

    if(command->vertices.size == 0u || fabsf(width) <= 0.0001f ||
        fabsf(height) <= 0.0001f)
    {
        return;
    }

    __figma_player_make_render_quad(
        node_rect, node_quad, command->rect, quad);
    for(index = 0u; index != command->vertices.size; ++index)
    {
        figma_render_vertex_t * vertex =
            FIGMA_ARRAY_PTR(figma_render_vertex_t, &command->vertices, index);
        const float normalized_x = (vertex->x - command->rect.x) / width;
        const float normalized_y = (vertex->y - command->rect.y) / height;
        const float top_x =
            __figma_player_lerp(quad[0].x, quad[1].x, normalized_x);
        const float top_y =
            __figma_player_lerp(quad[0].y, quad[1].y, normalized_x);
        const float bottom_x =
            __figma_player_lerp(quad[3].x, quad[2].x, normalized_x);
        const float bottom_y =
            __figma_player_lerp(quad[3].y, quad[2].y, normalized_x);
        vertex->x = __figma_player_lerp(top_x, bottom_x, normalized_y);
        vertex->y = __figma_player_lerp(top_y, bottom_y, normalized_y);
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_add_quad(figma_player_t * player, figma_render_command_t * command, figma_rectf_t node_rect, const figma_vec2f_t node_quad[4], figma_bool_t image, const figma_asset_t * asset)
{
    static const uint16_t indices[6] = {0u, 1u, 2u, 0u, 2u, 3u};
    figma_vec2f_t quad[4];
    float x0 = command->rect.x;
    float y0 = command->rect.y;
    float x1 = command->rect.x + command->rect.w;
    float y1 = command->rect.y + command->rect.h;
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;
    float source_width =
        asset != NULL && asset->width > 0u
        ? (float)asset->width
        : __figma_player_maxf(1.0f, command->rect.w);
    float source_height =
        asset != NULL && asset->height > 0u
        ? (float)asset->height
        : __figma_player_maxf(1.0f, command->rect.h);
    size_t index;

    if(image == FIGMA_TRUE)
    {
        const float source_aspect =
            source_width / __figma_player_maxf(1.0f, source_height);
        const float target_aspect =
            command->rect.w / __figma_player_maxf(1.0f, command->rect.h);
        if(command->image_scale_mode == FIGMA_RENDER_IMAGE_SCALE_FILL)
        {
            if(source_aspect > target_aspect)
            {
                const float cropped_width = source_height * target_aspect;
                const float crop =
                    (source_width - cropped_width) * 0.5f / source_width;
                u0 = crop;
                u1 = 1.0f - crop;
            }
            else if(source_aspect < target_aspect)
            {
                const float cropped_height =
                    source_width / __figma_player_maxf(0.001f, target_aspect);
                const float crop =
                    (source_height - cropped_height) * 0.5f / source_height;
                v0 = crop;
                v1 = 1.0f - crop;
            }
        }
        else if(command->image_scale_mode == FIGMA_RENDER_IMAGE_SCALE_FIT)
        {
            if(source_aspect > target_aspect)
            {
                const float height =
                    command->rect.w /
                    __figma_player_maxf(0.001f, source_aspect);
                y0 = command->rect.y + (command->rect.h - height) * 0.5f;
                y1 = y0 + height;
            }
            else
            {
                const float width = command->rect.h * source_aspect;
                x0 = command->rect.x + (command->rect.w - width) * 0.5f;
                x1 = x0 + width;
            }
            command->rect =
                (figma_rectf_t){x0, y0, x1 - x0, y1 - y0};
        }
    }

    if(figma_array_resize(
           player->memory, &command->vertices, 4u, NULL) == FIGMA_FALSE ||
        figma_array_resize(
            player->memory, &command->indices, 6u, NULL) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    __figma_player_make_render_quad(
        node_rect, node_quad, command->rect, quad);
    for(index = 0u; index != 4u; ++index)
    {
        figma_render_vertex_t * vertex =
            FIGMA_ARRAY_PTR(figma_render_vertex_t, &command->vertices, index);
        vertex->x = quad[index].x;
        vertex->y = quad[index].y;
        vertex->u = index == 0u || index == 3u ? u0 : u1;
        vertex->v = index == 0u || index == 1u ? v0 : v1;
        vertex->color =
            (figma_colorf_t){1.0f, 1.0f, 1.0f, 1.0f};
    }

    if(image == FIGMA_TRUE && command->has_image_transform == FIGMA_TRUE)
    {
        static const float local_x[4] = {0.0f, 1.0f, 1.0f, 0.0f};
        static const float local_y[4] = {0.0f, 0.0f, 1.0f, 1.0f};
        for(index = 0u; index != 4u; ++index)
        {
            figma_render_vertex_t * vertex =
                FIGMA_ARRAY_PTR(
                    figma_render_vertex_t, &command->vertices, index);
            vertex->u = command->image_transform[0] * local_x[index] +
                command->image_transform[1] * local_y[index] +
                command->image_transform[2];
            vertex->v = command->image_transform[3] * local_x[index] +
                command->image_transform[4] * local_y[index] +
                command->image_transform[5];
        }
    }

    if(image == FIGMA_TRUE)
    {
        figma_render_vertex_t * vertices =
            (figma_render_vertex_t *)command->vertices.data;
        float min_u = vertices[0].u;
        float max_u = vertices[0].u;
        float min_v = vertices[0].v;
        float max_v = vertices[0].v;
        float range_u;
        float range_v;
        float inset_u;
        float inset_v;
        for(index = 1u; index != 4u; ++index)
        {
            min_u = __figma_player_minf(min_u, vertices[index].u);
            max_u = __figma_player_maxf(max_u, vertices[index].u);
            min_v = __figma_player_minf(min_v, vertices[index].v);
            max_v = __figma_player_maxf(max_v, vertices[index].v);
        }
        range_u = max_u - min_u;
        range_v = max_v - min_v;
        inset_u = __figma_player_minf(
            range_u * 0.49f,
            0.5f / __figma_player_maxf(1.0f, source_width));
        inset_v = __figma_player_minf(
            range_v * 0.49f,
            0.5f / __figma_player_maxf(1.0f, source_height));
        for(index = 0u; index != 4u; ++index)
        {
            if(range_u > 0.000001f && inset_u > 0.0f)
            {
                const float t = (vertices[index].u - min_u) / range_u;
                vertices[index].u = min_u + inset_u +
                    t * __figma_player_maxf(0.0f, range_u - inset_u * 2.0f);
            }
            if(range_v > 0.000001f && inset_v > 0.0f)
            {
                const float t = (vertices[index].v - min_v) / range_v;
                vertices[index].v = min_v + inset_v +
                    t * __figma_player_maxf(0.0f, range_v - inset_v * 2.0f);
            }
        }
    }

    memcpy(command->indices.data, indices, sizeof(indices));
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_has_decoded_paths(const figma_array_t * paths)
{
    size_t index;
    for(index = 0u; index != paths->size; ++index)
    {
        const figma_canvas_path_t * path =
            FIGMA_ARRAY_CONST_PTR(figma_canvas_path_t, paths, index);
        if(path->commands_decoded == FIGMA_TRUE &&
            path->commands.size != 0u)
        {
            return FIGMA_TRUE;
        }
    }
    return FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_has_compound_paths(const figma_array_t * paths)
{
    size_t path_index;
    for(path_index = 0u; path_index != paths->size; ++path_index)
    {
        const figma_canvas_path_t * path =
            FIGMA_ARRAY_CONST_PTR(figma_canvas_path_t, paths, path_index);
        size_t command_index;
        uint32_t contours = 0u;
        for(command_index = 0u;
            command_index != path->commands.size;
            ++command_index)
        {
            if(FIGMA_ARRAY_CONST_PTR(
                   figma_canvas_path_command_t,
                   &path->commands,
                   command_index)
                    ->type == FIGMA_CANVAS_PATH_MOVE_TO &&
                ++contours > 1u)
            {
                return FIGMA_TRUE;
            }
        }
    }
    return FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_has_path_paints(const figma_array_t * paths)
{
    size_t index;
    for(index = 0u; index != paths->size; ++index)
    {
        if(FIGMA_ARRAY_CONST_PTR(figma_canvas_path_t, paths, index)
               ->paints.size != 0u)
        {
            return FIGMA_TRUE;
        }
    }
    return FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_is_primitive(figma_canvas_node_type_t type)
{
    return type == FIGMA_CANVAS_NODE_RECTANGLE ||
        type == FIGMA_CANVAS_NODE_ROUNDED_RECTANGLE ||
        type == FIGMA_CANVAS_NODE_ELLIPSE ||
        type == FIGMA_CANVAS_NODE_FRAME;
}

//////////////////////////////////////////////////////////////////////////
static int32_t __figma_player_font_weight(const figma_canvas_node_t * node)
{
    if(figma_string_contains_view(
           &node->font_style, figma_string_view_cstr("Black")) ||
        figma_string_contains_view(
            &node->font_style, figma_string_view_cstr("Heavy")))
    {
        return 900;
    }
    if(figma_string_contains_view(
           &node->font_style, figma_string_view_cstr("ExtraBold")) ||
        figma_string_contains_view(
            &node->font_style, figma_string_view_cstr("UltraBold")))
    {
        return 800;
    }
    if(figma_string_contains_view(
           &node->font_style, figma_string_view_cstr("Bold")))
    {
        return 700;
    }
    if(figma_string_contains_view(
           &node->font_style, figma_string_view_cstr("SemiBold")) ||
        figma_string_contains_view(
            &node->font_style, figma_string_view_cstr("DemiBold")))
    {
        return 600;
    }
    if(figma_string_contains_view(
           &node->font_style, figma_string_view_cstr("Medium")))
    {
        return 500;
    }
    if(figma_string_contains_view(
           &node->font_style, figma_string_view_cstr("Light")))
    {
        return 300;
    }
    if(figma_string_contains_view(
           &node->font_style, figma_string_view_cstr("Thin")))
    {
        return 200;
    }
    return node->font_weight;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_init_command(figma_player_t * player, figma_render_command_t * command, const char * id, const figma_canvas_node_t * node, figma_rectf_t rect)
{
    command->rect = rect;
    return __figma_player_assign_command_string(player, &command->id, id) ==
            FIGMA_TRUE &&
        (node == NULL ||
            figma_string_copy(
                player->memory, &command->node_id, &node->id) == FIGMA_TRUE);
}

typedef struct figma_player_animation_render_context
{
    const figma_array_t * tracks;
    const figma_array_t * dissolve_pairs;
    const figma_canvas_node_t * root;
    float progress;
    uint32_t render_layer_id;
    float render_layer_opacity;
    figma_bool_t smart_animate;
    figma_bool_t target_pass;
    figma_bool_t skip_root_geometry;
    figma_bool_t preserve_node_swap_state;
    figma_bool_t render_layer_disabled;
} figma_player_animation_render_context_t;

//////////////////////////////////////////////////////////////////////////
static const figma_animation_track_t * __figma_player_find_animation_track(const figma_player_animation_render_context_t * animation, const figma_canvas_node_t * node, figma_animation_track_type_t type)
{
    size_t index;
    if(animation == NULL || animation->tracks == NULL || node->id.size == 0u)
    {
        return NULL;
    }
    for(index = 0u; index != animation->tracks->size; ++index)
    {
        const figma_animation_track_t * track =
            FIGMA_ARRAY_CONST_PTR(
                figma_animation_track_t, animation->tracks, index);
        const figma_string_t * id = animation->target_pass == FIGMA_TRUE
            ? &track->target_node_id
            : &track->node_id;
        if(track->type == type &&
            figma_string_equal(id, &node->id) == FIGMA_TRUE)
        {
            return track;
        }
    }
    return NULL;
}

//////////////////////////////////////////////////////////////////////////
static const figma_animation_track_t * __figma_player_find_persistent_track(const figma_player_animation_render_context_t * animation, const figma_canvas_node_t * node)
{
    const figma_animation_track_t * track =
        __figma_player_find_animation_track(
            animation, node, FIGMA_ANIMATION_TRACK_RECT);
    return track != NULL && track->persistent == FIGMA_TRUE
        ? track
        : NULL;
}

//////////////////////////////////////////////////////////////////////////
static const figma_player_dissolve_pair_t * __figma_player_find_dissolve_pair(const figma_player_animation_render_context_t * animation, const figma_canvas_node_t * node, figma_bool_t target)
{
    size_t index;
    if(animation == NULL || animation->dissolve_pairs == NULL)
    {
        return NULL;
    }
    for(index = 0u; index != animation->dissolve_pairs->size; ++index)
    {
        const figma_player_dissolve_pair_t * pair =
            FIGMA_ARRAY_CONST_PTR(
                figma_player_dissolve_pair_t,
                animation->dissolve_pairs,
                index);
        if((target == FIGMA_TRUE ? pair->target : pair->source) == node)
        {
            return pair;
        }
    }
    return NULL;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_animation_node_matched(const figma_player_animation_render_context_t * animation, const figma_canvas_node_t * node)
{
    return __figma_player_find_animation_track(
               animation, node, FIGMA_ANIMATION_TRACK_RECT) != NULL
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static void __figma_player_assign_render_layer(figma_player_t * player, const figma_player_animation_render_context_t * animation, size_t begin, size_t end)
{
    size_t index;
    if(animation == NULL || animation->render_layer_id == 0u ||
        animation->render_layer_disabled == FIGMA_TRUE)
    {
        return;
    }
    for(index = begin; index != end; ++index)
    {
        figma_render_command_t * command =
            FIGMA_ARRAY_PTR(
                figma_render_command_t, &player->render_list.commands, index);
        command->render_layer_id = animation->render_layer_id;
        command->render_layer_opacity = animation->render_layer_opacity;
    }
}

static float __figma_player_apply_easing(figma_animation_easing_t easing, float progress);

static figma_bool_t __figma_player_append_canvas_node(figma_player_t * player, const figma_canvas_node_t * node, float parent_opacity, float offset_x, float offset_y, figma_bool_t allow_state, const figma_player_animation_render_context_t * animation);

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_append_solid_path_fill(figma_player_t * player, const figma_canvas_node_t * node, const figma_canvas_paint_t * paint, const figma_canvas_path_t * path, figma_rectf_t rect, figma_rectf_t node_rect, const figma_vec2f_t node_quad[4], const figma_canvas_arc_data_t * node_arc_data, float opacity)
{
    figma_render_command_t * fill;

    if(paint->visible == FIGMA_FALSE ||
        paint->type != FIGMA_CANVAS_PAINT_SOLID ||
        paint->blend_mode == FIGMA_CANVAS_BLEND_UNSUPPORTED)
    {
        return FIGMA_TRUE;
    }

    fill = figma_render_list_add(
        &player->render_list, FIGMA_RENDER_COMMAND_MESH);
    if(fill == NULL ||
        __figma_player_init_command(
            player, fill, "figma.path_fill", node, rect) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }
    fill->color = paint->color;
    fill->shape = __figma_player_render_shape(node->type);
    fill->corner_radius = node->corner_radius;
    fill->opacity = opacity * __figma_player_clamp01(paint->opacity);
    __figma_player_assign_paint_metadata(paint, fill);
    __figma_player_apply_node_blend(node, fill);
    __figma_player_assign_arc(node_arc_data, fill);
    if(__figma_player_build_path_mesh(
           player, fill, path, 1u, FIGMA_TRUE) == FIGMA_FALSE)
    {
        figma_render_list_remove_last(&player->render_list);
        return FIGMA_TRUE;
    }
    __figma_player_apply_quad(
        fill, node_rect, node_quad, FIGMA_TRUE, node->size);
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __figma_player_append_canvas_node(figma_player_t * player, const figma_canvas_node_t * node, float parent_opacity, float offset_x, float offset_y, figma_bool_t allow_state, const figma_player_animation_render_context_t * animation)
{
    figma_rectf_t node_rect;
    figma_rectf_t rect;
    figma_vec2f_t node_quad[4];
    figma_canvas_arc_data_t node_arc_data;
    float node_opacity;
    float opacity;
    figma_bool_t use_primitive;
    figma_bool_t has_fill_paths;
    figma_bool_t has_stroke_paths;
    figma_bool_t use_fill_paths;
    figma_bool_t use_stroke_paths;
    figma_bool_t render_own_geometry;
    figma_bool_t skip_own_geometry = FIGMA_FALSE;
    figma_player_animation_render_context_t local_animation;
    size_t command_begin = player->render_list.commands.size;
    size_t closing_begin;
    size_t index;

    if(node->visible == FIGMA_FALSE ||
        figma_player_is_node_visible(player, node) == FIGMA_FALSE)
    {
        return FIGMA_TRUE;
    }

    if(__figma_player_find_dissolve_pair(
           animation, node, FIGMA_FALSE) != NULL)
    {
        return FIGMA_TRUE;
    }
    if(__figma_player_find_dissolve_pair(
           animation, node, FIGMA_TRUE) != NULL)
    {
        local_animation = *animation;
        local_animation.render_layer_disabled = FIGMA_TRUE;
        animation = &local_animation;
    }

    node_rect = node->rect;
    memcpy(node_quad, node->quad, sizeof(node_quad));
    node_arc_data = node->arc_data;
    node_opacity = node->opacity;

    if(animation != NULL && animation->smart_animate == FIGMA_TRUE &&
        animation->target_pass == FIGMA_TRUE && node != animation->root)
    {
        const figma_animation_track_t * persistent =
            __figma_player_find_persistent_track(animation, node);
        if(persistent != NULL && persistent->persistent_source_node != NULL)
        {
            const figma_canvas_node_t * source =
                persistent->persistent_source_node;
            return __figma_player_append_canvas_node(
                player,
                source,
                parent_opacity,
                offset_x + node_rect.x - source->rect.x,
                offset_y + node_rect.y - source->rect.y,
                FIGMA_FALSE,
                NULL);
        }
    }

    if(animation != NULL && animation->smart_animate == FIGMA_TRUE &&
        node != animation->root)
    {
        const figma_bool_t matched =
            __figma_player_animation_node_matched(animation, node);
        if(animation->target_pass == FIGMA_TRUE)
        {
            const figma_animation_track_t * track;
            if(matched == FIGMA_FALSE)
            {
                node_opacity *= animation->progress;
            }
            track = __figma_player_find_animation_track(
                animation, node, FIGMA_ANIMATION_TRACK_RECT);
            if(track != NULL)
            {
                size_t quad_index;
                node_rect.x = __figma_player_lerp(
                    track->from[0], track->to[0], animation->progress);
                node_rect.y = __figma_player_lerp(
                    track->from[1], track->to[1], animation->progress);
                node_rect.w = __figma_player_lerp(
                    track->from[2], track->to[2], animation->progress);
                node_rect.h = __figma_player_lerp(
                    track->from[3], track->to[3], animation->progress);
                if(track->has_quad == FIGMA_TRUE)
                {
                    for(quad_index = 0u; quad_index != 4u; ++quad_index)
                    {
                        node_quad[quad_index].x = __figma_player_lerp(
                            track->from_quad[quad_index].x,
                            track->to_quad[quad_index].x,
                            animation->progress);
                        node_quad[quad_index].y = __figma_player_lerp(
                            track->from_quad[quad_index].y,
                            track->to_quad[quad_index].y,
                            animation->progress);
                    }
                }
            }
            track = __figma_player_find_animation_track(
                animation, node, FIGMA_ANIMATION_TRACK_OPACITY);
            if(track != NULL)
            {
                node_opacity = __figma_player_lerp(
                    track->from[0], track->to[0], animation->progress);
            }
            track = __figma_player_find_animation_track(
                animation, node, FIGMA_ANIMATION_TRACK_ARC);
            if(track != NULL)
            {
                node_arc_data.starting_angle = __figma_player_lerp(
                    track->from[0], track->to[0], animation->progress);
                node_arc_data.ending_angle = __figma_player_lerp(
                    track->from[1], track->to[1], animation->progress);
                node_arc_data.inner_radius = __figma_player_lerp(
                    track->from[2], track->to[2], animation->progress);
                node_arc_data.valid = FIGMA_TRUE;
            }
        }
        else if(matched == FIGMA_TRUE)
        {
            skip_own_geometry = FIGMA_TRUE;
        }
        else
        {
            node_opacity *= 1.0f - animation->progress;
        }
    }
    if(animation != NULL && animation->smart_animate == FIGMA_TRUE &&
        animation->skip_root_geometry == FIGMA_TRUE &&
        node == animation->root)
    {
        skip_own_geometry = FIGMA_TRUE;
    }

    if(animation != NULL &&
        animation->preserve_node_swap_state == FIGMA_TRUE)
    {
        const figma_player_node_swap_t * swap =
            __figma_player_find_swap(player, figma_string_view(&node->id));
        if(swap != NULL)
        {
            const figma_canvas_node_t * swapped =
                figma_document_find_canvas_node(
                    player->document, figma_string_view(&swap->current_node_id));
            if(swapped == NULL)
            {
                FIGMA_DIAGNOSTIC_ADD(
                    &player->diagnostics,
                    FIGMA_DIAGNOSTIC_WARNING,
                    "fig_prototype_target_missing",
                    "SWAP_STATE current node was not found in decoded document",
                    node->id.data);
                return FIGMA_TRUE;
            }
            if(swapped != node)
            {
                return __figma_player_append_canvas_node(
                    player,
                    swapped,
                    parent_opacity,
                    offset_x + node_rect.x - swapped->rect.x,
                    offset_y + node_rect.y - swapped->rect.y,
                    FIGMA_FALSE,
                    animation);
            }
        }
    }

    if(allow_state == FIGMA_TRUE)
    {
        const figma_player_local_animation_t * local =
            __figma_player_find_local_animation(
                player, figma_string_view(&node->id));
        const figma_player_node_swap_t * swap =
            __figma_player_find_swap(player, figma_string_view(&node->id));
        if(local != NULL)
        {
            const figma_canvas_node_t * from_node =
                figma_document_find_canvas_node(
                    player->document, figma_string_view(&local->from_node_id));
            const figma_canvas_node_t * target_node =
                figma_document_find_canvas_node(
                    player->document,
                    figma_string_view(&local->target_node_id));
            const float progress = __figma_player_apply_easing(
                local->easing, local->progress);
            if(from_node == NULL || target_node == NULL)
            {
                return FIGMA_TRUE;
            }
            if(local->smart_animate == FIGMA_TRUE &&
                local->tracks.size != 0u)
            {
                const figma_player_animation_render_context_t source_context = {
                    .tracks = &local->tracks,
                    .root = from_node,
                    .progress = progress,
                    .smart_animate = FIGMA_TRUE,
                    .target_pass = FIGMA_FALSE,
                    .skip_root_geometry = FIGMA_TRUE};
                const figma_player_animation_render_context_t target_context = {
                    .tracks = &local->tracks,
                    .root = target_node,
                    .progress = progress,
                    .smart_animate = FIGMA_TRUE,
                    .target_pass = FIGMA_TRUE};
                return __figma_player_append_canvas_node(
                           player,
                           from_node,
                           parent_opacity,
                           offset_x + node_rect.x - from_node->rect.x,
                           offset_y + node_rect.y - from_node->rect.y,
                           FIGMA_FALSE,
                           &source_context) == FIGMA_TRUE &&
                    __figma_player_append_canvas_node(
                        player,
                        target_node,
                        parent_opacity,
                        offset_x + node_rect.x - target_node->rect.x,
                        offset_y + node_rect.y - target_node->rect.y,
                        FIGMA_FALSE,
                        &target_context) == FIGMA_TRUE;
            }
            return __figma_player_append_canvas_node(
                       player,
                       from_node,
                       parent_opacity * (1.0f - progress),
                       offset_x + node_rect.x - from_node->rect.x,
                       offset_y + node_rect.y - from_node->rect.y,
                       FIGMA_FALSE,
                       NULL) == FIGMA_TRUE &&
                __figma_player_append_canvas_node(
                    player,
                    target_node,
                    parent_opacity * progress,
                    offset_x + node_rect.x - target_node->rect.x,
                    offset_y + node_rect.y - target_node->rect.y,
                    FIGMA_FALSE,
                    NULL) == FIGMA_TRUE;
        }
        if(swap != NULL)
        {
            const figma_canvas_node_t * swapped =
                figma_document_find_canvas_node(
                    player->document, figma_string_view(&swap->current_node_id));
            return swapped == NULL
                ? FIGMA_TRUE
                : __figma_player_append_canvas_node(
                      player,
                      swapped,
                      parent_opacity,
                      offset_x + node_rect.x - swapped->rect.x,
                      offset_y + node_rect.y - swapped->rect.y,
                      FIGMA_FALSE,
                      animation);
        }
    }

    opacity = parent_opacity * __figma_player_clamp01(node_opacity);
    if(opacity <= 0.001f)
    {
        return FIGMA_TRUE;
    }
    if(node->mask == FIGMA_TRUE)
    {
        FIGMA_DIAGNOSTIC_ADD(
            &player->diagnostics,
            FIGMA_DIAGNOSTIC_WARNING,
            "fig_mask_unsupported",
            "Decoded mask node is not rendered until mask/clip composition is implemented",
            node->id.data);
        return FIGMA_TRUE;
    }

    rect = (figma_rectf_t){
        node_rect.x + offset_x,
        node_rect.y + offset_y,
        node_rect.w,
        node_rect.h};
    use_primitive = __figma_player_is_primitive(node->type);
    has_fill_paths = __figma_player_has_decoded_paths(&node->fill_geometry);
    has_stroke_paths = __figma_player_has_decoded_paths(&node->stroke_geometry);
    use_fill_paths = has_fill_paths == FIGMA_TRUE &&
            use_primitive == FIGMA_FALSE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
    use_stroke_paths = has_stroke_paths == FIGMA_TRUE &&
            use_primitive == FIGMA_FALSE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
    if(use_fill_paths == FIGMA_TRUE &&
        __figma_player_has_compound_paths(&node->fill_geometry) == FIGMA_TRUE)
    {
        use_fill_paths = FIGMA_FALSE;
        FIGMA_DIAGNOSTIC_ADD(
            &player->diagnostics,
            FIGMA_DIAGNOSTIC_WARNING,
            "fig_path_compound_fill_unsupported",
            "Decoded fillGeometry has multiple contours; compound winding/hole triangulation is not implemented",
            node->id.data);
    }
    render_own_geometry =
        (node->has_vector_data == FIGMA_FALSE &&
            node->type != FIGMA_CANVAS_NODE_VECTOR) ||
            has_fill_paths == FIGMA_TRUE || has_stroke_paths == FIGMA_TRUE ||
            use_primitive == FIGMA_TRUE
        ? FIGMA_TRUE
        : FIGMA_FALSE;
    if(skip_own_geometry == FIGMA_TRUE)
    {
        render_own_geometry = FIGMA_FALSE;
    }

    if(render_own_geometry == FIGMA_TRUE &&
        node->blend_mode != FIGMA_CANVAS_BLEND_UNSUPPORTED)
    {
        if(node->type != FIGMA_CANVAS_NODE_TEXT)
        {
            const figma_bool_t path_paints =
                use_fill_paths == FIGMA_TRUE &&
                    __figma_player_has_path_paints(&node->fill_geometry) ==
                        FIGMA_TRUE
                ? FIGMA_TRUE
                : FIGMA_FALSE;

            if(path_paints == FIGMA_TRUE)
            {
                figma_array_t grouped_pairs;
                figma_bool_t groupable =
                    node->blend_mode != FIGMA_CANVAS_BLEND_NORMAL &&
                        node->blend_mode != FIGMA_CANVAS_BLEND_PASS_THROUGH
                    ? FIGMA_TRUE
                    : FIGMA_FALSE;
                figma_array_init(
                    &grouped_pairs, sizeof(figma_path_paint_pair_t));
                for(index = 0u;
                    index != node->fill_geometry.size;
                    ++index)
                {
                    const figma_canvas_path_t * path =
                        FIGMA_ARRAY_CONST_PTR(
                            figma_canvas_path_t,
                            &node->fill_geometry,
                            index);
                    const figma_array_t * paints =
                        path->paints.size != 0u ? &path->paints : &node->fills;
                    size_t paint_index;
                    for(paint_index = 0u;
                        paint_index != paints->size;
                        ++paint_index)
                    {
                        const figma_canvas_paint_t * paint =
                            FIGMA_ARRAY_CONST_PTR(
                                figma_canvas_paint_t, paints, paint_index);
                        if(paint->visible == FIGMA_FALSE)
                        {
                            continue;
                        }
                        if(paint->type != FIGMA_CANVAS_PAINT_SOLID ||
                            (paint->blend_mode != FIGMA_CANVAS_BLEND_NORMAL &&
                                paint->blend_mode !=
                                    FIGMA_CANVAS_BLEND_PASS_THROUGH))
                        {
                            groupable = FIGMA_FALSE;
                        }
                        if(groupable == FIGMA_TRUE)
                        {
                            const figma_path_paint_pair_t pair = {path, paint};
                            if(figma_array_push_copy(
                                   player->memory,
                                   &grouped_pairs,
                                   &pair) == FIGMA_FALSE)
                            {
                                figma_array_destroy(
                                    player->memory, &grouped_pairs, NULL);
                                return FIGMA_FALSE;
                            }
                        }
                    }
                }

                if(groupable == FIGMA_TRUE && grouped_pairs.size > 1u)
                {
                    figma_render_command_t * fill = figma_render_list_add(
                        &player->render_list, FIGMA_RENDER_COMMAND_MESH);
                    const figma_path_paint_pair_t * first =
                        FIGMA_ARRAY_CONST_PTR(
                            figma_path_paint_pair_t, &grouped_pairs, 0u);
                    if(fill == NULL ||
                        __figma_player_init_command(
                            player,
                            fill,
                            "figma.path_fill_group",
                            node,
                            rect) == FIGMA_FALSE)
                    {
                        figma_array_destroy(
                            player->memory, &grouped_pairs, NULL);
                        return FIGMA_FALSE;
                    }
                    fill->color = first->paint->color;
                    fill->shape = __figma_player_render_shape(node->type);
                    fill->corner_radius = node->corner_radius;
                    fill->opacity = 1.0f;
                    __figma_player_apply_node_blend(node, fill);
                    __figma_player_assign_arc(&node_arc_data, fill);
                    if(__figma_player_build_grouped_path_mesh(
                           player, fill, &grouped_pairs, opacity) ==
                        FIGMA_FALSE)
                    {
                        figma_render_list_remove_last(&player->render_list);
                    }
                    else
                    {
                        __figma_player_apply_quad(
                            fill,
                            node_rect,
                            node_quad,
                            FIGMA_TRUE,
                            node->size);
                    }
                }
                else
                {
                    for(index = 0u;
                        index != node->fill_geometry.size;
                        ++index)
                    {
                        const figma_canvas_path_t * path =
                            FIGMA_ARRAY_CONST_PTR(
                                figma_canvas_path_t,
                                &node->fill_geometry,
                                index);
                        const figma_array_t * paints =
                            path->paints.size != 0u
                            ? &path->paints
                            : &node->fills;
                        size_t paint_index;
                        for(paint_index = 0u;
                            paint_index != paints->size;
                            ++paint_index)
                        {
                            if(__figma_player_append_solid_path_fill(
                                   player,
                                   node,
                                   FIGMA_ARRAY_CONST_PTR(
                                       figma_canvas_paint_t,
                                       paints,
                                       paint_index),
                                   path,
                                   rect,
                                   node_rect,
                                   node_quad,
                                   &node_arc_data,
                                   opacity) == FIGMA_FALSE)
                            {
                                figma_array_destroy(
                                    player->memory, &grouped_pairs, NULL);
                                return FIGMA_FALSE;
                            }
                        }
                    }
                }
                figma_array_destroy(player->memory, &grouped_pairs, NULL);
            }
            else
            {
                for(index = 0u; index != node->fills.size; ++index)
                {
                    const figma_canvas_paint_t * paint =
                        FIGMA_ARRAY_CONST_PTR(
                            figma_canvas_paint_t, &node->fills, index);
                    if(paint->visible == FIGMA_FALSE ||
                        paint->type == FIGMA_CANVAS_PAINT_UNSUPPORTED ||
                        paint->blend_mode == FIGMA_CANVAS_BLEND_UNSUPPORTED)
                    {
                        continue;
                    }
                    if(paint->type == FIGMA_CANVAS_PAINT_IMAGE)
                    {
                        const figma_asset_t * asset;
                        figma_string_view_t asset_id =
                            figma_string_view(&paint->asset_id);
                        figma_string_view_t bound_asset_id;
                        figma_render_command_t * image;

                        if(paint->asset_id.size == 0u ||
                            paint->image_scale_mode ==
                                FIGMA_CANVAS_IMAGE_SCALE_TILE ||
                            paint->image_scale_mode ==
                                FIGMA_CANVAS_IMAGE_SCALE_UNKNOWN)
                        {
                            continue;
                        }
                        asset = figma_document_find_asset_internal(
                            player->document, asset_id);
                        if(asset == NULL)
                        {
                            continue;
                        }
                        if(figma_player_resolve_image(
                               player, node, &bound_asset_id) == FIGMA_TRUE)
                        {
                            const figma_asset_t * bound =
                                figma_document_find_asset_internal(
                                    player->document, bound_asset_id);
                            if(bound != NULL)
                            {
                                asset = bound;
                                asset_id = bound_asset_id;
                            }
                        }

                        image = figma_render_list_add(
                            &player->render_list,
                            FIGMA_RENDER_COMMAND_IMAGE);
                        if(image == NULL ||
                            __figma_player_init_command(
                                player,
                                image,
                                "figma.image_fill",
                                node,
                                rect) == FIGMA_FALSE ||
                            figma_string_assign(
                                player->memory,
                                &image->asset_id,
                                asset_id) == FIGMA_FALSE)
                        {
                            return FIGMA_FALSE;
                        }
                        image->shape =
                            __figma_player_render_shape(node->type);
                        image->corner_radius = node->corner_radius;
                        image->opacity =
                            opacity * __figma_player_clamp01(paint->opacity);
                        __figma_player_assign_paint_metadata(paint, image);
                        __figma_player_apply_node_blend(node, image);
                        if(__figma_player_add_quad(
                               player,
                               image,
                               node_rect,
                               node_quad,
                               FIGMA_TRUE,
                               asset) == FIGMA_FALSE)
                        {
                            return FIGMA_FALSE;
                        }
                    }
                    else if(paint->type == FIGMA_CANVAS_PAINT_SOLID &&
                        (use_fill_paths == FIGMA_TRUE ||
                            use_primitive == FIGMA_TRUE))
                    {
                        figma_render_command_t * fill = figma_render_list_add(
                            &player->render_list, FIGMA_RENDER_COMMAND_MESH);
                        figma_bool_t built;
                        if(fill == NULL ||
                            __figma_player_init_command(
                                player,
                                fill,
                                use_fill_paths == FIGMA_TRUE
                                    ? "figma.path_fill"
                                    : "figma.solid_fill",
                                node,
                                rect) == FIGMA_FALSE)
                        {
                            return FIGMA_FALSE;
                        }
                        fill->color = paint->color;
                        fill->shape =
                            __figma_player_render_shape(node->type);
                        fill->corner_radius = node->corner_radius;
                        fill->opacity =
                            opacity * __figma_player_clamp01(paint->opacity);
                        __figma_player_assign_paint_metadata(paint, fill);
                        __figma_player_apply_node_blend(node, fill);
                        __figma_player_assign_arc(&node_arc_data, fill);
                        built = use_fill_paths == FIGMA_TRUE
                            ? __figma_player_build_path_mesh(
                                  player,
                                  fill,
                                  (const figma_canvas_path_t *)
                                      node->fill_geometry.data,
                                  node->fill_geometry.size,
                                  FIGMA_TRUE)
                            : __figma_player_build_shape_mesh(
                                  player,
                                  fill,
                                  FIGMA_TRUE,
                                  FIGMA_CANVAS_STROKE_ALIGN_CENTER);
                        if(built == FIGMA_FALSE)
                        {
                            figma_render_list_remove_last(
                                &player->render_list);
                        }
                        else
                        {
                            __figma_player_apply_quad(
                                fill,
                                node_rect,
                                node_quad,
                                use_fill_paths,
                                node->size);
                        }
                    }
                }
            }

            for(index = 0u; index != node->strokes.size; ++index)
            {
                const figma_canvas_paint_t * paint =
                    FIGMA_ARRAY_CONST_PTR(
                        figma_canvas_paint_t, &node->strokes, index);
                figma_render_command_t * stroke;
                figma_bool_t built;
                if(paint->visible == FIGMA_FALSE ||
                    node->stroke_weight <= 0.001f ||
                    paint->type != FIGMA_CANVAS_PAINT_SOLID ||
                    paint->blend_mode == FIGMA_CANVAS_BLEND_UNSUPPORTED ||
                    (use_primitive == FIGMA_TRUE &&
                        (node->stroke_align ==
                                FIGMA_CANVAS_STROKE_ALIGN_UNSUPPORTED ||
                            node->stroke_cap ==
                                FIGMA_CANVAS_STROKE_CAP_UNSUPPORTED ||
                            node->stroke_join ==
                                FIGMA_CANVAS_STROKE_JOIN_UNSUPPORTED ||
                            node->dash_pattern.size != 0u)) ||
                    (use_stroke_paths == FIGMA_FALSE &&
                        use_primitive == FIGMA_FALSE))
                {
                    continue;
                }
                stroke = figma_render_list_add(
                    &player->render_list, FIGMA_RENDER_COMMAND_MESH);
                if(stroke == NULL ||
                    __figma_player_init_command(
                        player,
                        stroke,
                        use_stroke_paths == FIGMA_TRUE
                            ? "figma.path_stroke"
                            : "figma.stroke",
                        node,
                        rect) == FIGMA_FALSE)
                {
                    return FIGMA_FALSE;
                }
                stroke->color = paint->color;
                stroke->shape = __figma_player_render_shape(node->type);
                stroke->corner_radius = node->corner_radius;
                stroke->stroke_width = node->stroke_weight;
                stroke->opacity =
                    opacity * __figma_player_clamp01(paint->opacity);
                __figma_player_assign_paint_metadata(paint, stroke);
                __figma_player_apply_node_blend(node, stroke);
                __figma_player_assign_arc(&node_arc_data, stroke);
                built = use_stroke_paths == FIGMA_TRUE
                    ? __figma_player_build_path_mesh(
                          player,
                          stroke,
                          (const figma_canvas_path_t *)
                              node->stroke_geometry.data,
                          node->stroke_geometry.size,
                          FIGMA_TRUE)
                    : __figma_player_build_shape_mesh(
                          player, stroke, FIGMA_FALSE, node->stroke_align);
                if(built == FIGMA_FALSE)
                {
                    figma_render_list_remove_last(&player->render_list);
                }
                else
                {
                    __figma_player_apply_quad(
                        stroke,
                        node_rect,
                        node_quad,
                        use_stroke_paths,
                        node->size);
                }
            }
        }

        if(node->type == FIGMA_CANVAS_NODE_TEXT && node->text.size != 0u)
        {
            const figma_canvas_paint_t * text_paint = NULL;
            figma_render_command_t * text;
            figma_binding_value_t bound_text;
            figma_bool_t has_bound_text;

            if(node->text_lines.size == 0u)
            {
                return FIGMA_TRUE;
            }
            for(index = 0u; index != node->fills.size; ++index)
            {
                const figma_canvas_paint_t * paint =
                    FIGMA_ARRAY_CONST_PTR(
                        figma_canvas_paint_t, &node->fills, index);
                if(paint->visible == FIGMA_TRUE)
                {
                    text_paint = paint;
                    break;
                }
            }
            if(text_paint == NULL ||
                text_paint->type != FIGMA_CANVAS_PAINT_SOLID ||
                text_paint->blend_mode ==
                    FIGMA_CANVAS_BLEND_UNSUPPORTED)
            {
                return FIGMA_TRUE;
            }

            text = figma_render_list_add(
                &player->render_list, FIGMA_RENDER_COMMAND_TEXT);
            if(text == NULL ||
                __figma_player_init_command(
                    player, text, "figma.text", node, rect) == FIGMA_FALSE ||
                figma_string_copy(player->memory, &text->text, &node->text) ==
                    FIGMA_FALSE ||
                figma_string_copy(
                    player->memory,
                    &text->font_family,
                    &node->font_family) == FIGMA_FALSE ||
                figma_string_copy(
                    player->memory, &text->font_style, &node->font_style) ==
                    FIGMA_FALSE ||
                figma_string_copy(
                    player->memory,
                    &text->font_postscript_name,
                    &node->font_postscript_name) == FIGMA_FALSE)
            {
                return FIGMA_FALSE;
            }
            text->color = text_paint->color;
            text->font_size = node->font_size;
            text->line_height = node->line_height;
            text->font_weight = __figma_player_font_weight(node);
            text->text_align_horizontal =
                (figma_render_text_align_horizontal_t)
                    node->text_align_horizontal;
            text->text_align_vertical =
                (figma_render_text_align_vertical_t)node->text_align_vertical;
            text->opacity =
                opacity * __figma_player_clamp01(text_paint->opacity);
            __figma_player_assign_paint_metadata(text_paint, text);
            __figma_player_apply_node_blend(node, text);

            for(index = 0u; index != node->text_lines.size; ++index)
            {
                const figma_canvas_text_line_t * source =
                    FIGMA_ARRAY_CONST_PTR(
                        figma_canvas_text_line_t, &node->text_lines, index);
                figma_render_text_line_t * line =
                    (figma_render_text_line_t *)figma_array_push_uninitialized(
                        player->memory, &text->text_lines);
                if(line == NULL)
                {
                    return FIGMA_FALSE;
                }
                memset(line, 0, sizeof(*line));
                figma_string_init(&line->text);
                line->x = source->x;
                line->y = source->y;
                line->width = source->width;
                line->line_height = source->line_height;
                line->line_ascent = source->line_ascent;
                if(figma_string_copy(
                       player->memory, &line->text, &source->text) ==
                    FIGMA_FALSE)
                {
                    return FIGMA_FALSE;
                }
            }

            memset(&bound_text, 0, sizeof(bound_text));
            has_bound_text =
                figma_player_resolve_text(player, node, &bound_text);
            if(has_bound_text == FIGMA_TRUE)
            {
                if(text->text_lines.size != 1u)
                {
                    figma_render_list_remove_last(&player->render_list);
                    text = NULL;
                }
                else
                {
                    figma_string_view_t value = bound_text.string_value;
                    char number[64];
                    if(bound_text.type == FIGMA_BINDING_VALUE_BOOLEAN)
                    {
                        value = figma_string_view_cstr(
                            bound_text.bool_value == FIGMA_TRUE
                                ? "true"
                                : "false");
                    }
                    else if(bound_text.type == FIGMA_BINDING_VALUE_NUMBER)
                    {
                        size_t length = 0u;
                        if(figma_format_double_shortest(
                               bound_text.number_value,
                               number,
                               sizeof(number),
                               &length) == FIGMA_FALSE)
                        {
                            value = figma_string_view_cstr("0");
                        }
                        else
                        {
                            value.data = number;
                            value.size = length;
                        }
                    }
                    if(figma_string_assign(
                           player->memory, &text->text, value) == FIGMA_FALSE ||
                        figma_string_assign(
                            player->memory,
                            &FIGMA_ARRAY_PTR(
                                 figma_render_text_line_t,
                                 &text->text_lines,
                                 0u)
                                 ->text,
                            value) == FIGMA_FALSE)
                    {
                        return FIGMA_FALSE;
                    }
                }
            }
            if(text != NULL &&
                __figma_player_add_quad(
                    player,
                    text,
                    node_rect,
                    node_quad,
                    FIGMA_FALSE,
                    NULL) == FIGMA_FALSE)
            {
                return FIGMA_FALSE;
            }
        }
    }

    if(node->type == FIGMA_CANVAS_NODE_FRAME &&
        node->frame_mask_disabled == FIGMA_FALSE &&
        node->children.size != 0u)
    {
        figma_render_command_t * clip = figma_render_list_add(
            &player->render_list, FIGMA_RENDER_COMMAND_CLIP_BEGIN);
        if(clip == NULL ||
            __figma_player_init_command(
                player, clip, "figma.clip_begin", node, rect) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    __figma_player_assign_render_layer(
        player,
        animation,
        command_begin,
        player->render_list.commands.size);

    index = node->children.size;
    while(index != 0u)
    {
        --index;
        if(__figma_player_append_canvas_node(
               player,
               FIGMA_ARRAY_CONST_PTR(
                   figma_canvas_node_t, &node->children, index),
               opacity,
               offset_x,
               offset_y,
               animation == NULL ? FIGMA_TRUE : FIGMA_FALSE,
               animation) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    closing_begin = player->render_list.commands.size;
    if(node->type == FIGMA_CANVAS_NODE_FRAME &&
        node->frame_mask_disabled == FIGMA_FALSE &&
        node->children.size != 0u)
    {
        figma_render_command_t * clip = figma_render_list_add(
            &player->render_list, FIGMA_RENDER_COMMAND_CLIP_END);
        if(clip == NULL ||
            __figma_player_init_command(
                player, clip, "figma.clip_end", node, rect) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }

    __figma_player_assign_render_layer(
        player,
        animation,
        closing_begin,
        player->render_list.commands.size);

    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static float __figma_player_apply_easing(figma_animation_easing_t easing, float progress)
{
    const float value = __figma_player_clamp01(progress);
    switch(easing)
    {
    case FIGMA_ANIMATION_EASING_LINEAR:
        return value;
    case FIGMA_ANIMATION_EASING_EASE_IN:
        return value * value;
    case FIGMA_ANIMATION_EASING_EASE_OUT:
        return 1.0f - (1.0f - value) * (1.0f - value);
    case FIGMA_ANIMATION_EASING_EASE_IN_OUT:
        return value < 0.5f
            ? 2.0f * value * value
            : 1.0f - powf(-2.0f * value + 2.0f, 2.0f) * 0.5f;
    case FIGMA_ANIMATION_EASING_IN_CUBIC:
        return value * value * value;
    case FIGMA_ANIMATION_EASING_OUT_CUBIC:
        return 1.0f - powf(1.0f - value, 3.0f);
    case FIGMA_ANIMATION_EASING_IN_OUT_CUBIC:
        return value < 0.5f
            ? 4.0f * value * value * value
            : 1.0f - powf(-2.0f * value + 2.0f, 3.0f) * 0.5f;
    case FIGMA_ANIMATION_EASING_UNSUPPORTED:
        break;
    }
    return value;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t __figma_player_build_render_list(figma_player_t * player)
{
    const figma_canvas_node_t * frame;
    figma_render_command_t * fill;
    const float width = __figma_player_maxf(1.0f, player->desc.viewport.width);
    const float height =
        __figma_player_maxf(1.0f, player->desc.viewport.height);
    size_t index;

    figma_render_list_clear(&player->render_list);
    frame = player->current_frame;
    if(frame == NULL)
    {
        FIGMA_DIAGNOSTIC_ADD(
            &player->diagnostics,
            FIGMA_DIAGNOSTIC_WARNING,
            "fig_prototype_start_missing",
            "Decoded prototypeStartNodeID/prototypeStartingPoint was not found; render list is empty",
            NULL);
        return FIGMA_RESULT_OK;
    }

    fill =
        figma_render_list_add(&player->render_list, FIGMA_RENDER_COMMAND_MESH);
    if(fill == NULL ||
        __figma_player_init_command(
            player,
            fill,
            "figma.viewport_fill",
            NULL,
            (figma_rectf_t){0.0f, 0.0f, width, height}) == FIGMA_FALSE)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }
    fill->color = (figma_colorf_t){0.0f, 0.0f, 0.0f, 1.0f};
    fill->opacity = 1.0f;
    if(__figma_player_build_shape_mesh(
           player,
           fill,
           FIGMA_TRUE,
           FIGMA_CANVAS_STROKE_ALIGN_CENTER) == FIGMA_FALSE)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    if(player->animation_state.active == FIGMA_TRUE)
    {
        const figma_canvas_node_t * source = figma_document_find_canvas_node(
            player->document,
            figma_string_view(&player->animation_state.clip.source_frame_id));
        const figma_canvas_node_t * target = figma_document_find_canvas_node(
            player->document,
            figma_string_view(&player->animation_state.clip.target_frame_id));
        const float progress = __figma_player_apply_easing(
            player->animation_state.clip.easing,
            player->animation_state.progress);
        const figma_prototype_transition_type_t transition =
            player->animation_state.clip.transition_type;

        if(source == NULL || target == NULL)
        {
            __figma_player_complete_animation(player);
            frame = player->current_frame;
            if(frame != NULL &&
                __figma_player_append_canvas_node(
                    player,
                    frame,
                    1.0f,
                    -frame->rect.x,
                    -frame->rect.y,
                    FIGMA_TRUE,
                    NULL) == FIGMA_FALSE)
            {
                return FIGMA_RESULT_OUT_OF_MEMORY;
            }
        }
        else if(player->animation_state.clip.easing ==
            FIGMA_ANIMATION_EASING_UNSUPPORTED)
        {
            if(__figma_player_append_canvas_node(
                   player,
                   source,
                   1.0f,
                   -source->rect.x,
                   -source->rect.y,
                   FIGMA_TRUE,
                   NULL) == FIGMA_FALSE)
            {
                return FIGMA_RESULT_OUT_OF_MEMORY;
            }
        }
        else if(transition == FIGMA_PROTOTYPE_TRANSITION_MOVE_IN ||
            transition == FIGMA_PROTOTYPE_TRANSITION_MOVE_OUT ||
            transition == FIGMA_PROTOTYPE_TRANSITION_PUSH ||
            transition == FIGMA_PROTOTYPE_TRANSITION_SLIDE_IN ||
            transition == FIGMA_PROTOTYPE_TRANSITION_SLIDE_OUT)
        {
            float direction_x = 0.0f;
            float direction_y = 0.0f;
            const figma_bool_t move_out =
                transition == FIGMA_PROTOTYPE_TRANSITION_MOVE_OUT ||
                    transition == FIGMA_PROTOTYPE_TRANSITION_SLIDE_OUT
                ? FIGMA_TRUE
                : FIGMA_FALSE;
            const figma_bool_t push =
                transition == FIGMA_PROTOTYPE_TRANSITION_PUSH
                ? FIGMA_TRUE
                : FIGMA_FALSE;

            if(player->animation_state.clip.transition_direction ==
                FIGMA_PROTOTYPE_TRANSITION_DIRECTION_LEFT)
            {
                direction_x = player->desc.viewport.width;
            }
            else if(player->animation_state.clip.transition_direction ==
                FIGMA_PROTOTYPE_TRANSITION_DIRECTION_RIGHT)
            {
                direction_x = -player->desc.viewport.width;
            }
            else if(player->animation_state.clip.transition_direction ==
                FIGMA_PROTOTYPE_TRANSITION_DIRECTION_UP)
            {
                direction_y = player->desc.viewport.height;
            }
            else if(player->animation_state.clip.transition_direction ==
                FIGMA_PROTOTYPE_TRANSITION_DIRECTION_DOWN)
            {
                direction_y = -player->desc.viewport.height;
            }

            if(move_out == FIGMA_TRUE)
            {
                if(__figma_player_append_canvas_node(
                       player,
                       target,
                       1.0f,
                       -target->rect.x,
                       -target->rect.y,
                       FIGMA_TRUE,
                       NULL) == FIGMA_FALSE ||
                    __figma_player_append_canvas_node(
                        player,
                        source,
                        1.0f,
                        -source->rect.x - direction_x * progress,
                        -source->rect.y - direction_y * progress,
                        FIGMA_TRUE,
                        NULL) == FIGMA_FALSE)
                {
                    return FIGMA_RESULT_OUT_OF_MEMORY;
                }
            }
            else
            {
                if(__figma_player_append_canvas_node(
                       player,
                       source,
                       1.0f,
                       -source->rect.x -
                           (push == FIGMA_TRUE
                                   ? direction_x * progress
                                   : 0.0f),
                       -source->rect.y -
                           (push == FIGMA_TRUE
                                   ? direction_y * progress
                                   : 0.0f),
                       FIGMA_TRUE,
                       NULL) == FIGMA_FALSE ||
                    __figma_player_append_canvas_node(
                        player,
                        target,
                        1.0f,
                        -target->rect.x + direction_x * (1.0f - progress),
                        -target->rect.y + direction_y * (1.0f - progress),
                        FIGMA_TRUE,
                        NULL) == FIGMA_FALSE)
                {
                    return FIGMA_RESULT_OUT_OF_MEMORY;
                }
            }
        }
        else if(player->animation_state.clip.smart_animate == FIGMA_TRUE)
        {
            if(player->animation_state.clip.tracks.size == 0u)
            {
                FIGMA_DIAGNOSTIC_ADD(
                    &player->diagnostics,
                    FIGMA_DIAGNOSTIC_WARNING,
                    "fig_smart_animate_match_missing",
                    "Smart Animate has no decoded node id or sibling layer matches in the decoded target frame; visual animation is skipped until transition completes",
                    player->animation_state.clip.source_node_id.data);
                if(__figma_player_append_canvas_node(
                       player,
                       source,
                       1.0f,
                       -source->rect.x,
                       -source->rect.y,
                       FIGMA_TRUE,
                       NULL) == FIGMA_FALSE)
                {
                    return FIGMA_RESULT_OUT_OF_MEMORY;
                }
            }
            else
            {
                const figma_player_animation_render_context_t source_context = {
                    .tracks = &player->animation_state.clip.tracks,
                    .root = source,
                    .progress = progress,
                    .smart_animate = FIGMA_TRUE,
                    .target_pass = FIGMA_FALSE,
                    .skip_root_geometry = FIGMA_TRUE};
                const figma_player_animation_render_context_t target_context = {
                    .tracks = &player->animation_state.clip.tracks,
                    .root = target,
                    .progress = progress,
                    .smart_animate = FIGMA_TRUE,
                    .target_pass = FIGMA_TRUE};
                if(__figma_player_append_canvas_node(
                       player,
                       source,
                       1.0f,
                       -source->rect.x,
                       -source->rect.y,
                       FIGMA_FALSE,
                       &source_context) == FIGMA_FALSE ||
                    __figma_player_append_canvas_node(
                        player,
                        target,
                        1.0f,
                        -target->rect.x,
                        -target->rect.y,
                        FIGMA_FALSE,
                        &target_context) == FIGMA_FALSE)
                {
                    return FIGMA_RESULT_OUT_OF_MEMORY;
                }
            }
        }
        else if(transition == FIGMA_PROTOTYPE_TRANSITION_DISSOLVE)
        {
            figma_array_t used_targets;
            figma_array_t pairs;
            figma_player_animation_render_context_t source_context;
            figma_player_animation_render_context_t target_context;
            figma_bool_t appended;

            figma_array_init(
                &used_targets, sizeof(const figma_canvas_node_t *));
            figma_array_init(
                &pairs, sizeof(figma_player_dissolve_pair_t));
            if(__figma_player_collect_dissolve_pairs(
                   player,
                   source,
                   target,
                   source->rect,
                   target->rect,
                   &used_targets,
                   &pairs) == FIGMA_FALSE)
            {
                figma_array_destroy(player->memory, &used_targets, NULL);
                figma_array_destroy(player->memory, &pairs, NULL);
                return FIGMA_RESULT_OUT_OF_MEMORY;
            }

            memset(&source_context, 0, sizeof(source_context));
            source_context.dissolve_pairs = &pairs;
            source_context.root = source;
            source_context.render_layer_id = 1u;
            source_context.render_layer_opacity = 1.0f - progress;
            source_context.preserve_node_swap_state = FIGMA_TRUE;

            memset(&target_context, 0, sizeof(target_context));
            target_context.dissolve_pairs = &pairs;
            target_context.root = target;
            target_context.render_layer_id = 2u;
            target_context.render_layer_opacity = progress;

            appended = __figma_player_append_canvas_node(
                           player,
                           source,
                           1.0f,
                           -source->rect.x,
                           -source->rect.y,
                           FIGMA_FALSE,
                           &source_context) == FIGMA_TRUE &&
                __figma_player_append_canvas_node(
                    player,
                    target,
                    1.0f,
                    -target->rect.x,
                    -target->rect.y,
                    FIGMA_FALSE,
                    &target_context) == FIGMA_TRUE
                ? FIGMA_TRUE
                : FIGMA_FALSE;
            figma_array_destroy(player->memory, &used_targets, NULL);
            figma_array_destroy(player->memory, &pairs, NULL);
            if(appended == FIGMA_FALSE)
            {
                return FIGMA_RESULT_OUT_OF_MEMORY;
            }
        }
        else if(__figma_player_append_canvas_node(
                    player,
                    source,
                    1.0f,
                    -source->rect.x,
                    -source->rect.y,
                    FIGMA_TRUE,
                    NULL) == FIGMA_FALSE)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }
    }
    else if(__figma_player_append_canvas_node(
                player,
                frame,
                1.0f,
                -frame->rect.x,
                -frame->rect.y,
                FIGMA_TRUE,
                NULL) == FIGMA_FALSE)
    {
        return FIGMA_RESULT_OUT_OF_MEMORY;
    }

    for(index = 0u; index != player->overlay_frames.size; ++index)
    {
        const figma_canvas_node_t * overlay =
            FIGMA_ARRAY_AT(
                const figma_canvas_node_t *, &player->overlay_frames, index);
        if(__figma_player_append_canvas_node(
               player,
               overlay,
               1.0f,
               -overlay->rect.x +
                   (player->desc.viewport.width - overlay->rect.w) * 0.5f,
               -overlay->rect.y +
                   (player->desc.viewport.height - overlay->rect.h) * 0.5f,
               FIGMA_TRUE,
               NULL) == FIGMA_FALSE)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }
    }

    for(index = 0u; index != player->document->bindings.size; ++index)
    {
        const figma_binding_t * binding =
            FIGMA_ARRAY_CONST_PTR(
                figma_binding_t, &player->document->bindings, index);
        if(figma_document_find_canvas_node(
               player->document, figma_string_view(&binding->node_id)) == NULL)
        {
            FIGMA_DIAGNOSTIC_ADD(
                &player->diagnostics,
                FIGMA_DIAGNOSTIC_WARNING,
                "ux_binding_node_missing",
                "Binding target node was not found in decoded document",
                binding->node_id.data);
        }
    }

    for(index = 0u; index != player->hotspots.size; ++index)
    {
        const figma_player_hotspot_t * hotspot =
            FIGMA_ARRAY_CONST_PTR(
                figma_player_hotspot_t, &player->hotspots, index);
        figma_render_command_t * command = figma_render_list_add(
            &player->render_list, FIGMA_RENDER_COMMAND_DEBUG_HOTSPOT);
        if(command == NULL ||
            __figma_player_assign_command_string(
                player, &command->id, "figma.hotspot") == FIGMA_FALSE ||
            figma_string_copy(
                player->memory,
                &command->node_id,
                &hotspot->node_id) == FIGMA_FALSE ||
            figma_string_copy(
                player->memory,
                &command->text,
                &hotspot->action_id) == FIGMA_FALSE)
        {
            return FIGMA_RESULT_OUT_OF_MEMORY;
        }
        command->rect = hotspot->rect;
        command->color = (figma_colorf_t){1.0f, 0.62f, 0.12f, 0.32f};
    }

    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
figma_result_t figma_player_rebuild_render_list(figma_player_t * player)
{
    figma_render_list_t previous;
    figma_result_t result;

    previous = player->render_list;
    figma_render_list_init(&player->render_list, player->memory);
    result = __figma_player_build_render_list(player);
    if(result == FIGMA_RESULT_OK)
    {
        figma_render_list_destroy(&previous);
        return FIGMA_RESULT_OK;
    }

    figma_render_list_destroy(&player->render_list);
    player->render_list = previous;
    return result;
}

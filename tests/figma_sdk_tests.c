#include "figma/figma.h"

#include "figma_internal.h"
#include "figma_model.h"
#include "figma_player.h"

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_CHECK(condition) \
    do \
    { \
        if(!(condition)) \
        { \
            fprintf(stderr, "%s:%d: check failed: %s\n", \
                __FILE__, __LINE__, #condition); \
            return 0; \
        } \
    } while(0)

typedef struct test_allocator_state
{
    size_t calls;
    size_t active;
    size_t fail_at;
} test_allocator_state_t;

typedef struct test_callback_state
{
    uint32_t trigger_count;
    uint32_t action_count;
    uint32_t frame_change_count;
    uint32_t binding_count;
} test_callback_state_t;

//////////////////////////////////////////////////////////////////////////
static void * FIGMA_CALL __test_alloc(size_t size, void * user_data)
{
    test_allocator_state_t * state =
        (test_allocator_state_t *)user_data;
    void * value;
    ++state->calls;
    if(state->fail_at != 0u && state->calls >= state->fail_at)
    {
        return NULL;
    }
    value = malloc(size);
    if(value != NULL)
    {
        ++state->active;
    }
    return value;
}

//////////////////////////////////////////////////////////////////////////
static void * FIGMA_CALL __test_realloc(void * ptr, size_t size, void * user_data)
{
    test_allocator_state_t * state =
        (test_allocator_state_t *)user_data;
    void * value;
    ++state->calls;
    if(state->fail_at != 0u && state->calls >= state->fail_at)
    {
        return NULL;
    }
    value = realloc(ptr, size);
    if(value != NULL && ptr == NULL)
    {
        ++state->active;
    }
    return value;
}

//////////////////////////////////////////////////////////////////////////
static void FIGMA_CALL __test_free(void * ptr, void * user_data)
{
    test_allocator_state_t * state =
        (test_allocator_state_t *)user_data;
    if(ptr != NULL)
    {
        if(state->active == 0u)
        {
            abort();
        }
        --state->active;
        free(ptr);
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __test_assign(figma_memory_t * memory, figma_string_t * target, const char * value)
{
    return figma_string_assign_cstr(memory, target, value);
}

//////////////////////////////////////////////////////////////////////////
static void __test_set_geometry(figma_canvas_node_t * node, figma_rectf_t rect)
{
    node->rect = rect;
    node->size.x = rect.w;
    node->size.y = rect.h;
    node->quad[0] = (figma_vec2f_t){rect.x, rect.y};
    node->quad[1] = (figma_vec2f_t){rect.x + rect.w, rect.y};
    node->quad[2] =
        (figma_vec2f_t){rect.x + rect.w, rect.y + rect.h};
    node->quad[3] = (figma_vec2f_t){rect.x, rect.y + rect.h};
}

//////////////////////////////////////////////////////////////////////////
static figma_canvas_node_t * __test_add_node(figma_document_t * document, figma_canvas_node_t * parent, const char * id, const char * name, figma_canvas_node_type_t type, figma_rectf_t rect)
{
    figma_canvas_node_t * node =
        (figma_canvas_node_t *)figma_array_push_uninitialized(
            document->memory, &parent->children);
    if(node == NULL)
    {
        return NULL;
    }
    figma_canvas_node_init(node);
    node->type = type;
    __test_set_geometry(node, rect);
    if(__test_assign(document->memory, &node->id, id) == FIGMA_FALSE ||
        __test_assign(document->memory, &node->name, name) == FIGMA_FALSE)
    {
        return NULL;
    }
    return node;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __test_add_solid_fill(figma_document_t * document, figma_canvas_node_t * node, figma_colorf_t color)
{
    figma_canvas_paint_t * paint =
        (figma_canvas_paint_t *)figma_array_push_uninitialized(
            document->memory, &node->fills);
    if(paint == NULL)
    {
        return FIGMA_FALSE;
    }
    figma_canvas_paint_init(paint);
    paint->type = FIGMA_CANVAS_PAINT_SOLID;
    paint->blend_mode = FIGMA_CANVAS_BLEND_NORMAL;
    paint->visible = FIGMA_TRUE;
    paint->opacity = 1.0f;
    paint->color = color;
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __test_add_text_line(figma_document_t * document, figma_canvas_node_t * node, const char * text)
{
    figma_canvas_text_line_t * line =
        (figma_canvas_text_line_t *)figma_array_push_uninitialized(
            document->memory, &node->text_lines);
    if(line == NULL)
    {
        return FIGMA_FALSE;
    }
    memset(line, 0, sizeof(*line));
    figma_string_init(&line->text);
    line->width = node->rect.w;
    line->line_height = 20.0f;
    line->line_ascent = 15.0f;
    return __test_assign(document->memory, &line->text, text);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __test_add_navigation(figma_document_t * document, figma_canvas_node_t * node, const char * target_id)
{
    figma_prototype_interaction_t * interaction =
        (figma_prototype_interaction_t *)figma_array_push_uninitialized(
            document->memory, &node->prototype_interactions);
    figma_prototype_action_t * action;
    if(interaction == NULL)
    {
        return FIGMA_FALSE;
    }
    figma_prototype_interaction_init(interaction);
    interaction->event_type = FIGMA_PROTOTYPE_EVENT_CLICK;
    if(__test_assign(document->memory, &interaction->id, "click-a") ==
        FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }

    action = (figma_prototype_action_t *)figma_array_push_uninitialized(
        document->memory, &interaction->actions);
    if(action == NULL)
    {
        return FIGMA_FALSE;
    }
    figma_prototype_action_init(action);
    action->connection_type = FIGMA_PROTOTYPE_CONNECTION_INTERNAL_NODE;
    action->navigation_type = FIGMA_PROTOTYPE_NAVIGATION_NAVIGATE;
    action->transition_type = FIGMA_PROTOTYPE_TRANSITION_SMART_ANIMATE;
    action->transition_easing = FIGMA_ANIMATION_EASING_LINEAR;
    action->transition_duration = 1.0f;
    action->smart_animate = FIGMA_TRUE;
    return __test_assign(
        document->memory, &action->target_node_id, target_id);
}

//////////////////////////////////////////////////////////////////////////
static figma_document_t * __test_build_document(figma_runtime_t * runtime)
{
    figma_document_t * document =
        (figma_document_t *)figma_memory_allocate(
            &runtime->memory, sizeof(*document));
    figma_canvas_node_t * frame_a;
    figma_canvas_node_t * frame_b;
    figma_canvas_node_t * button_a;
    figma_canvas_node_t * button_b;
    figma_canvas_node_t * label;
    figma_canvas_node_t * persistent_a;
    figma_canvas_node_t * persistent_b;
    const figma_colorf_t blue = {0.1f, 0.3f, 0.8f, 1.0f};
    const figma_colorf_t white = {1.0f, 1.0f, 1.0f, 1.0f};

    if(document == NULL)
    {
        return NULL;
    }
    figma_document_init(document, runtime, &runtime->memory);
    document->has_canvas_root = FIGMA_TRUE;
    document->canvas_root.type = FIGMA_CANVAS_NODE_DOCUMENT;
    if(__test_assign(
           document->memory, &document->canvas_root.id, "root") ==
            FIGMA_FALSE ||
        __test_assign(
            document->memory,
            &document->prototype_start_node_id,
            "frame-a") == FIGMA_FALSE ||
        figma_array_reserve(
            document->memory, &document->canvas_root.children, 2u) ==
            FIGMA_FALSE)
    {
        figma_document_destroy(document);
        return NULL;
    }

    frame_a = __test_add_node(
        document,
        &document->canvas_root,
        "frame-a",
        "Frame A",
        FIGMA_CANVAS_NODE_FRAME,
        (figma_rectf_t){0.0f, 0.0f, 100.0f, 100.0f});
    if(frame_a == NULL ||
        figma_array_reserve(document->memory, &frame_a->children, 3u) ==
            FIGMA_FALSE)
    {
        figma_document_destroy(document);
        return NULL;
    }
    frame_a->has_prototype_starting_point = FIGMA_TRUE;
    button_a = __test_add_node(
        document,
        frame_a,
        "button-a",
        "Button",
        FIGMA_CANVAS_NODE_RECTANGLE,
        (figma_rectf_t){10.0f, 10.0f, 30.0f, 20.0f});
    label = __test_add_node(
        document,
        frame_a,
        "label",
        "Label",
        FIGMA_CANVAS_NODE_TEXT,
        (figma_rectf_t){10.0f, 50.0f, 60.0f, 20.0f});
    persistent_a = __test_add_node(
        document,
        frame_a,
        "persistent",
        "Persistent",
        FIGMA_CANVAS_NODE_RECTANGLE,
        (figma_rectf_t){70.0f, 70.0f, 20.0f, 20.0f});
    if(button_a == NULL || label == NULL || persistent_a == NULL ||
        __test_add_solid_fill(document, button_a, blue) == FIGMA_FALSE ||
        __test_add_solid_fill(document, persistent_a, white) == FIGMA_FALSE ||
        __test_add_navigation(document, button_a, "frame-b") == FIGMA_FALSE ||
        __test_assign(document->memory, &label->text, "0") == FIGMA_FALSE ||
        __test_assign(document->memory, &label->font_family, "Test") ==
            FIGMA_FALSE ||
        __test_add_text_line(document, label, "0") == FIGMA_FALSE ||
        __test_add_solid_fill(document, label, white) == FIGMA_FALSE)
    {
        figma_document_destroy(document);
        return NULL;
    }
    label->font_size = 16.0f;
    label->line_height = 20.0f;

    frame_b = __test_add_node(
        document,
        &document->canvas_root,
        "frame-b",
        "Frame B",
        FIGMA_CANVAS_NODE_FRAME,
        (figma_rectf_t){200.0f, 0.0f, 100.0f, 100.0f});
    if(frame_b == NULL ||
        figma_array_reserve(document->memory, &frame_b->children, 2u) ==
            FIGMA_FALSE)
    {
        figma_document_destroy(document);
        return NULL;
    }
    button_b = __test_add_node(
        document,
        frame_b,
        "button-b",
        "Button",
        FIGMA_CANVAS_NODE_RECTANGLE,
        (figma_rectf_t){240.0f, 15.0f, 45.0f, 25.0f});
    persistent_b = __test_add_node(
        document,
        frame_b,
        "persistent",
        "Persistent",
        FIGMA_CANVAS_NODE_RECTANGLE,
        (figma_rectf_t){270.0f, 70.0f, 20.0f, 20.0f});
    if(button_b == NULL || persistent_b == NULL ||
        __test_add_solid_fill(document, button_b, blue) == FIGMA_FALSE ||
        __test_add_solid_fill(document, persistent_b, white) == FIGMA_FALSE)
    {
        figma_document_destroy(document);
        return NULL;
    }

    return document;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t FIGMA_CALL __test_route_trigger(void * user_data, const figma_trigger_event_t * event)
{
    test_callback_state_t * state =
        (test_callback_state_t *)user_data;
    if(event == NULL || event->source_node_id.size == 0u)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    ++state->trigger_count;
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static figma_result_t FIGMA_CALL __test_route_action(void * user_data, const figma_action_event_t * event, figma_action_response_t * response)
{
    test_callback_state_t * state =
        (test_callback_state_t *)user_data;
    if(event == NULL || response == NULL)
    {
        return FIGMA_RESULT_INVALID_ARGUMENT;
    }
    ++state->action_count;
    response->result = FIGMA_ACTION_RESULT_ALLOW_DEFAULT;
    return FIGMA_RESULT_OK;
}

//////////////////////////////////////////////////////////////////////////
static void FIGMA_CALL __test_frame_changed(void * user_data, figma_string_view_t previous_frame_id, figma_string_view_t current_frame_id)
{
    test_callback_state_t * state =
        (test_callback_state_t *)user_data;
    if(previous_frame_id.size != 0u && current_frame_id.size != 0u)
    {
        ++state->frame_change_count;
    }
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t FIGMA_CALL __test_get_binding(void * user_data, figma_string_view_t key, figma_binding_value_t * value)
{
    test_callback_state_t * state =
        (test_callback_state_t *)user_data;
    if(value == NULL ||
        figma_string_view_equal_cstr(key, "number") == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }
    ++state->binding_count;
    memset(value, 0, sizeof(*value));
    value->type = FIGMA_BINDING_VALUE_NUMBER;
    value->number_value = 0.10000000000000001;
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t FIGMA_CALL __test_is_binding_dirty(void * user_data, figma_string_view_t key)
{
    (void)user_data;
    return figma_string_view_equal_cstr(key, "number");
}

//////////////////////////////////////////////////////////////////////////
static int __test_version_and_allocator(void)
{
    test_allocator_state_t state = {0u, 0u, 0u};
    figma_runtime_desc_t desc;
    figma_runtime_t * runtime = (figma_runtime_t *)(uintptr_t)1u;
    figma_document_t * document = (figma_document_t *)(uintptr_t)1u;
    void * aligned;
    figma_result_t result;
    const unsigned char invalid_fig[] = {0u};

    memset(&desc, 0, sizeof(desc));
    desc.allocator.alloc = &__test_alloc;
    desc.allocator.realloc = &__test_realloc;
    desc.allocator.free = &__test_free;
    desc.allocator.user_data = &state;

    result = figma_runtime_create(
        FIGMA_SDK_VERSION - 1u, &desc, &runtime);
    TEST_CHECK(result == FIGMA_RESULT_VERSION_MISMATCH);
    TEST_CHECK(runtime == NULL);
    TEST_CHECK(state.calls == 0u);

    state.fail_at = 1u;
    result = figma_runtime_create(FIGMA_SDK_VERSION, &desc, &runtime);
    TEST_CHECK(result == FIGMA_RESULT_OUT_OF_MEMORY);
    TEST_CHECK(runtime == NULL);
    TEST_CHECK(state.active == 0u);

    state.fail_at = 0u;
    result = figma_runtime_create(FIGMA_SDK_VERSION, &desc, &runtime);
    TEST_CHECK(result == FIGMA_RESULT_OK);
    TEST_CHECK(runtime != NULL);
    TEST_CHECK(state.active == 1u);

    aligned = figma_memory_allocate(&runtime->memory, sizeof(double));
    TEST_CHECK(aligned != NULL);
    TEST_CHECK((uintptr_t)aligned % _Alignof(double) == 0u);
    figma_memory_deallocate(&runtime->memory, aligned);

    state.fail_at = state.calls + 1u;
    result = figma_runtime_load_document_from_fig_data(
        runtime,
        invalid_fig,
        sizeof(invalid_fig),
        NULL,
        &document);
    TEST_CHECK(result != FIGMA_RESULT_OK);
    TEST_CHECK(document == NULL);
    TEST_CHECK(figma_memory_allocate(&runtime->memory, 16u) == NULL);
    state.fail_at = 0u;

    figma_runtime_destroy(runtime);
    TEST_CHECK(state.active == 0u);
    TEST_CHECK(figma_runtime_create(
        FIGMA_SDK_VERSION, NULL, NULL) == FIGMA_RESULT_INVALID_ARGUMENT);
    TEST_CHECK(figma_player_update(NULL, 0.0f) ==
        FIGMA_RESULT_INVALID_ARGUMENT);
    return 1;
}

//////////////////////////////////////////////////////////////////////////
static int __test_locale_formatter(void)
{
    char first[64];
    char second[64];
    char previous[128];
    size_t first_size = 0u;
    size_t second_size = 0u;
    const char * current = setlocale(LC_NUMERIC, NULL);
    const char * locale_names[] = {
        "fr_FR.UTF-8", "de_DE.UTF-8", "ru_RU.UTF-8", ""};
    size_t index;

    previous[0] = '\0';
    if(current != NULL)
    {
        (void)snprintf(previous, sizeof(previous), "%s", current);
    }
    TEST_CHECK(setlocale(LC_NUMERIC, "C") != NULL);
    TEST_CHECK(figma_format_double_shortest(
        0.10000000000000001, first, sizeof(first), &first_size) ==
        FIGMA_TRUE);
    TEST_CHECK(strcmp(first, "0.1") == 0);

    for(index = 0u;
        index != sizeof(locale_names) / sizeof(locale_names[0]);
        ++index)
    {
        if(setlocale(LC_NUMERIC, locale_names[index]) != NULL)
        {
            TEST_CHECK(figma_format_double_shortest(
                0.10000000000000001,
                second,
                sizeof(second),
                &second_size) == FIGMA_TRUE);
            TEST_CHECK(first_size == second_size);
            TEST_CHECK(memcmp(first, second, first_size) == 0);
            break;
        }
    }
    if(previous[0] != '\0')
    {
        (void)setlocale(LC_NUMERIC, previous);
    }
    return 1;
}

//////////////////////////////////////////////////////////////////////////
static int __test_player_and_atomic_ux(void)
{
    static const char valid_ux[] =
        "{\"bindings\":[{\"nodeId\":\"label\",\"key\":\"number\","
        "\"property\":\"text\"}]}";
    static const char invalid_ux[] = "{\"bindings\":{}}";
    figma_runtime_t * runtime = NULL;
    figma_document_t * document = NULL;
    figma_player_t * player = NULL;
    figma_player_desc_t player_desc;
    figma_action_router_t router;
    figma_data_context_t data_context;
    test_callback_state_t callbacks;
    figma_inspection_document_desc_t document_desc;
    figma_pointer_event_t pointer;
    figma_input_dispatch_result_t dispatch;
    const figma_render_list_t * render_list;
    uint32_t command_count;
    uint32_t index;
    figma_bool_t found_text = FIGMA_FALSE;
    figma_bool_t found_persistent_track = FIGMA_FALSE;

    TEST_CHECK(figma_runtime_create(
        FIGMA_SDK_VERSION, NULL, &runtime) == FIGMA_RESULT_OK);
    document = __test_build_document(runtime);
    TEST_CHECK(document != NULL);

    TEST_CHECK(figma_document_load_ux(
        document,
        (figma_string_view_t){valid_ux, sizeof(valid_ux) - 1u}) ==
        FIGMA_RESULT_OK);
    TEST_CHECK(figma_inspection_get_document(
        document, &document_desc) == FIGMA_RESULT_OK);
    TEST_CHECK(document_desc.binding_count == 1u);
    TEST_CHECK(figma_document_load_ux(
        document,
        (figma_string_view_t){invalid_ux, sizeof(invalid_ux) - 1u}) ==
        FIGMA_RESULT_PARSE_FAILED);
    TEST_CHECK(figma_inspection_get_document(
        document, &document_desc) == FIGMA_RESULT_OK);
    TEST_CHECK(document_desc.binding_count == 1u);

    memset(&player_desc, 0, sizeof(player_desc));
    player_desc.viewport.width = 100.0f;
    player_desc.viewport.height = 100.0f;
    player_desc.viewport.scale = 1.0f;
    TEST_CHECK(figma_runtime_create_player(
        runtime, document, &player_desc, &player) == FIGMA_RESULT_OK);

    memset(&callbacks, 0, sizeof(callbacks));
    memset(&router, 0, sizeof(router));
    router.user_data = &callbacks;
    router.route_trigger = &__test_route_trigger;
    router.route_action = &__test_route_action;
    router.on_frame_changed = &__test_frame_changed;
    TEST_CHECK(figma_player_set_action_router(player, &router) ==
        FIGMA_RESULT_OK);

    memset(&data_context, 0, sizeof(data_context));
    data_context.user_data = &callbacks;
    data_context.get_binding_value = &__test_get_binding;
    data_context.is_binding_dirty = &__test_is_binding_dirty;
    TEST_CHECK(figma_player_set_data_context(player, &data_context) ==
        FIGMA_RESULT_OK);
    TEST_CHECK(figma_player_update(player, 0.0f) == FIGMA_RESULT_OK);
    TEST_CHECK(callbacks.binding_count != 0u);

    render_list = figma_player_get_render_list(player);
    TEST_CHECK(render_list != NULL);
    command_count = figma_render_list_get_batch_count(render_list);
    TEST_CHECK(command_count > 1u);
    for(index = 0u; index != command_count; ++index)
    {
        figma_inspection_render_command_desc_t command;
        TEST_CHECK(figma_inspection_get_render_command(
            render_list, index, &command) == FIGMA_RESULT_OK);
        if(command.type == FIGMA_RENDER_COMMAND_TEXT &&
            figma_string_view_equal_cstr(command.node_id, "label") ==
                FIGMA_TRUE)
        {
            TEST_CHECK(figma_string_view_equal_cstr(
                command.text, "0.1") == FIGMA_TRUE);
            found_text = FIGMA_TRUE;
        }
    }
    TEST_CHECK(found_text == FIGMA_TRUE);

    memset(&pointer, 0, sizeof(pointer));
    memset(&dispatch, 0, sizeof(dispatch));
    pointer.type = FIGMA_POINTER_EVENT_DOWN;
    pointer.pointer_id = 7u;
    pointer.x = 20.0f;
    pointer.y = 20.0f;
    pointer.button = FIGMA_POINTER_BUTTON_LEFT;
    TEST_CHECK(figma_player_input_pointer(
        player, &pointer, &dispatch) == FIGMA_RESULT_OK);
    TEST_CHECK(dispatch.hit == FIGMA_TRUE);
    TEST_CHECK(dispatch.captured == FIGMA_TRUE);

    pointer.type = FIGMA_POINTER_EVENT_UP;
    TEST_CHECK(figma_player_input_pointer(
        player, &pointer, &dispatch) == FIGMA_RESULT_OK);
    TEST_CHECK(dispatch.handled == FIGMA_TRUE);
    TEST_CHECK(callbacks.trigger_count != 0u);
    TEST_CHECK(callbacks.action_count != 0u);
    TEST_CHECK(player->animation_state.active == FIGMA_TRUE);
    TEST_CHECK(player->animation_state.clip.tracks.size >= 2u);
    for(index = 0u;
        index != player->animation_state.clip.tracks.size;
        ++index)
    {
        const figma_animation_track_t * track =
            FIGMA_ARRAY_CONST_PTR(
                figma_animation_track_t,
                &player->animation_state.clip.tracks,
                index);
        if(track->type == FIGMA_ANIMATION_TRACK_RECT &&
            figma_string_equal_view(
                &track->node_id,
                figma_string_view_cstr("persistent")) == FIGMA_TRUE)
        {
            TEST_CHECK(track->persistent == FIGMA_TRUE);
            TEST_CHECK(track->persistent_source_node != NULL);
            found_persistent_track = FIGMA_TRUE;
        }
    }
    TEST_CHECK(found_persistent_track == FIGMA_TRUE);

    TEST_CHECK(figma_player_update(player, 0.5f) == FIGMA_RESULT_OK);
    TEST_CHECK(player->animation_state.active == FIGMA_TRUE);
    render_list = figma_player_get_render_list(player);
    command_count = figma_render_list_get_batch_count(render_list);
    TEST_CHECK(command_count > 1u);
    {
        uint32_t persistent_count = 0u;
        for(index = 0u; index != command_count; ++index)
        {
            figma_inspection_render_command_desc_t command;
            TEST_CHECK(figma_inspection_get_render_command(
                render_list, index, &command) == FIGMA_RESULT_OK);
            if(figma_string_view_equal_cstr(
                   command.node_id, "persistent") == FIGMA_TRUE)
            {
                ++persistent_count;
                TEST_CHECK(command.render_layer_id == 0u);
                TEST_CHECK(command.opacity > 0.99f);
            }
        }
        TEST_CHECK(persistent_count == 1u);
    }

    TEST_CHECK(figma_player_update(player, 0.6f) == FIGMA_RESULT_OK);
    TEST_CHECK(player->animation_state.active == FIGMA_FALSE);
    TEST_CHECK(figma_string_equal_view(
        &player->current_frame_id,
        figma_string_view_cstr("frame-b")) == FIGMA_TRUE);
    TEST_CHECK(callbacks.frame_change_count != 0u);
    TEST_CHECK(figma_player_go_back(player) == FIGMA_RESULT_OK);
    TEST_CHECK(figma_string_equal_view(
        &player->current_frame_id,
        figma_string_view_cstr("frame-a")) == FIGMA_TRUE);

    {
        figma_canvas_node_t * button =
            (figma_canvas_node_t *)(void *)figma_document_find_canvas_node(
                document, figma_string_view_cstr("button-a"));
        figma_prototype_interaction_t * interaction;
        figma_prototype_action_t * action;
        figma_bool_t layer_one = FIGMA_FALSE;
        figma_bool_t layer_two = FIGMA_FALSE;
        uint32_t persistent_count = 0u;

        TEST_CHECK(button != NULL);
        interaction = FIGMA_ARRAY_PTR(
            figma_prototype_interaction_t,
            &button->prototype_interactions,
            0u);
        action = FIGMA_ARRAY_PTR(
            figma_prototype_action_t, &interaction->actions, 0u);
        action->transition_type = FIGMA_PROTOTYPE_TRANSITION_DISSOLVE;
        action->smart_animate = FIGMA_FALSE;

        pointer.type = FIGMA_POINTER_EVENT_DOWN;
        TEST_CHECK(figma_player_input_pointer(
            player, &pointer, &dispatch) == FIGMA_RESULT_OK);
        pointer.type = FIGMA_POINTER_EVENT_UP;
        TEST_CHECK(figma_player_input_pointer(
            player, &pointer, &dispatch) == FIGMA_RESULT_OK);
        TEST_CHECK(player->animation_state.active == FIGMA_TRUE);
        TEST_CHECK(figma_player_update(player, 0.5f) == FIGMA_RESULT_OK);

        render_list = figma_player_get_render_list(player);
        command_count = figma_render_list_get_batch_count(render_list);
        for(index = 0u; index != command_count; ++index)
        {
            figma_inspection_render_command_desc_t command;
            TEST_CHECK(figma_inspection_get_render_command(
                render_list, index, &command) == FIGMA_RESULT_OK);
            if(command.render_layer_id == 1u)
            {
                layer_one = FIGMA_TRUE;
                TEST_CHECK(command.render_layer_opacity > 0.49f);
                TEST_CHECK(command.render_layer_opacity < 0.51f);
            }
            else if(command.render_layer_id == 2u)
            {
                layer_two = FIGMA_TRUE;
                TEST_CHECK(command.render_layer_opacity > 0.49f);
                TEST_CHECK(command.render_layer_opacity < 0.51f);
            }
            if(figma_string_view_equal_cstr(
                   command.node_id, "persistent") == FIGMA_TRUE)
            {
                ++persistent_count;
                TEST_CHECK(command.render_layer_id == 0u);
            }
        }
        TEST_CHECK(layer_one == FIGMA_TRUE);
        TEST_CHECK(layer_two == FIGMA_TRUE);
        TEST_CHECK(persistent_count == 1u);
        TEST_CHECK(figma_player_update(player, 0.6f) == FIGMA_RESULT_OK);
        TEST_CHECK(figma_player_go_back(player) == FIGMA_RESULT_OK);
    }

    TEST_CHECK(figma_player_open_overlay(
        player, figma_string_view_cstr("frame-b")) == FIGMA_RESULT_OK);
    TEST_CHECK(figma_player_close_overlay(player) == FIGMA_RESULT_OK);
    TEST_CHECK(figma_player_restart(player) == FIGMA_RESULT_OK);

    figma_player_destroy(player);
    figma_document_destroy(document);
    figma_runtime_destroy(runtime);
    return 1;
}

//////////////////////////////////////////////////////////////////////////
int main(void)
{
    if(__test_version_and_allocator() == 0 ||
        __test_locale_formatter() == 0 ||
        __test_player_and_atomic_ux() == 0)
    {
        return EXIT_FAILURE;
    }

    puts("figma_sdk_tests: OK");
    return EXIT_SUCCESS;
}

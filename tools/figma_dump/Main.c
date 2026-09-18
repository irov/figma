#include "figma/figma.h"
#include "figma_graphics_object.h"
#include "figma_inspection.h"

#include <float.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct dump_options
{
    const char * fig_path;
    const char * sidecar_path;
    const char * find_text;
    const char * node_id;
    const char * frame_id;
    figma_bool_t render_list;
    figma_bool_t animations;
} dump_options_t;

typedef struct byte_buffer
{
    uint8_t * data;
    size_t size;
} byte_buffer_t;

typedef struct node_list
{
    const figma_inspection_node_t ** data;
    size_t size;
    size_t capacity;
} node_list_t;

//////////////////////////////////////////////////////////////////////////
static figma_string_view_t __dump_cstr(const char * value)
{
    figma_string_view_t view;
    view.data = value;
    view.size = value != NULL ? strlen(value) : 0u;
    return view;
}

//////////////////////////////////////////////////////////////////////////
static const char * __dump_result_name(figma_result_t result)
{
    switch(result)
    {
    case FIGMA_RESULT_OK:
        return "Ok";
    case FIGMA_RESULT_INVALID_ARGUMENT:
        return "InvalidArgument";
    case FIGMA_RESULT_OUT_OF_MEMORY:
        return "OutOfMemory";
    case FIGMA_RESULT_IO_FAILED:
        return "IoFailed";
    case FIGMA_RESULT_PARSE_FAILED:
        return "ParseFailed";
    case FIGMA_RESULT_UNSUPPORTED_FORMAT:
        return "UnsupportedFormat";
    case FIGMA_RESULT_MISSING_ENTRY:
        return "MissingEntry";
    case FIGMA_RESULT_NOT_FOUND:
        return "NotFound";
    case FIGMA_RESULT_INVALID_STATE:
        return "InvalidState";
    case FIGMA_RESULT_VERSION_MISMATCH:
        return "VersionMismatch";
    }
    return "Unknown";
}

//////////////////////////////////////////////////////////////////////////
static void __dump_usage(const char * program)
{
    fprintf(
        stderr,
        "Usage: %s <file.fig> [--sidecar file.ux.json] [--find text] [--node id] [--frame id] [--render-list] [--animations]\n",
        program);
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __dump_parse_options(int argc, char ** argv, dump_options_t * options)
{
    int index;
    memset(options, 0, sizeof(*options));
    if(argc < 2)
    {
        return FIGMA_FALSE;
    }
    options->fig_path = argv[1];
    for(index = 2; index != argc; ++index)
    {
        const char * argument = argv[index];
        if(strcmp(argument, "--sidecar") == 0)
        {
            if(index + 1 == argc)
            {
                return FIGMA_FALSE;
            }
            options->sidecar_path = argv[++index];
        }
        else if(strcmp(argument, "--find") == 0)
        {
            if(index + 1 == argc)
            {
                return FIGMA_FALSE;
            }
            options->find_text = argv[++index];
        }
        else if(strcmp(argument, "--node") == 0)
        {
            if(index + 1 == argc)
            {
                return FIGMA_FALSE;
            }
            options->node_id = argv[++index];
        }
        else if(strcmp(argument, "--frame") == 0)
        {
            if(index + 1 == argc)
            {
                return FIGMA_FALSE;
            }
            options->frame_id = argv[++index];
            options->render_list = FIGMA_TRUE;
        }
        else if(strcmp(argument, "--render-list") == 0)
        {
            options->render_list = FIGMA_TRUE;
        }
        else if(strcmp(argument, "--animations") == 0)
        {
            options->animations = FIGMA_TRUE;
        }
        else
        {
            return FIGMA_FALSE;
        }
    }
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __dump_read_file(const char * path, byte_buffer_t * buffer)
{
    FILE * file;
    long length;
    size_t read_size;

    memset(buffer, 0, sizeof(*buffer));
    if(path == NULL)
    {
        return FIGMA_FALSE;
    }
    file = fopen(path, "rb");
    if(file == NULL)
    {
        return FIGMA_FALSE;
    }
    if(fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return FIGMA_FALSE;
    }
    length = ftell(file);
    if(length < 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return FIGMA_FALSE;
    }
    if(length == 0)
    {
        fclose(file);
        return FIGMA_TRUE;
    }
    buffer->data = (uint8_t *)malloc((size_t)length);
    if(buffer->data == NULL)
    {
        fclose(file);
        return FIGMA_FALSE;
    }
    read_size = fread(buffer->data, 1u, (size_t)length, file);
    fclose(file);
    if(read_size != (size_t)length)
    {
        free(buffer->data);
        memset(buffer, 0, sizeof(*buffer));
        return FIGMA_FALSE;
    }
    buffer->size = read_size;
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static void __dump_indent(int indent)
{
    int index;
    for(index = 0; index != indent; ++index)
    {
        printf(" ");
    }
}

//////////////////////////////////////////////////////////////////////////
static void __dump_json_string(figma_string_view_t value)
{
    size_t index;
    printf("\"");
    for(index = 0u; index != value.size; ++index)
    {
        const unsigned char character = (unsigned char)value.data[index];
        switch(character)
        {
        case '\\':
            printf("\\\\");
            break;
        case '"':
            printf("\\\"");
            break;
        case '\b':
            printf("\\b");
            break;
        case '\f':
            printf("\\f");
            break;
        case '\n':
            printf("\\n");
            break;
        case '\r':
            printf("\\r");
            break;
        case '\t':
            printf("\\t");
            break;
        default:
            if(character < 0x20u)
            {
                printf("\\u%04x", (unsigned int)character);
            }
            else
            {
                printf("%c", (int)character);
            }
            break;
        }
    }
    printf("\"");
}

//////////////////////////////////////////////////////////////////////////
static void __dump_json_key(int indent, const char * key)
{
    __dump_indent(indent);
    __dump_json_string(__dump_cstr(key));
    printf(": ");
}

//////////////////////////////////////////////////////////////////////////
static const char * __dump_node_type(figma_canvas_node_type_t type)
{
    static const char * names[] = {
        "Unknown",
        "Document",
        "Canvas",
        "Frame",
        "Group",
        "Rectangle",
        "RoundedRectangle",
        "Ellipse",
        "Text",
        "Vector"};
    return (unsigned int)type < sizeof(names) / sizeof(names[0])
        ? names[(unsigned int)type]
        : "Unknown";
}

//////////////////////////////////////////////////////////////////////////
static const char * __dump_paint_type(figma_canvas_paint_type_t type)
{
    if(type == FIGMA_CANVAS_PAINT_SOLID)
    {
        return "Solid";
    }
    if(type == FIGMA_CANVAS_PAINT_IMAGE)
    {
        return "Image";
    }
    return "Unsupported";
}

//////////////////////////////////////////////////////////////////////////
static const char * __dump_blend_mode(unsigned int mode)
{
    static const char * names[] = {
        "PassThrough",
        "Normal",
        "Multiply",
        "Screen",
        "Overlay",
        "Darken",
        "Lighten",
        "ColorDodge",
        "ColorBurn",
        "SoftLight",
        "HardLight",
        "Difference",
        "Exclusion",
        "Hue",
        "Saturation",
        "Color",
        "Luminosity",
        "Unsupported"};
    return mode < sizeof(names) / sizeof(names[0])
        ? names[mode]
        : "Unsupported";
}

//////////////////////////////////////////////////////////////////////////
static const char * __dump_image_scale(unsigned int mode)
{
    static const char * names[] = {
        "Stretch", "Fit", "Fill", "Tile", "Unknown"};
    return mode < sizeof(names) / sizeof(names[0])
        ? names[mode]
        : "Unknown";
}

//////////////////////////////////////////////////////////////////////////
static const char * __dump_stroke_align(figma_canvas_stroke_align_t align)
{
    static const char * names[] = {
        "Center", "Inside", "Outside", "Unsupported"};
    return (unsigned int)align < sizeof(names) / sizeof(names[0])
        ? names[(unsigned int)align]
        : "Unsupported";
}

//////////////////////////////////////////////////////////////////////////
static const char * __dump_event(figma_prototype_event_type_t type)
{
    static const char * names[] = {
        "Click",
        "HoverEnter",
        "HoverLeave",
        "Press",
        "PointerDown",
        "PointerUp",
        "AfterTimeout",
        "KeyDown",
        "Unsupported"};
    return (unsigned int)type < sizeof(names) / sizeof(names[0])
        ? names[(unsigned int)type]
        : "Unsupported";
}

//////////////////////////////////////////////////////////////////////////
static const char * __dump_connection(figma_prototype_connection_type_t type)
{
    static const char * names[] = {
        "None", "InternalNode", "Back", "Close", "Unsupported"};
    return (unsigned int)type < sizeof(names) / sizeof(names[0])
        ? names[(unsigned int)type]
        : "Unsupported";
}

//////////////////////////////////////////////////////////////////////////
static const char * __dump_navigation(figma_prototype_navigation_type_t type)
{
    static const char * names[] = {
        "Navigate", "Overlay", "Swap", "ScrollTo", "Unsupported"};
    return (unsigned int)type < sizeof(names) / sizeof(names[0])
        ? names[(unsigned int)type]
        : "Unsupported";
}

//////////////////////////////////////////////////////////////////////////
static const char * __dump_transition(figma_prototype_transition_type_t type)
{
    static const char * names[] = {
        "Instant",
        "Dissolve",
        "SmartAnimate",
        "MoveIn",
        "MoveOut",
        "Push",
        "SlideIn",
        "SlideOut",
        "Unsupported"};
    return (unsigned int)type < sizeof(names) / sizeof(names[0])
        ? names[(unsigned int)type]
        : "Unsupported";
}

//////////////////////////////////////////////////////////////////////////
static const char * __dump_direction(figma_prototype_transition_direction_t type)
{
    static const char * names[] = {
        "None", "Left", "Right", "Up", "Down", "Unsupported"};
    return (unsigned int)type < sizeof(names) / sizeof(names[0])
        ? names[(unsigned int)type]
        : "Unsupported";
}

//////////////////////////////////////////////////////////////////////////
static const char * __dump_easing(figma_animation_easing_t type)
{
    static const char * names[] = {
        "Linear",
        "EaseIn",
        "EaseOut",
        "EaseInOut",
        "InCubic",
        "OutCubic",
        "InOutCubic",
        "Unsupported"};
    return (unsigned int)type < sizeof(names) / sizeof(names[0])
        ? names[(unsigned int)type]
        : "Unsupported";
}

//////////////////////////////////////////////////////////////////////////
static const char * __dump_diagnostic_severity(figma_diagnostic_severity_t severity)
{
    static const char * names[] = {"Info", "Warning", "Error"};
    return (unsigned int)severity < sizeof(names) / sizeof(names[0])
        ? names[(unsigned int)severity]
        : "Unknown";
}

//////////////////////////////////////////////////////////////////////////
static const char * __dump_render_command(uint32_t type)
{
    static const char * names[] = {
        "Fill",
        "Stroke",
        "Image",
        "Text",
        "Mesh",
        "ClipBegin",
        "ClipEnd",
        "DebugHotspot"};
    return type < sizeof(names) / sizeof(names[0])
        ? names[type]
        : "Unknown";
}

//////////////////////////////////////////////////////////////////////////
static void __dump_rect(figma_rectf_t rect)
{
    printf(
        "{\"x\": %.3f, \"y\": %.3f, \"w\": %.3f, \"h\": %.3f}",
        rect.x,
        rect.y,
        rect.w,
        rect.h);
}

//////////////////////////////////////////////////////////////////////////
static void __dump_color(figma_colorf_t color)
{
    printf(
        "{\"r\": %.6f, \"g\": %.6f, \"b\": %.6f, \"a\": %.6f}",
        color.r,
        color.g,
        color.b,
        color.a);
}

//////////////////////////////////////////////////////////////////////////
static void __dump_float_array(const float * values, size_t count)
{
    size_t index;
    printf("[");
    for(index = 0u; index != count; ++index)
    {
        if(index != 0u)
        {
            printf(", ");
        }
        printf("%.6f", values[index]);
    }
    printf("]");
}

//////////////////////////////////////////////////////////////////////////
static void __dump_paint(const figma_inspection_paint_desc_t * paint, int indent)
{
    printf("{\n");
    __dump_json_key(indent + 2, "type");
    __dump_json_string(__dump_cstr(__dump_paint_type(paint->type)));
    printf(",\n");
    __dump_json_key(indent + 2, "rawType");
    __dump_json_string(paint->raw_type);
    printf(",\n");
    __dump_json_key(indent + 2, "blendMode");
    __dump_json_string(__dump_cstr(__dump_blend_mode((unsigned int)paint->blend_mode)));
    printf(",\n");
    __dump_json_key(indent + 2, "rawBlendMode");
    __dump_json_string(paint->raw_blend_mode);
    printf(",\n");
    __dump_json_key(indent + 2, "imageScaleMode");
    __dump_json_string(
        __dump_cstr(__dump_image_scale((unsigned int)paint->image_scale_mode)));
    printf(",\n");
    __dump_json_key(indent + 2, "hasTransform");
    printf("%s,\n", paint->has_transform ? "true" : "false");
    __dump_json_key(indent + 2, "transform");
    __dump_float_array(paint->transform, 6u);
    printf(",\n");
    __dump_json_key(indent + 2, "visible");
    printf("%s,\n", paint->visible ? "true" : "false");
    __dump_json_key(indent + 2, "opacity");
    printf("%.6f,\n", paint->opacity);
    __dump_json_key(indent + 2, "color");
    __dump_color(paint->color);
    if(paint->asset_id.size != 0u)
    {
        printf(",\n");
        __dump_json_key(indent + 2, "assetId");
        __dump_json_string(paint->asset_id);
    }
    printf(",\n");
    __dump_json_key(indent + 2, "hasFilterColorAdjust");
    printf("%s,\n", paint->has_filter_color_adjust ? "true" : "false");
    __dump_json_key(indent + 2, "filterColorAdjust");
    __dump_float_array(paint->filter_color_adjust, 8u);
    printf(",\n");
    __dump_json_key(indent + 2, "hasPaintFilter");
    printf("%s,\n", paint->has_paint_filter ? "true" : "false");
    __dump_json_key(indent + 2, "paintFilter");
    __dump_float_array(paint->paint_filter, 10u);
    printf(",\n");
    __dump_json_key(indent + 2, "originalImageSize");
    printf(
        "{\"w\": %u, \"h\": %u}\n",
        paint->original_image_width,
        paint->original_image_height);
    __dump_indent(indent);
    printf("}");
}

//////////////////////////////////////////////////////////////////////////
static void __dump_node_paints(const figma_inspection_node_t * node, uint32_t count, figma_bool_t strokes, int indent)
{
    uint32_t index;
    printf("[");
    if(count != 0u)
    {
        printf("\n");
        for(index = 0u; index != count; ++index)
        {
            figma_inspection_paint_desc_t paint;
            figma_result_t result = strokes
                ? figma_inspection_get_stroke(node, index, &paint)
                : figma_inspection_get_fill(node, index, &paint);
            __dump_indent(indent + 2);
            if(result == FIGMA_RESULT_OK)
            {
                __dump_paint(&paint, indent + 2);
            }
            else
            {
                printf("null");
            }
            printf("%s\n", index + 1u != count ? "," : "");
        }
        __dump_indent(indent);
    }
    printf("]");
}

//////////////////////////////////////////////////////////////////////////
static void __dump_path_paints(const figma_inspection_path_t * path, uint32_t count, int indent)
{
    uint32_t index;
    printf("[");
    if(count != 0u)
    {
        printf("\n");
        for(index = 0u; index != count; ++index)
        {
            figma_inspection_paint_desc_t paint;
            __dump_indent(indent + 2);
            if(figma_inspection_get_path_paint(path, index, &paint) ==
                FIGMA_RESULT_OK)
            {
                __dump_paint(&paint, indent + 2);
            }
            else
            {
                printf("null");
            }
            printf("%s\n", index + 1u != count ? "," : "");
        }
        __dump_indent(indent);
    }
    printf("]");
}

//////////////////////////////////////////////////////////////////////////
static void __dump_path(const figma_inspection_path_t * path, int indent)
{
    figma_inspection_path_desc_t desc;
    float min_x = FLT_MAX;
    float min_y = FLT_MAX;
    float max_x = -FLT_MAX;
    float max_y = -FLT_MAX;
    figma_bool_t has_point = FIGMA_FALSE;
    uint32_t index;

    if(path == NULL ||
        figma_inspection_get_path(path, &desc) != FIGMA_RESULT_OK)
    {
        printf("null");
        return;
    }

    for(index = 0u; index != desc.command_count; ++index)
    {
        figma_canvas_path_command_t command;
        figma_vec2f_t points[3];
        size_t count = 0u;
        size_t point_index;
        if(figma_inspection_get_path_command(path, index, &command) !=
            FIGMA_RESULT_OK)
        {
            continue;
        }
        if(command.type == FIGMA_CANVAS_PATH_MOVE_TO ||
            command.type == FIGMA_CANVAS_PATH_LINE_TO)
        {
            points[count++] = command.p0;
        }
        else if(command.type == FIGMA_CANVAS_PATH_QUADRATIC_TO)
        {
            points[count++] = command.p0;
            points[count++] = command.p1;
        }
        else if(command.type == FIGMA_CANVAS_PATH_CUBIC_TO)
        {
            points[count++] = command.p0;
            points[count++] = command.p1;
            points[count++] = command.p2;
        }
        for(point_index = 0u; point_index != count; ++point_index)
        {
            if(points[point_index].x < min_x)
            {
                min_x = points[point_index].x;
            }
            if(points[point_index].y < min_y)
            {
                min_y = points[point_index].y;
            }
            if(points[point_index].x > max_x)
            {
                max_x = points[point_index].x;
            }
            if(points[point_index].y > max_y)
            {
                max_y = points[point_index].y;
            }
            has_point = FIGMA_TRUE;
        }
    }

    printf("{\n");
    __dump_json_key(indent + 2, "styleId");
    printf("%u,\n", desc.style_id);
    __dump_json_key(indent + 2, "commandsBlob");
    printf("%u,\n", desc.commands_blob);
    __dump_json_key(indent + 2, "commandsDecoded");
    printf("%s,\n", desc.commands_decoded ? "true" : "false");
    __dump_json_key(indent + 2, "commands");
    printf("%u,\n", desc.command_count);
    __dump_json_key(indent + 2, "bounds");
    if(has_point)
    {
        printf(
            "{\"x\": %.6f, \"y\": %.6f, \"w\": %.6f, \"h\": %.6f},\n",
            min_x,
            min_y,
            max_x - min_x,
            max_y - min_y);
    }
    else
    {
        printf("null,\n");
    }
    __dump_json_key(indent + 2, "paints");
    __dump_path_paints(path, desc.paint_count, indent + 2);
    printf("\n");
    __dump_indent(indent);
    printf("}");
}

//////////////////////////////////////////////////////////////////////////
static void __dump_node_paths(const figma_inspection_node_t * node, uint32_t count, figma_bool_t strokes, int indent)
{
    uint32_t index;
    printf("[");
    if(count != 0u)
    {
        printf("\n");
        for(index = 0u; index != count; ++index)
        {
            const figma_inspection_path_t * path = strokes
                ? figma_inspection_get_stroke_path(node, index)
                : figma_inspection_get_fill_path(node, index);
            __dump_indent(indent + 2);
            __dump_path(path, indent + 2);
            printf("%s\n", index + 1u != count ? "," : "");
        }
        __dump_indent(indent);
    }
    printf("]");
}

//////////////////////////////////////////////////////////////////////////
static void __dump_node(const figma_inspection_node_t * node, int indent)
{
    figma_inspection_node_desc_t desc;
    if(node == NULL ||
        figma_inspection_get_node(node, &desc) != FIGMA_RESULT_OK)
    {
        printf("null");
        return;
    }

    printf("{\n");
    __dump_json_key(indent + 2, "id");
    __dump_json_string(desc.id);
    printf(",\n");
    __dump_json_key(indent + 2, "name");
    __dump_json_string(desc.name);
    printf(",\n");
    __dump_json_key(indent + 2, "type");
    __dump_json_string(__dump_cstr(__dump_node_type(desc.type)));
    printf(",\n");
    __dump_json_key(indent + 2, "rect");
    __dump_rect(desc.rect);
    printf(",\n");
    __dump_json_key(indent + 2, "opacity");
    printf("%.6f,\n", desc.opacity);
    __dump_json_key(indent + 2, "blendMode");
    __dump_json_string(__dump_cstr(__dump_blend_mode((unsigned int)desc.blend_mode)));
    printf(",\n");
    __dump_json_key(indent + 2, "rawBlendMode");
    __dump_json_string(desc.raw_blend_mode);
    printf(",\n");
    __dump_json_key(indent + 2, "visible");
    printf("%s,\n", desc.visible ? "true" : "false");
    __dump_json_key(indent + 2, "mask");
    printf("%s,\n", desc.mask ? "true" : "false");
    __dump_json_key(indent + 2, "cornerRadius");
    printf("%.6f,\n", desc.corner_radius);
    __dump_json_key(indent + 2, "strokeWeight");
    printf("%.6f,\n", desc.stroke_weight);
    __dump_json_key(indent + 2, "strokeAlign");
    __dump_json_string(__dump_cstr(__dump_stroke_align(desc.stroke_align)));
    printf(",\n");
    __dump_json_key(indent + 2, "text");
    __dump_json_string(desc.text);
    printf(",\n");
    __dump_json_key(indent + 2, "fontFamily");
    __dump_json_string(desc.font_family);
    printf(",\n");
    __dump_json_key(indent + 2, "fontStyle");
    __dump_json_string(desc.font_style);
    printf(",\n");
    __dump_json_key(indent + 2, "fontPostscriptName");
    __dump_json_string(desc.font_postscript_name);
    printf(",\n");
    __dump_json_key(indent + 2, "fontSize");
    printf("%.6f,\n", desc.font_size);
    __dump_json_key(indent + 2, "fontWeight");
    printf("%d,\n", desc.font_weight);
    __dump_json_key(indent + 2, "symbolId");
    __dump_json_string(desc.symbol_id);
    printf(",\n");
    __dump_json_key(indent + 2, "fillStyleNodeId");
    __dump_json_string(desc.fill_style_node_id);
    printf(",\n");
    __dump_json_key(indent + 2, "strokeFillStyleNodeId");
    __dump_json_string(desc.stroke_fill_style_node_id);
    printf(",\n");
    __dump_json_key(indent + 2, "fills");
    __dump_node_paints(node, desc.fill_count, FIGMA_FALSE, indent + 2);
    printf(",\n");
    __dump_json_key(indent + 2, "strokes");
    __dump_node_paints(node, desc.stroke_count, FIGMA_TRUE, indent + 2);
    printf(",\n");
    __dump_json_key(indent + 2, "fillGeometry");
    __dump_node_paths(
        node, desc.fill_geometry_count, FIGMA_FALSE, indent + 2);
    printf(",\n");
    __dump_json_key(indent + 2, "strokeGeometry");
    __dump_node_paths(
        node, desc.stroke_geometry_count, FIGMA_TRUE, indent + 2);
    printf(",\n");
    __dump_json_key(indent + 2, "children");
    printf("%u\n", desc.child_count);
    __dump_indent(indent);
    printf("}");
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __dump_view_equal_cstr(figma_string_view_t view, const char * value)
{
    const size_t size = value != NULL ? strlen(value) : 0u;
    return view.size == size &&
            (size == 0u || memcmp(view.data, value, size) == 0)
        ? FIGMA_TRUE
        : FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __dump_view_contains(figma_string_view_t value, const char * needle)
{
    const size_t needle_size = needle != NULL ? strlen(needle) : 0u;
    size_t index;
    if(needle_size == 0u || needle_size > value.size)
    {
        return FIGMA_FALSE;
    }
    for(index = 0u; index + needle_size <= value.size; ++index)
    {
        if(memcmp(value.data + index, needle, needle_size) == 0)
        {
            return FIGMA_TRUE;
        }
    }
    return FIGMA_FALSE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __dump_node_list_push(node_list_t * list, const figma_inspection_node_t * node)
{
    if(list->size == list->capacity)
    {
        const size_t capacity =
            list->capacity == 0u ? 16u : list->capacity * 2u;
        const figma_inspection_node_t ** data =
            (const figma_inspection_node_t **)realloc(
                list->data, capacity * sizeof(*data));
        if(data == NULL)
        {
            return FIGMA_FALSE;
        }
        list->data = data;
        list->capacity = capacity;
    }
    list->data[list->size++] = node;
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static figma_bool_t __dump_walk_nodes(const figma_inspection_node_t * node, const dump_options_t * options, size_t * node_count, size_t * frame_count, node_list_t * matches)
{
    figma_inspection_node_desc_t desc;
    uint32_t index;
    figma_bool_t match = FIGMA_FALSE;
    if(node == NULL ||
        figma_inspection_get_node(node, &desc) != FIGMA_RESULT_OK)
    {
        return FIGMA_TRUE;
    }
    ++*node_count;
    if(desc.type == FIGMA_CANVAS_NODE_FRAME)
    {
        ++*frame_count;
    }
    if(options->node_id != NULL &&
        __dump_view_equal_cstr(desc.id, options->node_id))
    {
        match = FIGMA_TRUE;
    }
    else if(options->find_text != NULL &&
        (__dump_view_contains(desc.id, options->find_text) ||
            __dump_view_contains(desc.name, options->find_text) ||
            __dump_view_contains(desc.text, options->find_text)))
    {
        match = FIGMA_TRUE;
    }
    if(match && __dump_node_list_push(matches, node) == FIGMA_FALSE)
    {
        return FIGMA_FALSE;
    }
    for(index = 0u; index != desc.child_count; ++index)
    {
        if(__dump_walk_nodes(
               figma_inspection_get_child(node, index),
               options,
               node_count,
               frame_count,
               matches) == FIGMA_FALSE)
        {
            return FIGMA_FALSE;
        }
    }
    return FIGMA_TRUE;
}

//////////////////////////////////////////////////////////////////////////
static void __dump_string_array_interaction(const figma_inspection_interaction_t * interaction, uint32_t count, int indent)
{
    uint32_t index;
    printf("[");
    if(count != 0u)
    {
        printf("\n");
        for(index = 0u; index != count; ++index)
        {
            figma_string_view_t field = {NULL, 0u};
            figma_inspection_get_interaction_unsupported_field(
                interaction, index, &field);
            __dump_indent(indent + 2);
            __dump_json_string(field);
            printf("%s\n", index + 1u != count ? "," : "");
        }
        __dump_indent(indent);
    }
    printf("]");
}

//////////////////////////////////////////////////////////////////////////
static void __dump_string_array_action(const figma_inspection_interaction_t * interaction, uint32_t action_index, uint32_t count, int indent)
{
    uint32_t index;
    printf("[");
    if(count != 0u)
    {
        printf("\n");
        for(index = 0u; index != count; ++index)
        {
            figma_string_view_t field = {NULL, 0u};
            figma_inspection_get_action_unsupported_field(
                interaction, action_index, index, &field);
            __dump_indent(indent + 2);
            __dump_json_string(field);
            printf("%s\n", index + 1u != count ? "," : "");
        }
        __dump_indent(indent);
    }
    printf("]");
}

//////////////////////////////////////////////////////////////////////////
static void __dump_prototype_action(const figma_inspection_interaction_t * interaction, uint32_t action_index, int indent)
{
    figma_inspection_action_desc_t action;
    if(figma_inspection_get_interaction_action(
           interaction, action_index, &action) != FIGMA_RESULT_OK)
    {
        printf("null");
        return;
    }
    printf("{\n");
    __dump_json_key(indent + 2, "targetNodeId");
    __dump_json_string(action.target_node_id);
    printf(",\n");
    __dump_json_key(indent + 2, "connectionType");
    __dump_json_string(__dump_cstr(__dump_connection(action.connection_type)));
    printf(",\n");
    __dump_json_key(indent + 2, "rawConnectionType");
    __dump_json_string(action.raw_connection_type);
    printf(",\n");
    __dump_json_key(indent + 2, "navigationType");
    __dump_json_string(__dump_cstr(__dump_navigation(action.navigation_type)));
    printf(",\n");
    __dump_json_key(indent + 2, "rawNavigationType");
    __dump_json_string(action.raw_navigation_type);
    printf(",\n");
    __dump_json_key(indent + 2, "transitionType");
    __dump_json_string(__dump_cstr(__dump_transition(action.transition_type)));
    printf(",\n");
    __dump_json_key(indent + 2, "rawTransitionType");
    __dump_json_string(action.raw_transition_type);
    printf(",\n");
    __dump_json_key(indent + 2, "transitionDirection");
    __dump_json_string(__dump_cstr(__dump_direction(action.transition_direction)));
    printf(",\n");
    __dump_json_key(indent + 2, "rawTransitionDirection");
    __dump_json_string(action.raw_transition_direction);
    printf(",\n");
    __dump_json_key(indent + 2, "transitionEasing");
    __dump_json_string(__dump_cstr(__dump_easing(action.transition_easing)));
    printf(",\n");
    __dump_json_key(indent + 2, "rawTransitionEasing");
    __dump_json_string(action.raw_transition_easing);
    printf(",\n");
    __dump_json_key(indent + 2, "transitionDuration");
    printf("%.6f,\n", action.transition_duration);
    __dump_json_key(indent + 2, "smartAnimate");
    printf("%s,\n", action.smart_animate ? "true" : "false");
    __dump_json_key(indent + 2, "transitionPreserveScroll");
    printf("%s,\n", action.transition_preserve_scroll ? "true" : "false");
    __dump_json_key(indent + 2, "transitionResetVideoPosition");
    printf("%s,\n", action.transition_reset_video_position ? "true" : "false");
    __dump_json_key(indent + 2, "hasEasingFunction");
    printf("%s,\n", action.has_easing_function ? "true" : "false");
    __dump_json_key(indent + 2, "unsupportedFields");
    __dump_string_array_action(
        interaction,
        action_index,
        action.unsupported_field_count,
        indent + 2);
    printf("\n");
    __dump_indent(indent);
    printf("}");
}

//////////////////////////////////////////////////////////////////////////
static void __dump_prototype_interaction(const figma_inspection_interaction_t * interaction, int indent)
{
    figma_inspection_interaction_desc_t desc;
    uint32_t index;
    if(interaction == NULL ||
        figma_inspection_get_interaction_desc(interaction, &desc) !=
            FIGMA_RESULT_OK)
    {
        printf("null");
        return;
    }
    printf("{\n");
    __dump_json_key(indent + 2, "id");
    __dump_json_string(desc.id);
    printf(",\n");
    __dump_json_key(indent + 2, "eventType");
    __dump_json_string(__dump_cstr(__dump_event(desc.event_type)));
    printf(",\n");
    __dump_json_key(indent + 2, "rawEventType");
    __dump_json_string(desc.raw_event_type);
    printf(",\n");
    __dump_json_key(indent + 2, "transitionTimeout");
    printf("%.6f,\n", desc.transition_timeout);
    __dump_json_key(indent + 2, "unsupportedFields");
    __dump_string_array_interaction(
        interaction, desc.unsupported_field_count, indent + 2);
    printf(",\n");
    __dump_json_key(indent + 2, "actions");
    printf("[");
    if(desc.action_count != 0u)
    {
        printf("\n");
        for(index = 0u; index != desc.action_count; ++index)
        {
            __dump_indent(indent + 4);
            __dump_prototype_action(interaction, index, indent + 4);
            printf("%s\n", index + 1u != desc.action_count ? "," : "");
        }
        __dump_indent(indent + 2);
    }
    printf("]\n");
    __dump_indent(indent);
    printf("}");
}

//////////////////////////////////////////////////////////////////////////
static void __dump_animation_node_recursive(const figma_inspection_node_t * node, int indent, figma_bool_t * first)
{
    figma_inspection_node_desc_t desc;
    uint32_t index;
    if(node == NULL ||
        figma_inspection_get_node(node, &desc) != FIGMA_RESULT_OK)
    {
        return;
    }
    if(desc.interaction_count != 0u)
    {
        if(*first == FIGMA_FALSE)
        {
            printf(",\n");
        }
        *first = FIGMA_FALSE;
        __dump_indent(indent);
        printf("{\n");
        __dump_json_key(indent + 2, "nodeId");
        __dump_json_string(desc.id);
        printf(",\n");
        __dump_json_key(indent + 2, "name");
        __dump_json_string(desc.name);
        printf(",\n");
        __dump_json_key(indent + 2, "type");
        __dump_json_string(__dump_cstr(__dump_node_type(desc.type)));
        printf(",\n");
        __dump_json_key(indent + 2, "rect");
        __dump_rect(desc.rect);
        printf(",\n");
        __dump_json_key(indent + 2, "interactions");
        printf("[\n");
        for(index = 0u; index != desc.interaction_count; ++index)
        {
            __dump_indent(indent + 4);
            __dump_prototype_interaction(
                figma_inspection_get_interaction(node, index), indent + 4);
            printf("%s\n", index + 1u != desc.interaction_count ? "," : "");
        }
        __dump_indent(indent + 2);
        printf("]\n");
        __dump_indent(indent);
        printf("}");
    }
    for(index = 0u; index != desc.child_count; ++index)
    {
        __dump_animation_node_recursive(
            figma_inspection_get_child(node, index), indent, first);
    }
}

//////////////////////////////////////////////////////////////////////////
static void __dump_animation_nodes(const figma_inspection_node_t * root, int indent)
{
    printf("[");
    if(root != NULL)
    {
        figma_bool_t first = FIGMA_TRUE;
        printf("\n");
        __dump_animation_node_recursive(root, indent + 2, &first);
        if(first == FIGMA_FALSE)
        {
            printf("\n");
        }
        __dump_indent(indent);
    }
    printf("]");
}

//////////////////////////////////////////////////////////////////////////
static void __dump_diagnostics(const figma_diagnostics_t * diagnostics, int indent)
{
    const uint32_t count = figma_diagnostics_get_count(diagnostics);
    uint32_t index;
    printf("[");
    if(count != 0u)
    {
        printf("\n");
        for(index = 0u; index != count; ++index)
        {
            figma_diagnostic_t diagnostic;
            if(figma_diagnostics_get(diagnostics, index, &diagnostic) !=
                FIGMA_RESULT_OK)
            {
                continue;
            }
            __dump_indent(indent + 2);
            printf("{\"severity\": ");
            __dump_json_string(
                __dump_cstr(__dump_diagnostic_severity(diagnostic.severity)));
            printf(", \"code\": ");
            __dump_json_string(diagnostic.code);
            printf(", \"nodeId\": ");
            __dump_json_string(diagnostic.node_id);
            printf(", \"message\": ");
            __dump_json_string(diagnostic.message);
            printf("}%s\n", index + 1u != count ? "," : "");
        }
        __dump_indent(indent);
    }
    printf("]");
}

//////////////////////////////////////////////////////////////////////////
static void __dump_render_list(const figma_render_list_t * render_list, int indent)
{
    const uint32_t count = figma_render_list_get_batch_count(render_list);
    uint32_t index;
    printf("[");
    if(count != 0u)
    {
        printf("\n");
        for(index = 0u; index != count; ++index)
        {
            figma_inspection_render_command_desc_t command;
            uint32_t line_index;
            figma_inspection_get_render_command(render_list, index, &command);
            __dump_indent(indent + 2);
            printf("{\"type\": ");
            __dump_json_string(__dump_cstr(__dump_render_command(command.type)));
            printf(", \"id\": ");
            __dump_json_string(command.id);
            printf(", \"nodeId\": ");
            __dump_json_string(command.node_id);
            printf(", \"rect\": ");
            __dump_rect(command.rect);
            printf(", \"color\": ");
            __dump_color(command.color);
            printf(", \"opacity\": %.6f", command.opacity);
            printf(", \"blendMode\": ");
            __dump_json_string(
                __dump_cstr(__dump_blend_mode((unsigned int)command.blend_mode)));
            printf(", \"renderLayerId\": %u", command.render_layer_id);
            printf(
                ", \"renderLayerOpacity\": %.6f",
                command.render_layer_opacity);
            printf(", \"strokeWidth\": %.6f", command.stroke_width);
            printf(", \"fontSize\": %.6f", command.font_size);
            printf(", \"fontWeight\": %d", command.font_weight);
            printf(", \"fontFamily\": ");
            __dump_json_string(command.font_family);
            printf(", \"fontStyle\": ");
            __dump_json_string(command.font_style);
            printf(", \"fontPostscriptName\": ");
            __dump_json_string(command.font_postscript_name);
            printf(", \"assetId\": ");
            __dump_json_string(command.asset_id);
            printf(", \"imageScaleMode\": ");
            __dump_json_string(__dump_cstr(__dump_image_scale(command.image_scale_mode)));
            printf(
                ", \"hasImageTransform\": %s",
                command.has_image_transform ? "true" : "false");
            printf(
                ", \"imageTransform\": [%.6f, %.6f, %.6f, %.6f, %.6f, %.6f]",
                command.image_transform[0],
                command.image_transform[1],
                command.image_transform[2],
                command.image_transform[3],
                command.image_transform[4],
                command.image_transform[5]);
            printf(
                ", \"hasFilterColorAdjust\": %s",
                command.has_filter_color_adjust ? "true" : "false");
            printf(", \"filterColorAdjust\": ");
            __dump_float_array(command.filter_color_adjust, 8u);
            printf(
                ", \"hasPaintFilter\": %s",
                command.has_paint_filter ? "true" : "false");
            printf(", \"paintFilter\": ");
            __dump_float_array(command.paint_filter, 10u);
            printf(
                ", \"originalImageSize\": {\"w\": %u, \"h\": %u}",
                command.original_image_width,
                command.original_image_height);
            printf(", \"text\": ");
            __dump_json_string(command.text);
            printf(", \"textLines\": [");
            for(line_index = 0u;
                line_index != command.text_line_count;
                ++line_index)
            {
                figma_render_generated_text_line_desc_t line;
                figma_render_list_get_generated_texture_text_line(
                    render_list, index, line_index, &line);
                if(line_index != 0u)
                {
                    printf(", ");
                }
                printf(
                    "{\"x\": %.6f, \"y\": %.6f, \"width\": %.6f, \"lineHeight\": %.6f, \"lineAscent\": %.6f, \"text\": ",
                    line.x,
                    line.y,
                    line.width,
                    line.line_height,
                    line.line_ascent);
                __dump_json_string(line.text);
                printf("}");
            }
            printf("]");
            printf(
                ", \"vertices\": %u, \"indices\": %u}%s\n",
                command.vertex_count,
                command.index_count,
                index + 1u != count ? "," : "");
        }
        __dump_indent(indent);
    }
    printf("]");
}

//////////////////////////////////////////////////////////////////////////
int main(int argc, char ** argv)
{
    dump_options_t options;
    byte_buffer_t fig_data;
    byte_buffer_t ux_data;
    figma_runtime_t * runtime = NULL;
    figma_document_t * document = NULL;
    figma_player_t * player = NULL;
    figma_runtime_desc_t runtime_desc;
    gp_graphics_t * graphics = NULL;
    figma_load_options_t load_options;
    figma_inspection_document_desc_t document_desc;
    const figma_inspection_node_t * root;
    const figma_inspection_node_t * prototype_frame;
    node_list_t matches;
    size_t node_count = 0u;
    size_t frame_count = 0u;
    figma_result_t result;
    int exit_code = EXIT_FAILURE;

    setlocale(LC_NUMERIC, "C");
    memset(&fig_data, 0, sizeof(fig_data));
    memset(&ux_data, 0, sizeof(ux_data));
    memset(&matches, 0, sizeof(matches));
    memset(&runtime_desc, 0, sizeof(runtime_desc));
    memset(&load_options, 0, sizeof(load_options));

    if(__dump_parse_options(argc, argv, &options) == FIGMA_FALSE)
    {
        __dump_usage(argv[0]);
        return EXIT_FAILURE;
    }
    graphics = figma_graphics_object_create();
    if(graphics == NULL)
    {
        fprintf(stderr, "createGraphics failed\n");
        goto cleanup;
    }
    runtime_desc.graphics = graphics;
    result =
        figma_runtime_create(FIGMA_SDK_VERSION, &runtime_desc, &runtime);
    if(result != FIGMA_RESULT_OK)
    {
        fprintf(
            stderr, "createRuntime failed: %s\n", __dump_result_name(result));
        goto cleanup;
    }
    if(__dump_read_file(options.fig_path, &fig_data) == FIGMA_FALSE)
    {
        fprintf(
            stderr, "Unable to read .fig file: %s\n", options.fig_path);
        goto cleanup;
    }
    load_options.source_name = __dump_cstr(options.fig_path);
    load_options.extract_image_assets = FIGMA_TRUE;
    load_options.keep_canvas_bytes = FIGMA_TRUE;
    result = figma_runtime_load_document_from_fig_data(
        runtime,
        fig_data.data,
        fig_data.size,
        &load_options,
        &document);
    if(result != FIGMA_RESULT_OK)
    {
        fprintf(
            stderr,
            "loadDocumentFromFigData failed: %s\n",
            __dump_result_name(result));
        goto cleanup;
    }
    if(options.sidecar_path != NULL)
    {
        if(__dump_read_file(options.sidecar_path, &ux_data) == FIGMA_FALSE)
        {
            fprintf(
                stderr,
                "Unable to read .ux.json sidecar: %s\n",
                options.sidecar_path);
            goto cleanup;
        }
        result = figma_document_load_ux(
            document,
            (figma_string_view_t){
                (const char *)ux_data.data, ux_data.size});
        if(result != FIGMA_RESULT_OK)
        {
            fprintf(stderr, "loadUX failed: %s\n", __dump_result_name(result));
            goto cleanup;
        }
    }

    figma_inspection_get_document(document, &document_desc);
    root = figma_inspection_get_canvas_root(document);
    prototype_frame = figma_inspection_get_prototype_start_node(document);
    if(__dump_walk_nodes(
           root,
           &options,
           &node_count,
           &frame_count,
           &matches) == FIGMA_FALSE)
    {
        fprintf(stderr, "Unable to collect node matches\n");
        goto cleanup;
    }

    if(options.render_list)
    {
        figma_player_desc_t player_desc;
        const figma_inspection_node_t * viewport_frame = NULL;
        figma_inspection_node_desc_t viewport_desc;
        memset(&player_desc, 0, sizeof(player_desc));
        if(options.frame_id != NULL)
        {
            player_desc.start_frame_id = __dump_cstr(options.frame_id);
            viewport_frame = figma_inspection_find_node(
                document, player_desc.start_frame_id);
        }
        if(viewport_frame == NULL)
        {
            viewport_frame = prototype_frame;
        }
        if(viewport_frame != NULL &&
            figma_inspection_get_node(viewport_frame, &viewport_desc) ==
                FIGMA_RESULT_OK)
        {
            player_desc.viewport.width =
                viewport_desc.rect.w > 1.0f ? viewport_desc.rect.w : 1.0f;
            player_desc.viewport.height =
                viewport_desc.rect.h > 1.0f ? viewport_desc.rect.h : 1.0f;
        }
        result = figma_runtime_create_player(
            runtime, document, &player_desc, &player);
        if(result != FIGMA_RESULT_OK)
        {
            fprintf(
                stderr,
                "createPlayer failed: %s\n",
                __dump_result_name(result));
            goto cleanup;
        }
    }

    printf("{\n");
    __dump_json_key(2, "path");
    __dump_json_string(document_desc.path);
    printf(",\n");
    __dump_json_key(2, "fileName");
    __dump_json_string(document_desc.file_name);
    printf(",\n");
    __dump_json_key(2, "canvasVersion");
    printf("%d,\n", (int)document_desc.canvas_version);
    __dump_json_key(2, "hasCanvasBytes");
    printf("%s,\n", document_desc.has_canvas_bytes ? "true" : "false");
    __dump_json_key(2, "thumbnailSize");
    printf(
        "{\"w\": %.3f, \"h\": %.3f},\n",
        document_desc.thumbnail_size.x,
        document_desc.thumbnail_size.y);
    __dump_json_key(2, "assets");
    printf("%u,\n", document_desc.asset_count);
    __dump_json_key(2, "nodes");
    printf("%zu,\n", node_count);
    __dump_json_key(2, "frames");
    printf("%zu,\n", frame_count);
    __dump_json_key(2, "prototypeStartFrame");
    __dump_node(prototype_frame, 2);
    printf(",\n");
    __dump_json_key(2, "matches");
    printf("[");
    if(matches.size != 0u)
    {
        size_t index;
        printf("\n");
        for(index = 0u; index != matches.size; ++index)
        {
            __dump_indent(4);
            __dump_node(matches.data[index], 4);
            printf("%s\n", index + 1u != matches.size ? "," : "");
        }
        __dump_indent(2);
    }
    printf("],\n");
    __dump_json_key(2, "diagnostics");
    __dump_diagnostics(figma_document_get_diagnostics(document), 2);
    if(options.animations)
    {
        printf(",\n");
        __dump_json_key(2, "prototypeAnimations");
        __dump_animation_nodes(root, 2);
    }
    if(player != NULL)
    {
        printf(",\n");
        __dump_json_key(2, "renderList");
        __dump_render_list(figma_player_get_render_list(player), 2);
        printf(",\n");
        __dump_json_key(2, "playerDiagnostics");
        __dump_diagnostics(figma_player_get_diagnostics(player), 2);
    }
    printf("\n}\n");
    exit_code = EXIT_SUCCESS;

cleanup:
    figma_player_destroy(player);
    figma_document_destroy(document);
    figma_runtime_destroy(runtime);
    if(graphics != NULL)
    {
        gp_graphics_destroy(graphics);
    }
    free(matches.data);
    free(ux_data.data);
    free(fig_data.data);
    return exit_code;
}

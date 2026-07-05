#include "Figma/Figma.h"

#include "../../sdk/src/Document.h"
#include "../../sdk/src/RenderList.h"

#include <algorithm>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    struct DumpOptions
    {
        const char * figPath = nullptr;
        const char * sidecarPath = nullptr;
        const char * findText = nullptr;
        const char * nodeId = nullptr;
        const char * frameId = nullptr;
        bool renderList = false;
        bool animations = false;
    };

    template<class T>
    struct FigmaInterfaceDeleter
    {
        //////////////////////////////////////////////////////////////////////////
        void operator()(T * const _ptr) const
        {
            if(_ptr != nullptr)
            {
                _ptr->destroy();
            }
        }
    };

    //////////////////////////////////////////////////////////////////////////
    const char * resultToString(Figma::EResult _result)
    {
        switch(_result)
        {
        case Figma::EResult::Ok:
            return "Ok";
        case Figma::EResult::InvalidArgument:
            return "InvalidArgument";
        case Figma::EResult::OutOfMemory:
            return "OutOfMemory";
        case Figma::EResult::IoFailed:
            return "IoFailed";
        case Figma::EResult::ParseFailed:
            return "ParseFailed";
        case Figma::EResult::UnsupportedFormat:
            return "UnsupportedFormat";
        case Figma::EResult::MissingEntry:
            return "MissingEntry";
        case Figma::EResult::NotFound:
            return "NotFound";
        case Figma::EResult::InvalidState:
            return "InvalidState";
        }

        return "Unknown";
    }

    //////////////////////////////////////////////////////////////////////////
    const Figma::Document * privateDocument(const Figma::DocumentInterface * _document)
    {
        return static_cast<const Figma::Document *>(_document);
    }

    //////////////////////////////////////////////////////////////////////////
    void printUsage(const char * _program)
    {
        std::fprintf(stderr, "Usage: %s <file.fig> [--sidecar file.ux.json] [--find text] [--node id] [--frame id] [--render-list] [--animations]\n", _program);
    }

    //////////////////////////////////////////////////////////////////////////
    bool readFileBytes(const char * _path, std::vector<std::uint8_t> * const _bytes)
    {
        if(_path == nullptr || _bytes == nullptr)
        {
            return false;
        }

        std::ifstream stream(_path, std::ios::binary);
        if(stream.good() == false)
        {
            return false;
        }

        stream.seekg(0, std::ios::end);
        const std::streampos size = stream.tellg();
        if(size < 0)
        {
            return false;
        }

        stream.seekg(0, std::ios::beg);
        _bytes->resize(static_cast<std::size_t>(size));
        if(_bytes->empty() == true)
        {
            return true;
        }

        stream.read(reinterpret_cast<char *>(_bytes->data()), static_cast<std::streamsize>(_bytes->size()));

        return stream.good();
    }

    //////////////////////////////////////////////////////////////////////////
    bool readFileText(const char * _path, std::string * const _text)
    {
        std::vector<std::uint8_t> bytes;
        if(readFileBytes(_path, &bytes) == false || _text == nullptr)
        {
            return false;
        }

        _text->assign(reinterpret_cast<const char *>(bytes.data()), bytes.size());

        return true;
    }

    //////////////////////////////////////////////////////////////////////////
    bool parseOptions(int _argc, char ** _argv, DumpOptions * const _options)
    {
        if(_argc < 2)
        {
            return false;
        }

        _options->figPath = _argv[1];

        for(int index = 2; index != _argc; ++index)
        {
            const char * arg = _argv[index];
            if(std::strcmp(arg, "--sidecar") == 0)
            {
                if(index + 1 == _argc)
                {
                    return false;
                }

                _options->sidecarPath = _argv[++index];
            }
            else if(std::strcmp(arg, "--find") == 0)
            {
                if(index + 1 == _argc)
                {
                    return false;
                }

                _options->findText = _argv[++index];
            }
            else if(std::strcmp(arg, "--node") == 0)
            {
                if(index + 1 == _argc)
                {
                    return false;
                }

                _options->nodeId = _argv[++index];
            }
            else if(std::strcmp(arg, "--frame") == 0)
            {
                if(index + 1 == _argc)
                {
                    return false;
                }

                _options->frameId = _argv[++index];
                _options->renderList = true;
            }
            else if(std::strcmp(arg, "--render-list") == 0)
            {
                _options->renderList = true;
            }
            else if(std::strcmp(arg, "--animations") == 0)
            {
                _options->animations = true;
            }
            else
            {
                return false;
            }
        }

        return true;
    }

    //////////////////////////////////////////////////////////////////////////
    void printIndent(int _indent)
    {
        for(int index = 0; index != _indent; ++index)
        {
            std::printf(" ");
        }
    }

    //////////////////////////////////////////////////////////////////////////
    void printJsonString(std::string_view _value)
    {
        std::printf("\"");
        for(char c : _value)
        {
            switch(c)
            {
            case '\\':
                std::printf("\\\\");
                break;
            case '"':
                std::printf("\\\"");
                break;
            case '\b':
                std::printf("\\b");
                break;
            case '\f':
                std::printf("\\f");
                break;
            case '\n':
                std::printf("\\n");
                break;
            case '\r':
                std::printf("\\r");
                break;
            case '\t':
                std::printf("\\t");
                break;
            default:
                if(static_cast<unsigned char>(c) < 0x20)
                {
                    std::printf("\\u%04x", static_cast<unsigned int>(static_cast<unsigned char>(c)));
                }
                else
                {
                    std::printf("%c", c);
                }
                break;
            }
        }
        std::printf("\"");
    }

    //////////////////////////////////////////////////////////////////////////
    std::string_view stringView(const Figma::FigmaString & _value)
    {
        return std::string_view(_value.data(), _value.size());
    }

    //////////////////////////////////////////////////////////////////////////
    void printJsonKey(int _indent, const char * _key)
    {
        printIndent(_indent);
        printJsonString(_key);
        std::printf(": ");
    }

    //////////////////////////////////////////////////////////////////////////
    const char * nodeTypeName(Figma::ECanvasNodeType _type)
    {
        switch(_type)
        {
        case Figma::ECanvasNodeType::Document:
            return "Document";
        case Figma::ECanvasNodeType::Canvas:
            return "Canvas";
        case Figma::ECanvasNodeType::Frame:
            return "Frame";
        case Figma::ECanvasNodeType::Group:
            return "Group";
        case Figma::ECanvasNodeType::Rectangle:
            return "Rectangle";
        case Figma::ECanvasNodeType::RoundedRectangle:
            return "RoundedRectangle";
        case Figma::ECanvasNodeType::Ellipse:
            return "Ellipse";
        case Figma::ECanvasNodeType::Text:
            return "Text";
        case Figma::ECanvasNodeType::Vector:
            return "Vector";
        default:
            return "Unknown";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    const char * paintTypeName(Figma::ECanvasPaintType _type)
    {
        switch(_type)
        {
        case Figma::ECanvasPaintType::Solid:
            return "Solid";
        case Figma::ECanvasPaintType::Image:
            return "Image";
        default:
            return "Unsupported";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    const char * canvasBlendModeName(Figma::ECanvasBlendMode _mode)
    {
        switch(_mode)
        {
        case Figma::ECanvasBlendMode::PassThrough:
            return "PassThrough";
        case Figma::ECanvasBlendMode::Normal:
            return "Normal";
        case Figma::ECanvasBlendMode::Multiply:
            return "Multiply";
        case Figma::ECanvasBlendMode::Screen:
            return "Screen";
        case Figma::ECanvasBlendMode::Overlay:
            return "Overlay";
        case Figma::ECanvasBlendMode::Darken:
            return "Darken";
        case Figma::ECanvasBlendMode::Lighten:
            return "Lighten";
        case Figma::ECanvasBlendMode::ColorDodge:
            return "ColorDodge";
        case Figma::ECanvasBlendMode::ColorBurn:
            return "ColorBurn";
        case Figma::ECanvasBlendMode::SoftLight:
            return "SoftLight";
        case Figma::ECanvasBlendMode::HardLight:
            return "HardLight";
        case Figma::ECanvasBlendMode::Difference:
            return "Difference";
        case Figma::ECanvasBlendMode::Exclusion:
            return "Exclusion";
        case Figma::ECanvasBlendMode::Hue:
            return "Hue";
        case Figma::ECanvasBlendMode::Saturation:
            return "Saturation";
        case Figma::ECanvasBlendMode::Color:
            return "Color";
        case Figma::ECanvasBlendMode::Luminosity:
            return "Luminosity";
        default:
            return "Unsupported";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    const char * imageScaleModeName(Figma::ECanvasImageScaleMode _mode)
    {
        switch(_mode)
        {
        case Figma::ECanvasImageScaleMode::Stretch:
            return "Stretch";
        case Figma::ECanvasImageScaleMode::Fit:
            return "Fit";
        case Figma::ECanvasImageScaleMode::Fill:
            return "Fill";
        case Figma::ECanvasImageScaleMode::Tile:
            return "Tile";
        default:
            return "Unknown";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    const char * strokeAlignName(Figma::ECanvasStrokeAlign _align)
    {
        switch(_align)
        {
        case Figma::ECanvasStrokeAlign::Center:
            return "Center";
        case Figma::ECanvasStrokeAlign::Inside:
            return "Inside";
        case Figma::ECanvasStrokeAlign::Outside:
            return "Outside";
        default:
            return "Unsupported";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    const char * renderCommandName(Figma::ERenderCommandType _type)
    {
        switch(_type)
        {
        case Figma::ERenderCommandType::Fill:
            return "Fill";
        case Figma::ERenderCommandType::Stroke:
            return "Stroke";
        case Figma::ERenderCommandType::Image:
            return "Image";
        case Figma::ERenderCommandType::Text:
            return "Text";
        case Figma::ERenderCommandType::Mesh:
            return "Mesh";
        case Figma::ERenderCommandType::ClipBegin:
            return "ClipBegin";
        case Figma::ERenderCommandType::ClipEnd:
            return "ClipEnd";
        case Figma::ERenderCommandType::DebugHotspot:
            return "DebugHotspot";
        default:
            return "Unknown";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    const char * renderBlendModeName(Figma::ERenderBlendMode _mode)
    {
        switch(_mode)
        {
        case Figma::ERenderBlendMode::PassThrough:
            return "PassThrough";
        case Figma::ERenderBlendMode::Normal:
            return "Normal";
        case Figma::ERenderBlendMode::Multiply:
            return "Multiply";
        case Figma::ERenderBlendMode::Screen:
            return "Screen";
        case Figma::ERenderBlendMode::Overlay:
            return "Overlay";
        case Figma::ERenderBlendMode::Darken:
            return "Darken";
        case Figma::ERenderBlendMode::Lighten:
            return "Lighten";
        case Figma::ERenderBlendMode::ColorDodge:
            return "ColorDodge";
        case Figma::ERenderBlendMode::ColorBurn:
            return "ColorBurn";
        case Figma::ERenderBlendMode::SoftLight:
            return "SoftLight";
        case Figma::ERenderBlendMode::HardLight:
            return "HardLight";
        case Figma::ERenderBlendMode::Difference:
            return "Difference";
        case Figma::ERenderBlendMode::Exclusion:
            return "Exclusion";
        case Figma::ERenderBlendMode::Hue:
            return "Hue";
        case Figma::ERenderBlendMode::Saturation:
            return "Saturation";
        case Figma::ERenderBlendMode::Color:
            return "Color";
        case Figma::ERenderBlendMode::Luminosity:
            return "Luminosity";
        default:
            return "Unsupported";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    const char * renderImageScaleModeName(Figma::ERenderImageScaleMode _mode)
    {
        switch(_mode)
        {
        case Figma::ERenderImageScaleMode::Stretch:
            return "Stretch";
        case Figma::ERenderImageScaleMode::Fit:
            return "Fit";
        case Figma::ERenderImageScaleMode::Fill:
            return "Fill";
        case Figma::ERenderImageScaleMode::Tile:
            return "Tile";
        default:
            return "Unknown";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    const char * diagnosticSeverityName(Figma::EDiagnosticSeverity _severity)
    {
        switch(_severity)
        {
        case Figma::EDiagnosticSeverity::Info:
            return "Info";
        case Figma::EDiagnosticSeverity::Warning:
            return "Warning";
        case Figma::EDiagnosticSeverity::Error:
            return "Error";
        default:
            return "Unknown";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    const char * prototypeEventName(Figma::EPrototypeEventType _type)
    {
        switch(_type)
        {
        case Figma::EPrototypeEventType::Click:
            return "Click";
        case Figma::EPrototypeEventType::Hover:
            return "Hover";
        case Figma::EPrototypeEventType::AfterTimeout:
            return "AfterTimeout";
        default:
            return "Unsupported";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    const char * prototypeConnectionName(Figma::EPrototypeConnectionType _type)
    {
        switch(_type)
        {
        case Figma::EPrototypeConnectionType::None:
            return "None";
        case Figma::EPrototypeConnectionType::InternalNode:
            return "InternalNode";
        case Figma::EPrototypeConnectionType::Back:
            return "Back";
        case Figma::EPrototypeConnectionType::Close:
            return "Close";
        default:
            return "Unsupported";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    const char * prototypeNavigationName(Figma::EPrototypeNavigationType _type)
    {
        switch(_type)
        {
        case Figma::EPrototypeNavigationType::Navigate:
            return "Navigate";
        case Figma::EPrototypeNavigationType::Overlay:
            return "Overlay";
        case Figma::EPrototypeNavigationType::Swap:
            return "Swap";
        case Figma::EPrototypeNavigationType::ScrollTo:
            return "ScrollTo";
        default:
            return "Unsupported";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    const char * prototypeTransitionName(Figma::EPrototypeTransitionType _type)
    {
        switch(_type)
        {
        case Figma::EPrototypeTransitionType::Instant:
            return "Instant";
        case Figma::EPrototypeTransitionType::Dissolve:
            return "Dissolve";
        case Figma::EPrototypeTransitionType::SmartAnimate:
            return "SmartAnimate";
        case Figma::EPrototypeTransitionType::MoveIn:
            return "MoveIn";
        case Figma::EPrototypeTransitionType::MoveOut:
            return "MoveOut";
        case Figma::EPrototypeTransitionType::Push:
            return "Push";
        case Figma::EPrototypeTransitionType::SlideIn:
            return "SlideIn";
        case Figma::EPrototypeTransitionType::SlideOut:
            return "SlideOut";
        default:
            return "Unsupported";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    const char * prototypeTransitionDirectionName(Figma::EPrototypeTransitionDirection _type)
    {
        switch(_type)
        {
        case Figma::EPrototypeTransitionDirection::None:
            return "None";
        case Figma::EPrototypeTransitionDirection::Left:
            return "Left";
        case Figma::EPrototypeTransitionDirection::Right:
            return "Right";
        case Figma::EPrototypeTransitionDirection::Up:
            return "Up";
        case Figma::EPrototypeTransitionDirection::Down:
            return "Down";
        default:
            return "Unsupported";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    const char * animationEasingName(Figma::EAnimationEasing _type)
    {
        switch(_type)
        {
        case Figma::EAnimationEasing::Linear:
            return "Linear";
        case Figma::EAnimationEasing::EaseIn:
            return "EaseIn";
        case Figma::EAnimationEasing::EaseOut:
            return "EaseOut";
        case Figma::EAnimationEasing::EaseInOut:
            return "EaseInOut";
        case Figma::EAnimationEasing::InCubic:
            return "InCubic";
        case Figma::EAnimationEasing::OutCubic:
            return "OutCubic";
        case Figma::EAnimationEasing::InOutCubic:
            return "InOutCubic";
        default:
            return "Unsupported";
        }
    }

    //////////////////////////////////////////////////////////////////////////
    bool containsText(std::string_view _value, std::string_view _needle)
    {
        return _needle.empty() == false && _value.find(_needle) != std::string_view::npos;
    }

    //////////////////////////////////////////////////////////////////////////
    bool nodeMatches(const Figma::CanvasNodeDesc * const _node, const DumpOptions & _options)
    {
        if(_node == nullptr)
        {
            return false;
        }

        if(_options.nodeId != nullptr && stringView(_node->id) == _options.nodeId)
        {
            return true;
        }

        if(_options.findText != nullptr)
        {
            const std::string_view needle(_options.findText);
            return containsText(stringView(_node->id), needle) == true || containsText(stringView(_node->name), needle) == true || containsText(stringView(_node->text), needle) == true;
        }

        return false;
    }

    //////////////////////////////////////////////////////////////////////////
    void collectNodeMatches(const Figma::CanvasNodeDesc * const _node, const DumpOptions & _options, std::vector<const Figma::CanvasNodeDesc *> * const _matches)
    {
        if(_node == nullptr)
        {
            return;
        }

        if(nodeMatches(_node, _options) == true)
        {
            _matches->emplace_back(_node);
        }

        const std::uint32_t childCount = static_cast<std::uint32_t>(_node->children.size());
        for(std::uint32_t index = 0; index != childCount; ++index)
        {
            if(const Figma::CanvasNodeDesc * child = Figma::valueAt(_node->children, index))
            {
                collectNodeMatches(child, _options, _matches);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////
    void countNodes(const Figma::CanvasNodeDesc * const _node, std::size_t * const _nodeCount, std::size_t * const _frameCount)
    {
        if(_node == nullptr)
        {
            return;
        }

        ++*_nodeCount;
        if(_node->type == Figma::ECanvasNodeType::Frame)
        {
            ++*_frameCount;
        }

        const std::uint32_t childCount = static_cast<std::uint32_t>(_node->children.size());
        for(std::uint32_t index = 0; index != childCount; ++index)
        {
            if(const Figma::CanvasNodeDesc * child = Figma::valueAt(_node->children, index))
            {
                countNodes(child, _nodeCount, _frameCount);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////
    void printRect(const Figma::Rectf & _rect)
    {
        //////////////////////////////////////////////////////////////////////////
        std::printf("{\"x\": %.3f, \"y\": %.3f, \"w\": %.3f, \"h\": %.3f}", _rect.x, _rect.y, _rect.w, _rect.h);
    }

    //////////////////////////////////////////////////////////////////////////
    void printColor(const Figma::Color & _color)
    {
        //////////////////////////////////////////////////////////////////////////
        std::printf("{\"r\": %.6f, \"g\": %.6f, \"b\": %.6f, \"a\": %.6f}", _color.r, _color.g, _color.b, _color.a);
    }

    //////////////////////////////////////////////////////////////////////////
    void printFloatArray(const float * _values, std::size_t _count)
    {
        std::printf("[");
        for(std::size_t index = 0; index != _count; ++index)
        {
            if(index != 0)
            {
                std::printf(", ");
            }
            std::printf("%.6f", _values[index]);
        }
        std::printf("]");
    }

    //////////////////////////////////////////////////////////////////////////
    void printPaint(const Figma::CanvasPaint * const _paint, int _indent)
    {
        if(_paint == nullptr)
        {
            std::printf("null");
            return;
        }

        //////////////////////////////////////////////////////////////////////////
        std::printf("{\n");
        printJsonKey(_indent + 2, "type");
        printJsonString(paintTypeName(_paint->type));
        std::printf(",\n");
        printJsonKey(_indent + 2, "rawType");
        printJsonString(stringView(_paint->rawType));
        std::printf(",\n");
        printJsonKey(_indent + 2, "blendMode");
        printJsonString(canvasBlendModeName(_paint->blendMode));
        std::printf(",\n");
        printJsonKey(_indent + 2, "rawBlendMode");
        printJsonString(stringView(_paint->rawBlendMode));
        std::printf(",\n");
        printJsonKey(_indent + 2, "imageScaleMode");
        printJsonString(imageScaleModeName(_paint->imageScaleMode));
        std::printf(",\n");
        printJsonKey(_indent + 2, "hasTransform");
        std::printf("%s,\n", _paint->hasTransformValue == true ? "true" : "false");
        printJsonKey(_indent + 2, "transform");
        printFloatArray(_paint->transform, 6);
        std::printf(",\n");
        printJsonKey(_indent + 2, "visible");
        std::printf("%s,\n", _paint->visible == true ? "true" : "false");
        printJsonKey(_indent + 2, "opacity");
        std::printf("%.6f,\n", _paint->opacity);
        printJsonKey(_indent + 2, "color");
        printColor(_paint->color);
        if(_paint->assetId.empty() == false)
        {
            std::printf(",\n");
            printJsonKey(_indent + 2, "assetId");
            printJsonString(stringView(_paint->assetId));
        }
        std::printf(",\n");
        printJsonKey(_indent + 2, "hasFilterColorAdjust");
        std::printf("%s,\n", _paint->hasFilterColorAdjustValue == true ? "true" : "false");
        printJsonKey(_indent + 2, "filterColorAdjust");
        printFloatArray(_paint->filterColorAdjust, 8);
        std::printf(",\n");
        printJsonKey(_indent + 2, "hasPaintFilter");
        std::printf("%s,\n", _paint->hasPaintFilterValue == true ? "true" : "false");
        printJsonKey(_indent + 2, "paintFilter");
        printFloatArray(_paint->paintFilter, 10);
        std::printf(",\n");
        printJsonKey(_indent + 2, "originalImageSize");
        //////////////////////////////////////////////////////////////////////////
        std::printf("{\"w\": %u, \"h\": %u}", _paint->originalImageWidth, _paint->originalImageHeight);
        std::printf("\n");
        printIndent(_indent);
        std::printf("}");
    }

    template<class TGetter>
    //////////////////////////////////////////////////////////////////////////
    void printPaintArray(std::uint32_t _count, const TGetter & _getter, int _indent)
    {
        std::printf("[");
        if(_count != 0)
        {
            std::printf("\n");
            for(std::uint32_t index = 0; index != _count; ++index)
            {
                printIndent(_indent + 2);
                if(const Figma::CanvasPaint * paint = _getter(index))
                {
                    printPaint(paint, _indent + 2);
                }
                else
                {
                    std::printf("null");
                }
                if(index + 1 != _count)
                {
                    std::printf(",");
                }
                std::printf("\n");
            }
            printIndent(_indent);
        }
        std::printf("]");
    }

    //////////////////////////////////////////////////////////////////////////
    void printPath(const Figma::CanvasPathDesc * const _path, int _indent)
    {
        if(_path == nullptr)
        {
            std::printf("null");
            return;
        }

        float minX = std::numeric_limits<float>::max();
        float minY = std::numeric_limits<float>::max();
        float maxX = std::numeric_limits<float>::lowest();
        float maxY = std::numeric_limits<float>::lowest();
        bool hasPoint = false;
        auto addPoint = [&](const Figma::Vec2f & _point) {
            minX = std::min(minX, _point.x);
            minY = std::min(minY, _point.y);
            maxX = std::max(maxX, _point.x);
            maxY = std::max(maxY, _point.y);
            hasPoint = true;
        };

        const std::uint32_t commandCount = static_cast<std::uint32_t>(_path->commands.size());
        for(std::uint32_t index = 0; index != commandCount; ++index)
        {
            const Figma::CanvasPathCommandDesc * commandPtr = Figma::valueAt(_path->commands, index);
            if(commandPtr == nullptr)
            {
                continue;
            }

            const Figma::CanvasPathCommandDesc & command = *commandPtr;
            switch(command.type)
            {
            case Figma::ECanvasPathCommandType::MoveTo:
            case Figma::ECanvasPathCommandType::LineTo:
                addPoint(command.p0);
                break;
            case Figma::ECanvasPathCommandType::QuadraticTo:
                addPoint(command.p0);
                addPoint(command.p1);
                break;
            case Figma::ECanvasPathCommandType::CubicTo:
                addPoint(command.p0);
                addPoint(command.p1);
                addPoint(command.p2);
                break;
            case Figma::ECanvasPathCommandType::Close:
                break;
            }
        }

        //////////////////////////////////////////////////////////////////////////
        std::printf("{\n");
        printJsonKey(_indent + 2, "styleId");
        std::printf("%u,\n", _path->styleId);
        printJsonKey(_indent + 2, "commandsBlob");
        std::printf("%u,\n", _path->commandsBlob);
        printJsonKey(_indent + 2, "commandsDecoded");
        std::printf("%s,\n", _path->commandsDecoded == true ? "true" : "false");
        printJsonKey(_indent + 2, "commands");
        std::printf("%u,\n", static_cast<std::uint32_t>(_path->commands.size()));
        printJsonKey(_indent + 2, "bounds");
        if(hasPoint == true)
        {
            //////////////////////////////////////////////////////////////////////////
            std::printf("{\"x\": %.6f, \"y\": %.6f, \"w\": %.6f, \"h\": %.6f},\n", minX, minY, maxX - minX, maxY - minY);
        }
        else
        {
            std::printf("null,\n");
        }
        printJsonKey(_indent + 2, "paints");
        printPaintArray(static_cast<std::uint32_t>(_path->paints.size()), [&](std::uint32_t _index) {
            return Figma::valueAt(_path->paints, _index);
        }, _indent + 2);
        std::printf("\n");
        printIndent(_indent);
        std::printf("}");
    }

    template<class TGetter>
    //////////////////////////////////////////////////////////////////////////
    void printPathArray(std::uint32_t _count, const TGetter & _getter, int _indent)
    {
        std::printf("[");
        if(_count != 0)
        {
            std::printf("\n");
            for(std::uint32_t index = 0; index != _count; ++index)
            {
                printIndent(_indent + 2);
                if(const Figma::CanvasPathDesc * path = _getter(index))
                {
                    printPath(path, _indent + 2);
                }
                else
                {
                    std::printf("null");
                }
                if(index + 1 != _count)
                {
                    std::printf(",");
                }
                std::printf("\n");
            }
            printIndent(_indent);
        }
        std::printf("]");
    }

    template<class TGetter>
    //////////////////////////////////////////////////////////////////////////
    void printStringArray(std::uint32_t _count, const TGetter & _getter, int _indent)
    {
        std::printf("[");
        if(_count != 0)
        {
            std::printf("\n");
            for(std::uint32_t index = 0; index != _count; ++index)
            {
                printIndent(_indent + 2);
                printJsonString(_getter(index));
                if(index + 1 != _count)
                {
                    std::printf(",");
                }
                std::printf("\n");
            }
            printIndent(_indent);
        }
        std::printf("]");
    }

    //////////////////////////////////////////////////////////////////////////
    void printPrototypeAction(const Figma::PrototypeActionDesc * const _action, int _indent)
    {
        if(_action == nullptr)
        {
            std::printf("null");
            return;
        }

        //////////////////////////////////////////////////////////////////////////
        std::printf("{\n");
        printJsonKey(_indent + 2, "targetNodeId");
        printJsonString(stringView(_action->targetNodeId));
        std::printf(",\n");
        printJsonKey(_indent + 2, "connectionType");
        printJsonString(prototypeConnectionName(_action->connectionType));
        std::printf(",\n");
        printJsonKey(_indent + 2, "rawConnectionType");
        printJsonString(stringView(_action->rawConnectionType));
        std::printf(",\n");
        printJsonKey(_indent + 2, "navigationType");
        printJsonString(prototypeNavigationName(_action->navigationType));
        std::printf(",\n");
        printJsonKey(_indent + 2, "rawNavigationType");
        printJsonString(stringView(_action->rawNavigationType));
        std::printf(",\n");
        printJsonKey(_indent + 2, "transitionType");
        printJsonString(prototypeTransitionName(_action->transitionType));
        std::printf(",\n");
        printJsonKey(_indent + 2, "rawTransitionType");
        printJsonString(stringView(_action->rawTransitionType));
        std::printf(",\n");
        printJsonKey(_indent + 2, "transitionDirection");
        printJsonString(prototypeTransitionDirectionName(_action->transitionDirection));
        std::printf(",\n");
        printJsonKey(_indent + 2, "rawTransitionDirection");
        printJsonString(stringView(_action->rawTransitionDirection));
        std::printf(",\n");
        printJsonKey(_indent + 2, "transitionEasing");
        printJsonString(animationEasingName(_action->transitionEasing));
        std::printf(",\n");
        printJsonKey(_indent + 2, "rawTransitionEasing");
        printJsonString(stringView(_action->rawTransitionEasing));
        std::printf(",\n");
        printJsonKey(_indent + 2, "transitionDuration");
        std::printf("%.6f,\n", _action->transitionDuration);
        printJsonKey(_indent + 2, "smartAnimate");
        std::printf("%s,\n", _action->smartAnimate == true ? "true" : "false");
        printJsonKey(_indent + 2, "transitionPreserveScroll");
        std::printf("%s,\n", _action->transitionPreserveScroll == true ? "true" : "false");
        printJsonKey(_indent + 2, "transitionResetVideoPosition");
        std::printf("%s,\n", _action->transitionResetVideoPosition == true ? "true" : "false");
        printJsonKey(_indent + 2, "hasEasingFunction");
        std::printf("%s,\n", _action->hasEasingFunctionValue == true ? "true" : "false");
        printJsonKey(_indent + 2, "unsupportedFields");
        printStringArray(static_cast<std::uint32_t>(_action->unsupportedFields.size()), [&](std::uint32_t _index) {
            return stringView(_action->unsupportedFields[_index]);
        }, _indent + 2);
        std::printf("\n");
        printIndent(_indent);
        std::printf("}");
    }

    //////////////////////////////////////////////////////////////////////////
    void printPrototypeInteraction(const Figma::PrototypeInteractionDesc * const _interaction, int _indent)
    {
        if(_interaction == nullptr)
        {
            std::printf("null");
            return;
        }

        //////////////////////////////////////////////////////////////////////////
        std::printf("{\n");
        printJsonKey(_indent + 2, "id");
        printJsonString(stringView(_interaction->id));
        std::printf(",\n");
        printJsonKey(_indent + 2, "eventType");
        printJsonString(prototypeEventName(_interaction->eventType));
        std::printf(",\n");
        printJsonKey(_indent + 2, "rawEventType");
        printJsonString(stringView(_interaction->rawEventType));
        std::printf(",\n");
        printJsonKey(_indent + 2, "transitionTimeout");
        std::printf("%.6f,\n", _interaction->transitionTimeout);
        printJsonKey(_indent + 2, "unsupportedFields");
        printStringArray(static_cast<std::uint32_t>(_interaction->unsupportedFields.size()), [&](std::uint32_t _index) {
            return stringView(_interaction->unsupportedFields[_index]);
        }, _indent + 2);
        std::printf(",\n");
        printJsonKey(_indent + 2, "actions");
        std::printf("[");
        const std::uint32_t actionCount = static_cast<std::uint32_t>(_interaction->actions.size());
        if(actionCount != 0)
        {
            std::printf("\n");
            for(std::uint32_t index = 0; index != actionCount; ++index)
            {
                printIndent(_indent + 4);
                if(const Figma::PrototypeActionDesc * action = Figma::valueAt(_interaction->actions, index))
                {
                    printPrototypeAction(action, _indent + 4);
                }
                else
                {
                    std::printf("null");
                }
                if(index + 1 != actionCount)
                {
                    std::printf(",");
                }
                std::printf("\n");
            }
            printIndent(_indent + 2);
        }
        std::printf("]\n");
        printIndent(_indent);
        std::printf("}");
    }

    //////////////////////////////////////////////////////////////////////////
    void printAnimationNodeRecursive(const Figma::CanvasNodeDesc * const _node, int _indent, bool * const _first)
    {
        if(_node == nullptr)
        {
            return;
        }

        const std::uint32_t interactionCount = static_cast<std::uint32_t>(_node->prototypeInteractions.size());
        if(interactionCount != 0)
        {
            if(*_first == false)
            {
                std::printf(",\n");
            }
            *_first = false;

            printIndent(_indent);
            //////////////////////////////////////////////////////////////////////////
            std::printf("{\n");
            printJsonKey(_indent + 2, "nodeId");
            printJsonString(stringView(_node->id));
            std::printf(",\n");
            printJsonKey(_indent + 2, "name");
            printJsonString(stringView(_node->name));
            std::printf(",\n");
            printJsonKey(_indent + 2, "type");
            printJsonString(nodeTypeName(_node->type));
            std::printf(",\n");
            printJsonKey(_indent + 2, "rect");
            printRect(_node->rect);
            std::printf(",\n");
            printJsonKey(_indent + 2, "interactions");
            std::printf("[");
            if(interactionCount != 0)
            {
                std::printf("\n");
                for(std::uint32_t index = 0; index != interactionCount; ++index)
                {
                    printIndent(_indent + 4);
                    if(const Figma::PrototypeInteractionDesc * interaction = Figma::valueAt(_node->prototypeInteractions, index))
                    {
                        printPrototypeInteraction(interaction, _indent + 4);
                    }
                    else
                    {
                        std::printf("null");
                    }
                    if(index + 1 != interactionCount)
                    {
                        std::printf(",");
                    }
                    std::printf("\n");
                }
                printIndent(_indent + 2);
            }
            std::printf("]\n");
            printIndent(_indent);
            std::printf("}");
        }

        const std::uint32_t childCount = static_cast<std::uint32_t>(_node->children.size());
        for(std::uint32_t index = 0; index != childCount; ++index)
        {
            if(const Figma::CanvasNodeDesc * child = Figma::valueAt(_node->children, index))
            {
                printAnimationNodeRecursive(child, _indent, _first);
            }
        }
    }

    //////////////////////////////////////////////////////////////////////////
    void printAnimationNodes(const Figma::CanvasNodeDesc * _root, int _indent)
    {
        std::printf("[");
        if(_root != nullptr)
        {
            bool first = true;
            std::printf("\n");
            printAnimationNodeRecursive(_root, _indent + 2, &first);
            if(first == false)
            {
                std::printf("\n");
            }
            printIndent(_indent);
        }
        std::printf("]");
    }

    //////////////////////////////////////////////////////////////////////////
    void printNode(const Figma::CanvasNodeDesc * const _node, int _indent)
    {
        if(_node == nullptr)
        {
            std::printf("null");
            return;
        }

        //////////////////////////////////////////////////////////////////////////
        std::printf("{\n");
        printJsonKey(_indent + 2, "id");
        printJsonString(stringView(_node->id));
        std::printf(",\n");
        printJsonKey(_indent + 2, "name");
        printJsonString(stringView(_node->name));
        std::printf(",\n");
        printJsonKey(_indent + 2, "type");
        printJsonString(nodeTypeName(_node->type));
        std::printf(",\n");
        printJsonKey(_indent + 2, "rect");
        printRect(_node->rect);
        std::printf(",\n");
        printJsonKey(_indent + 2, "opacity");
        std::printf("%.6f,\n", _node->opacity);
        printJsonKey(_indent + 2, "blendMode");
        printJsonString(canvasBlendModeName(_node->blendMode));
        std::printf(",\n");
        printJsonKey(_indent + 2, "rawBlendMode");
        printJsonString(stringView(_node->rawBlendMode));
        std::printf(",\n");
        printJsonKey(_indent + 2, "visible");
        std::printf("%s,\n", _node->visible == true ? "true" : "false");
        printJsonKey(_indent + 2, "mask");
        std::printf("%s,\n", _node->mask == true ? "true" : "false");
        printJsonKey(_indent + 2, "cornerRadius");
        std::printf("%.6f,\n", _node->cornerRadius);
        printJsonKey(_indent + 2, "strokeWeight");
        std::printf("%.6f,\n", _node->strokeWeight);
        printJsonKey(_indent + 2, "strokeAlign");
        printJsonString(strokeAlignName(_node->strokeAlign));
        std::printf(",\n");
        printJsonKey(_indent + 2, "text");
        printJsonString(stringView(_node->text));
        std::printf(",\n");
        printJsonKey(_indent + 2, "fontFamily");
        printJsonString(stringView(_node->fontFamily));
        std::printf(",\n");
        printJsonKey(_indent + 2, "fontStyle");
        printJsonString(stringView(_node->fontStyle));
        std::printf(",\n");
        printJsonKey(_indent + 2, "fontPostscriptName");
        printJsonString(stringView(_node->fontPostscriptName));
        std::printf(",\n");
        printJsonKey(_indent + 2, "fontSize");
        std::printf("%.6f,\n", _node->fontSize);
        printJsonKey(_indent + 2, "fontWeight");
        std::printf("%d,\n", _node->fontWeight);
        printJsonKey(_indent + 2, "symbolId");
        printJsonString(stringView(_node->symbolId));
        std::printf(",\n");
        printJsonKey(_indent + 2, "fillStyleNodeId");
        printJsonString(stringView(_node->fillStyleNodeId));
        std::printf(",\n");
        printJsonKey(_indent + 2, "strokeFillStyleNodeId");
        printJsonString(stringView(_node->strokeFillStyleNodeId));
        std::printf(",\n");
        printJsonKey(_indent + 2, "fills");
        printPaintArray(static_cast<std::uint32_t>(_node->fills.size()), [&](std::uint32_t _index) {
            return Figma::valueAt(_node->fills, _index);
        }, _indent + 2);
        std::printf(",\n");
        printJsonKey(_indent + 2, "strokes");
        printPaintArray(static_cast<std::uint32_t>(_node->strokes.size()), [&](std::uint32_t _index) {
            return Figma::valueAt(_node->strokes, _index);
        }, _indent + 2);
        std::printf(",\n");
        printJsonKey(_indent + 2, "fillGeometry");
        printPathArray(static_cast<std::uint32_t>(_node->fillGeometry.size()), [&](std::uint32_t _index) {
            return Figma::valueAt(_node->fillGeometry, _index);
        }, _indent + 2);
        std::printf(",\n");
        printJsonKey(_indent + 2, "strokeGeometry");
        printPathArray(static_cast<std::uint32_t>(_node->strokeGeometry.size()), [&](std::uint32_t _index) {
            return Figma::valueAt(_node->strokeGeometry, _index);
        }, _indent + 2);
        std::printf(",\n");
        printJsonKey(_indent + 2, "children");
        std::printf("%u\n", static_cast<std::uint32_t>(_node->children.size()));
        printIndent(_indent);
        std::printf("}");
    }

    //////////////////////////////////////////////////////////////////////////
    void printDiagnostics(const Figma::DiagnosticsInterface * const _diagnostics, int _indent)
    {
        if(_diagnostics == nullptr)
        {
            std::printf("[]");
            return;
        }

        const Figma::DiagnosticVector & items = _diagnostics->getItems();
        std::printf("[");
        if(items.empty() == false)
        {
            std::printf("\n");
            const std::size_t itemSize = items.size();
            for(std::size_t index = 0; index != itemSize; ++index)
            {
                const Figma::Diagnostic & diagnostic = items[index];
                printIndent(_indent + 2);
                //////////////////////////////////////////////////////////////////////////
                std::printf("{\"severity\": ");
                printJsonString(diagnosticSeverityName(diagnostic.severity));
                std::printf(", \"code\": ");
                printJsonString(diagnostic.code);
                std::printf(", \"nodeId\": ");
                printJsonString(diagnostic.nodeId);
                std::printf(", \"message\": ");
                printJsonString(diagnostic.message);
                std::printf("}");
                if(index + 1 != itemSize)
                {
                    std::printf(",");
                }
                std::printf("\n");
            }
            printIndent(_indent);
        }
        std::printf("]");
    }

    //////////////////////////////////////////////////////////////////////////
    void printRenderList(const Figma::RenderListInterface * const _renderList, int _indent)
    {
        if(_renderList == nullptr)
        {
            std::printf("[]");
            return;
        }

        const Figma::RenderList & renderList = static_cast<const Figma::RenderList &>(*_renderList);
        const Figma::RenderCommandVector & commands = renderList.getCommands();
        std::printf("[");
        if(commands.empty() == false)
        {
            std::printf("\n");
            const std::size_t commandSize = commands.size();
            for(std::size_t index = 0; index != commandSize; ++index)
            {
                const Figma::RenderCommand & command = commands[index];
                printIndent(_indent + 2);
                //////////////////////////////////////////////////////////////////////////
                std::printf("{\"type\": ");
                printJsonString(renderCommandName(command.type));
                std::printf(", \"id\": ");
                printJsonString(command.id);
                std::printf(", \"nodeId\": ");
                printJsonString(command.nodeId);
                std::printf(", \"rect\": ");
                printRect(command.rect);
                std::printf(", \"color\": ");
                printColor(command.color);
                std::printf(", \"opacity\": %.6f", command.opacity);
                std::printf(", \"blendMode\": ");
                printJsonString(renderBlendModeName(command.blendMode));
                std::printf(", \"renderLayerId\": %u", command.renderLayerId);
                std::printf(", \"renderLayerOpacity\": %.6f", command.renderLayerOpacity);
                std::printf(", \"strokeWidth\": %.6f", command.strokeWidth);
                std::printf(", \"fontSize\": %.6f", command.fontSize);
                std::printf(", \"fontWeight\": %d", command.fontWeight);
                std::printf(", \"fontFamily\": ");
                printJsonString(command.fontFamily);
                std::printf(", \"fontStyle\": ");
                printJsonString(command.fontStyle);
                std::printf(", \"fontPostscriptName\": ");
                printJsonString(command.fontPostscriptName);
                std::printf(", \"assetId\": ");
                printJsonString(command.assetId);
                std::printf(", \"imageScaleMode\": ");
                printJsonString(renderImageScaleModeName(command.imageScaleMode));
                std::printf(", \"hasImageTransform\": %s", command.hasImageTransformValue == true ? "true" : "false");
                std::printf(", \"imageTransform\": [%.6f, %.6f, %.6f, %.6f, %.6f, %.6f]",
                    command.imageTransform[0], command.imageTransform[1], command.imageTransform[2],
                    command.imageTransform[3], command.imageTransform[4], command.imageTransform[5]);
                std::printf(", \"hasFilterColorAdjust\": %s", command.hasFilterColorAdjustValue == true ? "true" : "false");
                std::printf(", \"filterColorAdjust\": [%.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f]",
                    command.filterColorAdjust[0], command.filterColorAdjust[1], command.filterColorAdjust[2], command.filterColorAdjust[3],
                    command.filterColorAdjust[4], command.filterColorAdjust[5], command.filterColorAdjust[6], command.filterColorAdjust[7]);
                std::printf(", \"hasPaintFilter\": %s", command.hasPaintFilterValue == true ? "true" : "false");
                std::printf(", \"paintFilter\": [%.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f, %.6f]",
                    command.paintFilter[0], command.paintFilter[1], command.paintFilter[2], command.paintFilter[3], command.paintFilter[4],
                    command.paintFilter[5], command.paintFilter[6], command.paintFilter[7], command.paintFilter[8], command.paintFilter[9]);
                //////////////////////////////////////////////////////////////////////////
                std::printf(", \"originalImageSize\": {\"w\": %u, \"h\": %u}", command.originalImageWidth, command.originalImageHeight);
                std::printf(", \"text\": ");
                printJsonString(command.text);
                std::printf(", \"textLines\": [");
                const std::size_t textLineSize = command.textLines.size();
                for(std::size_t lineIndex = 0; lineIndex != textLineSize; ++lineIndex)
                {
                    const Figma::RenderTextLineDesc & line = command.textLines[lineIndex];
                    if(lineIndex != 0)
                    {
                        std::printf(", ");
                    }
                    std::printf("{\"x\": %.6f, \"y\": %.6f, \"width\": %.6f, \"lineHeight\": %.6f, \"lineAscent\": %.6f, \"text\": ",
                        line.x, line.y, line.width, line.lineHeight, line.lineAscent);
                    printJsonString(line.text);
                    std::printf("}");
                }
                std::printf("]");
                std::printf(", \"vertices\": %zu, \"indices\": %zu}", command.vertices.size(), command.indices.size());
                if(index + 1 != commandSize)
                {
                    std::printf(",");
                }
                std::printf("\n");
            }
            printIndent(_indent);
        }
        std::printf("]");
    }
}

//////////////////////////////////////////////////////////////////////////
int main(int _argc, char ** _argv)
{
    DumpOptions options;
    if(parseOptions(_argc, _argv, &options) == false)
    {
        printUsage(_argv[0]);
        return EXIT_FAILURE;
    }

    Figma::RuntimeInterface * runtimePtr = nullptr;
    Figma::EResult result = Figma::createRuntime({}, &runtimePtr);
    if(result != Figma::EResult::Ok)
    {
        std::fprintf(stderr, "createRuntime failed: %s\n", resultToString(result));
        return EXIT_FAILURE;
    }
    std::unique_ptr<Figma::RuntimeInterface, FigmaInterfaceDeleter<Figma::RuntimeInterface>> runtime(runtimePtr);

    std::vector<std::uint8_t> figBytes;
    if(readFileBytes(options.figPath, &figBytes) == false)
    {
        std::fprintf(stderr, "Unable to read .fig file: %s\n", options.figPath);
        return EXIT_FAILURE;
    }

    Figma::LoadOptions loadOptions;
    loadOptions.sourceName = options.figPath;

    Figma::DocumentInterface * documentPtr = nullptr;
    result = runtime->loadDocumentFromFigData(figBytes.data(), figBytes.size(), loadOptions, &documentPtr);
    if(result != Figma::EResult::Ok)
    {
        std::fprintf(stderr, "loadDocumentFromFigData failed: %s\n", resultToString(result));
        return EXIT_FAILURE;
    }
    std::unique_ptr<Figma::DocumentInterface, FigmaInterfaceDeleter<Figma::DocumentInterface>> document(documentPtr);

    if(options.sidecarPath != nullptr)
    {
        std::string sidecarJson;
        if(readFileText(options.sidecarPath, &sidecarJson) == false)
        {
            std::fprintf(stderr, "Unable to read .ux.json sidecar: %s\n", options.sidecarPath);
            return EXIT_FAILURE;
        }

        result = document->loadBindingSidecarJson(Figma::FigmaStringView(sidecarJson.data(), sidecarJson.size()));
        if(result != Figma::EResult::Ok)
        {
            std::fprintf(stderr, "loadBindingSidecarJson failed: %s\n", resultToString(result));
            return EXIT_FAILURE;
        }
    }

    std::size_t nodeCount = 0;
    std::size_t frameCount = 0;
    const Figma::Document * debugDocument = privateDocument(document.get());
    const Figma::DocumentInspectionInterface * inspection = debugDocument->getInspection();
    std::vector<const Figma::CanvasNodeDesc *> matches;
    if(inspection != nullptr)
    {
        if(const Figma::CanvasNodeDesc * root = inspection->getCanvasRoot())
        {
            countNodes(root, &nodeCount, &frameCount);
            collectNodeMatches(root, options, &matches);
        }
    }

    Figma::PlayerInterface * playerPtr = nullptr;
    std::unique_ptr<Figma::PlayerInterface, FigmaInterfaceDeleter<Figma::PlayerInterface>> player;
    if(options.renderList == true)
    {
        Figma::PlayerDesc playerDesc;
        const Figma::CanvasNodeDesc * viewportFrame = nullptr;
        if(options.frameId != nullptr)
        {
            playerDesc.startFrameId = options.frameId;
            viewportFrame = inspection != nullptr ? inspection->findCanvasNode(options.frameId) : nullptr;
        }
        if(viewportFrame == nullptr)
        {
            viewportFrame = inspection != nullptr ? inspection->getPrototypeStartFrame() : nullptr;
        }
        if(viewportFrame != nullptr)
        {
            const Figma::Rectf rect = viewportFrame->rect;
            playerDesc.viewport.width = std::max(1.0f, rect.w);
            playerDesc.viewport.height = std::max(1.0f, rect.h);
        }
        result = runtime->createPlayer(document.get(), playerDesc, &playerPtr);
        if(result != Figma::EResult::Ok)
        {
            std::fprintf(stderr, "createPlayer failed: %s\n", resultToString(result));
            return EXIT_FAILURE;
        }
        player.reset(playerPtr);
    }

    //////////////////////////////////////////////////////////////////////////
    std::printf("{\n");
    printJsonKey(2, "path");
    printJsonString(debugDocument->getPath());
    std::printf(",\n");
    printJsonKey(2, "fileName");
    printJsonString(debugDocument->getFileName());
    std::printf(",\n");
    printJsonKey(2, "canvasVersion");
    std::printf("%d,\n", static_cast<int>(debugDocument->getCanvasVersion()));
    printJsonKey(2, "hasCanvasBytes");
    std::printf("%s,\n", debugDocument->hasCanvasBytes() == true ? "true" : "false");
    printJsonKey(2, "thumbnailSize");
    //////////////////////////////////////////////////////////////////////////
    std::printf("{\"w\": %.3f, \"h\": %.3f},\n", debugDocument->getThumbnailSize().x, debugDocument->getThumbnailSize().y);
    printJsonKey(2, "assets");
    std::printf("%zu,\n", debugDocument->getAssets().size());
    printJsonKey(2, "nodes");
    std::printf("%zu,\n", nodeCount);
    printJsonKey(2, "frames");
    std::printf("%zu,\n", frameCount);
    printJsonKey(2, "prototypeStartFrame");
    if(const Figma::CanvasNodeDesc * prototypeFrame = inspection != nullptr ? inspection->getPrototypeStartFrame() : nullptr)
    {
        printNode(prototypeFrame, 2);
    }
    else
    {
        std::printf("null");
    }
    std::printf(",\n");
    printJsonKey(2, "matches");
    std::printf("[");
    if(matches.empty() == false)
    {
        std::printf("\n");
        const std::size_t matchSize = matches.size();
        for(std::size_t index = 0; index != matchSize; ++index)
        {
            printIndent(4);
            printNode(matches[index], 4);
            if(index + 1 != matchSize)
            {
                std::printf(",");
            }
            std::printf("\n");
        }
        printIndent(2);
    }
    std::printf("],\n");
    printJsonKey(2, "diagnostics");
    printDiagnostics(document->getDiagnostics(), 2);
    if(options.animations == true)
    {
        std::printf(",\n");
        printJsonKey(2, "prototypeAnimations");
        printAnimationNodes(inspection != nullptr ? inspection->getCanvasRoot() : nullptr, 2);
    }
    if(player != nullptr)
    {
        std::printf(",\n");
        printJsonKey(2, "renderList");
        printRenderList(player->getRenderList(), 2);
        std::printf(",\n");
        printJsonKey(2, "playerDiagnostics");
        printDiagnostics(player->getDiagnostics(), 2);
    }
    std::printf("\n}\n");

    return EXIT_SUCCESS;
}

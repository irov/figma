#include "Player.h"

#include "Figma/ActionRouter.h"

#include "PlayerFactory.h"
#include "RenderList.h"

#include "graphics/graphics.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <memory>
#include <new>
#include <utility>

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    namespace Detail
    {
        using CanvasNodeDescPtrVector = FigmaVector<const CanvasNodeDesc *>;
        //////////////////////////////////////////////////////////////////////////
        static constexpr float OneDiv255[] = {
            0.f / 255.f, 1.f / 255.f, 2.f / 255.f, 3.f / 255.f, 4.f / 255.f, 5.f / 255.f, 6.f / 255.f, 7.f / 255.f, 8.f / 255.f, 9.f / 255.f,
            10.f / 255.f, 11.f / 255.f, 12.f / 255.f, 13.f / 255.f, 14.f / 255.f, 15.f / 255.f, 16.f / 255.f, 17.f / 255.f, 18.f / 255.f, 19.f / 255.f,
            20.f / 255.f, 21.f / 255.f, 22.f / 255.f, 23.f / 255.f, 24.f / 255.f, 25.f / 255.f, 26.f / 255.f, 27.f / 255.f, 28.f / 255.f, 29.f / 255.f,
            30.f / 255.f, 31.f / 255.f, 32.f / 255.f, 33.f / 255.f, 34.f / 255.f, 35.f / 255.f, 36.f / 255.f, 37.f / 255.f, 38.f / 255.f, 39.f / 255.f,
            40.f / 255.f, 41.f / 255.f, 42.f / 255.f, 43.f / 255.f, 44.f / 255.f, 45.f / 255.f, 46.f / 255.f, 47.f / 255.f, 48.f / 255.f, 49.f / 255.f,
            50.f / 255.f, 51.f / 255.f, 52.f / 255.f, 53.f / 255.f, 54.f / 255.f, 55.f / 255.f, 56.f / 255.f, 57.f / 255.f, 58.f / 255.f, 59.f / 255.f,
            60.f / 255.f, 61.f / 255.f, 62.f / 255.f, 63.f / 255.f, 64.f / 255.f, 65.f / 255.f, 66.f / 255.f, 67.f / 255.f, 68.f / 255.f, 69.f / 255.f,
            70.f / 255.f, 71.f / 255.f, 72.f / 255.f, 73.f / 255.f, 74.f / 255.f, 75.f / 255.f, 76.f / 255.f, 77.f / 255.f, 78.f / 255.f, 79.f / 255.f,
            80.f / 255.f, 81.f / 255.f, 82.f / 255.f, 83.f / 255.f, 84.f / 255.f, 85.f / 255.f, 86.f / 255.f, 87.f / 255.f, 88.f / 255.f, 89.f / 255.f,
            90.f / 255.f, 91.f / 255.f, 92.f / 255.f, 93.f / 255.f, 94.f / 255.f, 95.f / 255.f, 96.f / 255.f, 97.f / 255.f, 98.f / 255.f, 99.f / 255.f,
            100.f / 255.f, 101.f / 255.f, 102.f / 255.f, 103.f / 255.f, 104.f / 255.f, 105.f / 255.f, 106.f / 255.f, 107.f / 255.f, 108.f / 255.f, 109.f / 255.f,
            110.f / 255.f, 111.f / 255.f, 112.f / 255.f, 113.f / 255.f, 114.f / 255.f, 115.f / 255.f, 116.f / 255.f, 117.f / 255.f, 118.f / 255.f, 119.f / 255.f,
            120.f / 255.f, 121.f / 255.f, 122.f / 255.f, 123.f / 255.f, 124.f / 255.f, 125.f / 255.f, 126.f / 255.f, 127.f / 255.f, 128.f / 255.f, 129.f / 255.f,
            130.f / 255.f, 131.f / 255.f, 132.f / 255.f, 133.f / 255.f, 134.f / 255.f, 135.f / 255.f, 136.f / 255.f, 137.f / 255.f, 138.f / 255.f, 139.f / 255.f,
            140.f / 255.f, 141.f / 255.f, 142.f / 255.f, 143.f / 255.f, 144.f / 255.f, 145.f / 255.f, 146.f / 255.f, 147.f / 255.f, 148.f / 255.f, 149.f / 255.f,
            150.f / 255.f, 151.f / 255.f, 152.f / 255.f, 153.f / 255.f, 154.f / 255.f, 155.f / 255.f, 156.f / 255.f, 157.f / 255.f, 158.f / 255.f, 159.f / 255.f,
            160.f / 255.f, 161.f / 255.f, 162.f / 255.f, 163.f / 255.f, 164.f / 255.f, 165.f / 255.f, 166.f / 255.f, 167.f / 255.f, 168.f / 255.f, 169.f / 255.f,
            170.f / 255.f, 171.f / 255.f, 172.f / 255.f, 173.f / 255.f, 174.f / 255.f, 175.f / 255.f, 176.f / 255.f, 177.f / 255.f, 178.f / 255.f, 179.f / 255.f,
            180.f / 255.f, 181.f / 255.f, 182.f / 255.f, 183.f / 255.f, 184.f / 255.f, 185.f / 255.f, 186.f / 255.f, 187.f / 255.f, 188.f / 255.f, 189.f / 255.f,
            190.f / 255.f, 191.f / 255.f, 192.f / 255.f, 193.f / 255.f, 194.f / 255.f, 195.f / 255.f, 196.f / 255.f, 197.f / 255.f, 198.f / 255.f, 199.f / 255.f,
            200.f / 255.f, 201.f / 255.f, 202.f / 255.f, 203.f / 255.f, 204.f / 255.f, 205.f / 255.f, 206.f / 255.f, 207.f / 255.f, 208.f / 255.f, 209.f / 255.f,
            210.f / 255.f, 211.f / 255.f, 212.f / 255.f, 213.f / 255.f, 214.f / 255.f, 215.f / 255.f, 216.f / 255.f, 217.f / 255.f, 218.f / 255.f, 219.f / 255.f,
            220.f / 255.f, 221.f / 255.f, 222.f / 255.f, 223.f / 255.f, 224.f / 255.f, 225.f / 255.f, 226.f / 255.f, 227.f / 255.f, 228.f / 255.f, 229.f / 255.f,
            230.f / 255.f, 231.f / 255.f, 232.f / 255.f, 233.f / 255.f, 234.f / 255.f, 235.f / 255.f, 236.f / 255.f, 237.f / 255.f, 238.f / 255.f, 239.f / 255.f,
            240.f / 255.f, 241.f / 255.f, 242.f / 255.f, 243.f / 255.f, 244.f / 255.f, 245.f / 255.f, 246.f / 255.f, 247.f / 255.f, 248.f / 255.f, 249.f / 255.f,
            250.f / 255.f, 251.f / 255.f, 252.f / 255.f, 253.f / 255.f, 254.f / 255.f, 255.f / 255.f
        };
        //////////////////////////////////////////////////////////////////////////
        static constexpr float makeColorChannel8(std::uint8_t _channel)
        {
            return OneDiv255[_channel];
        }
        //////////////////////////////////////////////////////////////////////////
        static bool contains(const Rectf & _rect, float _x, float _y)
        {
            return _x >= _rect.x && _y >= _rect.y && _x <= _rect.x + _rect.w && _y <= _rect.y + _rect.h;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool isSmartAnimateLayerMatch(const CanvasNodeDesc & _sourceNode, const CanvasNodeDesc & _targetNode)
        {
            return _sourceNode.type == _targetNode.type && _sourceNode.name.empty() == false && _sourceNode.name == _targetNode.name;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool isTargetNodeUsed(const CanvasNodeDescPtrVector & _usedTargets, const CanvasNodeDesc * const _targetNode)
        {
            return std::find(_usedTargets.begin(), _usedTargets.end(), _targetNode) != _usedTargets.end();
        }
        //////////////////////////////////////////////////////////////////////////
        struct DissolvePersistentNodeDesc
        {
            const CanvasNodeDesc * source = nullptr;
            const CanvasNodeDesc * target = nullptr;
        };
        //////////////////////////////////////////////////////////////////////////
        using DissolvePersistentNodeVector = FigmaVector<DissolvePersistentNodeDesc>;
        //////////////////////////////////////////////////////////////////////////
        static bool almostEqual(float _left, float _right, float _tolerance)
        {
            return std::fabs(_left - _right) <= _tolerance;
        }
        //////////////////////////////////////////////////////////////////////////
        static Rectf frameLocalRect(const Rectf & _rect, const Rectf & _frameRect)
        {
            return {
                _rect.x - _frameRect.x,
                _rect.y - _frameRect.y,
                _rect.w,
                _rect.h
            };
        }
        //////////////////////////////////////////////////////////////////////////
        static bool rectAlmostEqual(const Rectf & _left, const Rectf & _right, float _tolerance)
        {
            return almostEqual(_left.x, _right.x, _tolerance) == true && almostEqual(_left.y, _right.y, _tolerance) == true &&
                almostEqual(_left.w, _right.w, _tolerance) == true && almostEqual(_left.h, _right.h, _tolerance) == true;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool hasDissolvePersistentIdentity(const CanvasNodeDesc & _sourceNode, const CanvasNodeDesc & _targetNode)
        {
            if(_sourceNode.symbolId.empty() == false && _sourceNode.symbolId == _targetNode.symbolId)
            {
                return true;
            }

            return _sourceNode.id.empty() == false && _sourceNode.id == _targetNode.id;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool isDissolvePersistentNodeMatch(const CanvasNodeDesc & _sourceNode, const CanvasNodeDesc & _targetNode, const Rectf & _sourceFrameRect, const Rectf & _targetFrameRect)
        {
            if(_sourceNode.type != _targetNode.type || hasDissolvePersistentIdentity(_sourceNode, _targetNode) == false)
            {
                return false;
            }

            const Rectf sourceRect = frameLocalRect(_sourceNode.rect, _sourceFrameRect);
            const Rectf targetRect = frameLocalRect(_targetNode.rect, _targetFrameRect);
            return rectAlmostEqual(sourceRect, targetRect, 1.0f);
        }
        //////////////////////////////////////////////////////////////////////////
        static bool containsNode(const CanvasNodeDesc & _root, const CanvasNodeDesc * const _node)
        {
            if(&_root == _node)
            {
                return true;
            }

            for(const CanvasNodeDesc & child : _root.children)
            {
                if(containsNode(child, _node) == true)
                {
                    return true;
                }
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool isInsideUsedTargetNode(const CanvasNodeDesc * const _node, const CanvasNodeDescPtrVector & _usedTargets)
        {
            for(const CanvasNodeDesc * const target : _usedTargets)
            {
                if(containsNode(*target, _node) == true)
                {
                    return true;
                }
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        static const CanvasNodeDesc * findDissolvePersistentTargetNode(const CanvasNodeDesc & _sourceNode, const CanvasNodeDesc & _targetNode, const Rectf & _sourceFrameRect, const Rectf & _targetFrameRect, const CanvasNodeDescPtrVector & _usedTargets)
        {
            if(isInsideUsedTargetNode(&_targetNode, _usedTargets) == true)
            {
                return nullptr;
            }

            if(isDissolvePersistentNodeMatch(_sourceNode, _targetNode, _sourceFrameRect, _targetFrameRect) == true)
            {
                return &_targetNode;
            }

            for(const CanvasNodeDesc & child : _targetNode.children)
            {
                const CanvasNodeDesc * found = findDissolvePersistentTargetNode(_sourceNode, child, _sourceFrameRect, _targetFrameRect, _usedTargets);
                if(found != nullptr)
                {
                    return found;
                }
            }

            return nullptr;
        }
        //////////////////////////////////////////////////////////////////////////
        static void collectDissolvePersistentNodes(const CanvasNodeDesc & _sourceNode, const CanvasNodeDesc & _targetFrame, const Rectf & _sourceFrameRect, const Rectf & _targetFrameRect, CanvasNodeDescPtrVector * const _usedTargets, DissolvePersistentNodeVector * const _nodes)
        {
            for(const CanvasNodeDesc & sourceChild : _sourceNode.children)
            {
                const CanvasNodeDesc * target = findDissolvePersistentTargetNode(sourceChild, _targetFrame, _sourceFrameRect, _targetFrameRect, *_usedTargets);
                if(target != nullptr)
                {
                    _usedTargets->emplace_back(target);
                    _nodes->push_back({&sourceChild, target});
                    continue;
                }

                collectDissolvePersistentNodes(sourceChild, _targetFrame, _sourceFrameRect, _targetFrameRect, _usedTargets, _nodes);
            }
        }
        //////////////////////////////////////////////////////////////////////////
        static const CanvasNodeDesc * findSmartAnimateChildMatch(const CanvasNodeDesc & _sourceChild, const CanvasNodeDesc & _targetParent, std::size_t _sourceIndex, const CanvasNodeDescPtrVector & _usedTargets)
        {
            if(_sourceChild.id.empty() == false)
            {
                for(const CanvasNodeDesc & targetChild : _targetParent.children)
                {
                    if(targetChild.id == _sourceChild.id && isTargetNodeUsed(_usedTargets, &targetChild) == false)
                    {
                        return &targetChild;
                    }
                }
            }

            if(_sourceIndex < _targetParent.children.size())
            {
                const CanvasNodeDesc & targetChild = _targetParent.children[_sourceIndex];
                if(isTargetNodeUsed(_usedTargets, &targetChild) == false && isSmartAnimateLayerMatch(_sourceChild, targetChild) == true)
                {
                    return &targetChild;
                }
            }

            for(const CanvasNodeDesc & targetChild : _targetParent.children)
            {
                if(isTargetNodeUsed(_usedTargets, &targetChild) == false && isSmartAnimateLayerMatch(_sourceChild, targetChild) == true)
                {
                    return &targetChild;
                }
            }

            return nullptr;
        }
        //////////////////////////////////////////////////////////////////////////
        static float clamp01(float _value)
        {
            return std::max(0.0f, std::min(1.0f, _value));
        }
        //////////////////////////////////////////////////////////////////////////
        static float lerp(float _from, float _to, float _t)
        {
            return _from + (_to - _from) * _t;
        }
        //////////////////////////////////////////////////////////////////////////
        static Rectf lerpRect(const Rectf & _from, const Rectf & _to, float _t)
        {
            return {
                lerp(_from.x, _to.x, _t),
                lerp(_from.y, _to.y, _t),
                lerp(_from.w, _to.w, _t),
                lerp(_from.h, _to.h, _t)
            };
        }
        //////////////////////////////////////////////////////////////////////////
        static float applyEasing(EAnimationEasing _easing, float _progress)
        {
            const float t = clamp01(_progress);
            switch(_easing)
            {
            case EAnimationEasing::Linear:
                return t;
            case EAnimationEasing::EaseIn:
                return t * t;
            case EAnimationEasing::EaseOut:
                return 1.0f - (1.0f - t) * (1.0f - t);
            case EAnimationEasing::EaseInOut:
                return t < 0.5f ? 2.0f * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 2.0f) * 0.5f;
            case EAnimationEasing::InCubic:
                return t * t * t;
            case EAnimationEasing::OutCubic:
                return 1.0f - std::pow(1.0f - t, 3.0f);
            case EAnimationEasing::InOutCubic:
                return t < 0.5f ? 4.0f * t * t * t : 1.0f - std::pow(-2.0f * t + 2.0f, 3.0f) * 0.5f;
            case EAnimationEasing::Unsupported:
                break;
            }

            return t;
        }
        //////////////////////////////////////////////////////////////////////////
        static BindingValue copyValue(FigmaMemoryResource * _memory, const BindingValue & _value)
        {
            BindingValue result(_memory);
            result.type = _value.type;
            result.stringValue = FigmaString(_value.stringValue.begin(), _value.stringValue.end(), _memory);
            result.numberValue = _value.numberValue;
            result.boolValue = _value.boolValue;
            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        static FigmaString valueAsText(FigmaMemoryResource * _memory, const BindingValue & _value)
        {
            switch(_value.type)
            {
            case EBindingValueType::Text:
            case EBindingValueType::Image:
                return FigmaString(_value.stringValue.begin(), _value.stringValue.end(), _memory);
            case EBindingValueType::Number:
            {
                char buffer[64] = {'\0'};
                const auto result = std::to_chars(buffer, buffer + sizeof(buffer), _value.numberValue);
                if(result.ec == std::errc())
                {
                    return FigmaString(buffer, result.ptr, _memory);
                }
                return FigmaString("0", _memory);
            }
            case EBindingValueType::Boolean:
                return FigmaString(_value.boolValue == true ? "true" : "false", _memory);
            case EBindingValueType::None:
                break;
            }

            return FigmaString(_memory);
        }
        //////////////////////////////////////////////////////////////////////////
        static bool valueAsVisible(const BindingValue & _value)
        {
            if(_value.type == EBindingValueType::Boolean)
            {
                return _value.boolValue;
            }

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        static ERenderShapeType renderShapeFromCanvasNode(ECanvasNodeType _type)
        {
            if(_type == ECanvasNodeType::Ellipse)
            {
                return ERenderShapeType::Ellipse;
            }

            if(_type == ECanvasNodeType::RoundedRectangle)
            {
                return ERenderShapeType::RoundedRectangle;
            }

            return ERenderShapeType::Rectangle;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool canRenderPrimitiveShapeGeometry(const CanvasNodeDesc & _node)
        {
            return _node.type == ECanvasNodeType::Rectangle || _node.type == ECanvasNodeType::RoundedRectangle ||
                _node.type == ECanvasNodeType::Ellipse || _node.type == ECanvasNodeType::Frame;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool hasDecodedPathGeometry(const CanvasPathVector & _paths)
        {
            for(const CanvasPathDesc & path : _paths)
            {
                if(path.commandsDecoded == true && path.commands.empty() == false)
                {
                    return true;
                }
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool hasCompoundPathGeometry(const CanvasPathVector & _paths)
        {
            for(const CanvasPathDesc & path : _paths)
            {
                std::uint32_t contourCount = 0;
                for(const CanvasPathCommandDesc & command : path.commands)
                {
                    if(command.type == ECanvasPathCommandType::MoveTo)
                    {
                        ++contourCount;
                        if(contourCount > 1)
                        {
                            return true;
                        }
                    }
                }
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool hasPathPaints(const CanvasPathVector & _paths)
        {
            for(const CanvasPathDesc & path : _paths)
            {
                if(path.paints.empty() == false)
                {
                    return true;
                }
            }

            return false;
        }
        //////////////////////////////////////////////////////////////////////////
        static ERenderTextAlignHorizontal renderHorizontalAlign(ECanvasTextAlignHorizontal _align)
        {
            switch(_align)
            {
            case ECanvasTextAlignHorizontal::Center:
                return ERenderTextAlignHorizontal::Center;
            case ECanvasTextAlignHorizontal::Right:
                return ERenderTextAlignHorizontal::Right;
            case ECanvasTextAlignHorizontal::Left:
                break;
            }

            return ERenderTextAlignHorizontal::Left;
        }
        //////////////////////////////////////////////////////////////////////////
        static ERenderTextAlignVertical renderVerticalAlign(ECanvasTextAlignVertical _align)
        {
            switch(_align)
            {
            case ECanvasTextAlignVertical::Center:
                return ERenderTextAlignVertical::Center;
            case ECanvasTextAlignVertical::Bottom:
                return ERenderTextAlignVertical::Bottom;
            case ECanvasTextAlignVertical::Top:
                break;
            }

            return ERenderTextAlignVertical::Top;
        }
        //////////////////////////////////////////////////////////////////////////
        struct GraphicsMemoryContext
        {
            FigmaMemoryResource * memory = nullptr;
        };
        //////////////////////////////////////////////////////////////////////////
        struct alignas(std::max_align_t) GraphicsMemoryHeader
        {
            std::size_t size = 0;
        };
        //////////////////////////////////////////////////////////////////////////
        static void * graphicsAlloc(gp_size_t _size, void * _userData)
        {
            GraphicsMemoryContext * context = static_cast<GraphicsMemoryContext *>(_userData);
            if(context == nullptr || context->memory == nullptr)
            {
                return nullptr;
            }

            const std::size_t totalSize = sizeof(GraphicsMemoryHeader) + _size;
            void * block = context->memory->allocate(totalSize, alignof(GraphicsMemoryHeader));
            GraphicsMemoryHeader * header = static_cast<GraphicsMemoryHeader *>(block);
            header->size = _size;

            return header + 1;
        }
        //////////////////////////////////////////////////////////////////////////
        static void graphicsFree(void * _ptr, void * _userData)
        {
            if(_ptr == nullptr)
            {
                return;
            }

            GraphicsMemoryContext * context = static_cast<GraphicsMemoryContext *>(_userData);
            if(context == nullptr || context->memory == nullptr)
            {
                return;
            }

            GraphicsMemoryHeader * header = static_cast<GraphicsMemoryHeader *>(_ptr) - 1;
            const std::size_t totalSize = sizeof(GraphicsMemoryHeader) + header->size;
            context->memory->deallocate(header, totalSize, alignof(GraphicsMemoryHeader));
        }
        //////////////////////////////////////////////////////////////////////////
        static void * graphicsRealloc(void * _ptr, gp_size_t _size, void * _userData)
        {
            if(_ptr == nullptr)
            {
                return graphicsAlloc(_size, _userData);
            }

            if(_size == 0)
            {
                graphicsFree(_ptr, _userData);
                return nullptr;
            }

            GraphicsMemoryHeader * oldHeader = static_cast<GraphicsMemoryHeader *>(_ptr) - 1;
            const std::size_t oldSize = oldHeader->size;
            void * newPtr = graphicsAlloc(_size, _userData);
            if(newPtr == nullptr)
            {
                return nullptr;
            }

            std::memcpy(newPtr, _ptr, std::min<std::size_t>(oldSize, _size));
            graphicsFree(_ptr, _userData);

            return newPtr;
        }
        //////////////////////////////////////////////////////////////////////////
        static Color colorFromArgb(gp_argb_t _argb)
        {
            return {
                makeColorChannel8(static_cast<std::uint8_t>((_argb >> 16) & 0xff)),
                makeColorChannel8(static_cast<std::uint8_t>((_argb >> 8) & 0xff)),
                makeColorChannel8(static_cast<std::uint8_t>(_argb & 0xff)),
                makeColorChannel8(static_cast<std::uint8_t>((_argb >> 24) & 0xff))
            };
        }
        //////////////////////////////////////////////////////////////////////////
        using GraphicsColorVector = FigmaVector<gp_argb_t>;
        //////////////////////////////////////////////////////////////////////////
        static bool renderGraphicsCanvasToCommand(FigmaMemoryResource * _memory, gp_canvas_t * _canvas, RenderCommand * const _command)
        {
            gp_mesh_t mesh;
            if(gp_calculate_mesh_size(_canvas, &mesh) == GP_FAILURE)
            {
                return false;
            }

            if(mesh.vertex_count == 0 || mesh.index_count == 0)
            {
                return false;
            }

            GraphicsColorVector colors(_memory);
            _command->vertices.resize(mesh.vertex_count);
            _command->indices.resize(mesh.index_count);
            colors.resize(mesh.vertex_count);

            mesh.color.r = 1.0f;
            mesh.color.g = 1.0f;
            mesh.color.b = 1.0f;
            mesh.color.a = 1.0f;

            mesh.positions_buffer = _command->vertices.data();
            mesh.positions_offset = offsetof(RenderVertex, x);
            mesh.positions_stride = sizeof(RenderVertex);

            mesh.colors_buffer = colors.data();
            mesh.colors_offset = 0;
            mesh.colors_stride = sizeof(gp_argb_t);

            mesh.uv_buffer = _command->vertices.data();
            mesh.uv_offset = offsetof(RenderVertex, u);
            mesh.uv_stride = sizeof(RenderVertex);

            mesh.indices_buffer = _command->indices.data();
            mesh.indices_offset = 0;
            mesh.indices_stride = sizeof(std::uint16_t);

            if(gp_render(_canvas, &mesh) == GP_FAILURE)
            {
                _command->vertices.clear();
                _command->indices.clear();
                return false;
            }

            const std::size_t vertexSize = _command->vertices.size();
            for(std::size_t index = 0; index != vertexSize; ++index)
            {
                _command->vertices[index].color = colorFromArgb(colors[index]);
            }

            _command->opacity = 1.0f;

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        static Rectf alignedStrokeRect(const Rectf & _rect, float _strokeWidth, ECanvasStrokeAlign _strokeAlign)
        {
            if(_strokeAlign == ECanvasStrokeAlign::Inside)
            {
                const float inset = _strokeWidth * 0.5f;
                return {
                    _rect.x + inset,
                    _rect.y + inset,
                    std::max(0.0f, _rect.w - _strokeWidth),
                    std::max(0.0f, _rect.h - _strokeWidth)
                };
            }

            if(_strokeAlign == ECanvasStrokeAlign::Outside)
            {
                const float outset = _strokeWidth * 0.5f;
                return {
                    _rect.x - outset,
                    _rect.y - outset,
                    _rect.w + _strokeWidth,
                    _rect.h + _strokeWidth
                };
            }

            return _rect;
        }
        //////////////////////////////////////////////////////////////////////////
        static float alignedStrokeCornerRadius(float _cornerRadius, float _strokeWidth, ECanvasStrokeAlign _strokeAlign)
        {
            if(_strokeAlign == ECanvasStrokeAlign::Inside)
            {
                return std::max(0.0f, _cornerRadius - _strokeWidth * 0.5f);
            }

            if(_strokeAlign == ECanvasStrokeAlign::Outside)
            {
                return _cornerRadius + _strokeWidth * 0.5f;
            }

            return _cornerRadius;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool buildShapeMesh(FigmaMemoryResource * _memory, RenderCommand * const _command, bool _fill, ECanvasStrokeAlign _strokeAlign = ECanvasStrokeAlign::Center)
        {
            GraphicsMemoryContext context{_memory};
            gp_canvas_t * canvas = nullptr;
            if(gp_canvas_create(&canvas, &graphicsAlloc, &graphicsRealloc, &graphicsFree, &context) == GP_FAILURE || canvas == nullptr)
            {
                return false;
            }

            const Color color = _command->color;
            const float alpha = std::max(0.0f, std::min(1.0f, color.a * _command->opacity));
            const float strokeWidth = std::max(0.0f, _command->strokeWidth);
            gp_set_color(canvas, color.r, color.g, color.b, alpha);
            gp_set_thickness(canvas, strokeWidth);
            gp_set_penumbra(canvas, _fill == false && strokeWidth > 0.0f ? std::min(0.25f, strokeWidth * 0.25f) : 0.0f);
            gp_set_curve_quality(canvas, 24);
            gp_set_ellipse_quality(canvas, 64);
            gp_set_rect_quality(canvas, 16);

            if(_fill == true)
            {
                gp_begin_fill(canvas);
            }

            const Rectf rect = _fill == true ? _command->rect : alignedStrokeRect(_command->rect, strokeWidth, _strokeAlign);
            const float cornerRadius = _fill == true ? _command->cornerRadius : alignedStrokeCornerRadius(_command->cornerRadius, strokeWidth, _strokeAlign);
            if(_command->shape == ERenderShapeType::Ellipse)
            {
                const float centerX = rect.x + rect.w * 0.5f;
                const float centerY = rect.y + rect.h * 0.5f;
                const float radiusX = rect.w * 0.5f;
                const float radiusY = rect.h * 0.5f;
                if(_command->hasArcDataValue == true)
                {
                    const float innerRatio = std::max(0.0f, std::min(1.0f, _command->arcInnerRadius));
                    gp_set_inner_radius(canvas, innerRatio);
                    if(_fill == true)
                    {
                        gp_ellipse_arc(canvas, centerX, centerY, radiusX, radiusY, _command->arcStartingAngle, _command->arcEndingAngle);
                    }
                    else
                    {
                        gp_set_inner_radius(canvas, 0.0f);
                        gp_ellipse_arc(canvas, centerX, centerY, radiusX, radiusY, _command->arcStartingAngle, _command->arcEndingAngle);
                    }
                }
                else
                {
                    gp_ellipse(canvas, centerX, centerY, radiusX, radiusY);
                }
            }
            else if(_command->shape == ERenderShapeType::RoundedRectangle && cornerRadius > 0.0f)
            {
                const float radius = std::min(cornerRadius, std::min(rect.w, rect.h) * 0.5f);
                gp_rounded_rect(canvas, rect.x, rect.y, rect.w, rect.h, radius);
            }
            else
            {
                gp_rect(canvas, rect.x, rect.y, rect.w, rect.h);
            }

            if(_fill == true)
            {
                gp_end_fill(canvas);
            }

            const bool result = renderGraphicsCanvasToCommand(_memory, canvas, _command);
            gp_canvas_destroy(canvas);

            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool samePoint(const Vec2f & _left, const Vec2f & _right)
        {
            return std::fabs(_left.x - _right.x) <= 0.001f && std::fabs(_left.y - _right.y) <= 0.001f;
        }
        //////////////////////////////////////////////////////////////////////////
        static Vec2f localPathPoint(const Rectf & _rect, const Vec2f & _point)
        {
            return {_rect.x + _point.x, _rect.y + _point.y};
        }
        //////////////////////////////////////////////////////////////////////////
        static bool appendPathToCanvas(gp_canvas_t * const _canvas, const Rectf & _rect, const CanvasPathDesc & _path, bool _fill)
        {
            if(_path.commandsDecoded == false || _path.commands.empty() == true)
            {
                return false;
            }

            bool hasContour = false;
            Vec2f start{};
            Vec2f current{};

            const std::size_t commandSize = _path.commands.size();
            for(std::size_t index = 0; index != commandSize; ++index)
            {
                const CanvasPathCommandDesc & command = _path.commands[index];
                switch(command.type)
                {
                case ECanvasPathCommandType::MoveTo:
                {
                    const Vec2f p = localPathPoint(_rect, command.p0);
                    if(gp_point_move_to(_canvas, p.x, p.y) == GP_FAILURE)
                    {
                        return false;
                    }

                    start = p;
                    current = p;
                    hasContour = true;
                } break;
                case ECanvasPathCommandType::LineTo:
                {
                    if(hasContour == false)
                    {
                        return false;
                    }

                    const Vec2f p = localPathPoint(_rect, command.p0);
                    const bool nextClosesContour = index + 1 < commandSize && _path.commands[index + 1].type == ECanvasPathCommandType::Close;
                    if(_fill == true && nextClosesContour == true && samePoint(p, start) == true)
                    {
                        current = p;
                        break;
                    }

                    if(gp_point_line_to(_canvas, p.x, p.y) == GP_FAILURE)
                    {
                        return false;
                    }

                    current = p;
                } break;
                case ECanvasPathCommandType::QuadraticTo:
                {
                    if(hasContour == false)
                    {
                        return false;
                    }

                    const Vec2f p0 = localPathPoint(_rect, command.p0);
                    const Vec2f p1 = localPathPoint(_rect, command.p1);
                    if(gp_point_quadratic_curve_to(_canvas, p0.x, p0.y, p1.x, p1.y) == GP_FAILURE)
                    {
                        return false;
                    }

                    current = p1;
                } break;
                case ECanvasPathCommandType::CubicTo:
                {
                    if(hasContour == false)
                    {
                        return false;
                    }

                    const Vec2f p0 = localPathPoint(_rect, command.p0);
                    const Vec2f p1 = localPathPoint(_rect, command.p1);
                    const Vec2f p2 = localPathPoint(_rect, command.p2);
                    if(gp_point_bezier_curve_to(_canvas, p0.x, p0.y, p1.x, p1.y, p2.x, p2.y) == GP_FAILURE)
                    {
                        return false;
                    }

                    current = p2;
                } break;
                case ECanvasPathCommandType::Close:
                {
                    if(hasContour == false)
                    {
                        break;
                    }

                    if(_fill == false && samePoint(current, start) == false)
                    {
                        if(gp_point_line_to(_canvas, start.x, start.y) == GP_FAILURE)
                        {
                            return false;
                        }
                    }

                    hasContour = false;
                } break;
                }
            }

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool buildPathMeshSpan(FigmaMemoryResource * _memory, RenderCommand * const _command, const CanvasPathDesc * const _paths, std::size_t _pathCount, bool _fill)
        {
            GraphicsMemoryContext context{_memory};
            gp_canvas_t * canvas = nullptr;
            if(gp_canvas_create(&canvas, &graphicsAlloc, &graphicsRealloc, &graphicsFree, &context) == GP_FAILURE || canvas == nullptr)
            {
                return false;
            }

            const Color color = _command->color;
            const float alpha = std::max(0.0f, std::min(1.0f, color.a * _command->opacity));
            gp_set_color(canvas, color.r, color.g, color.b, alpha);
            gp_set_penumbra(canvas, 0.0f);
            gp_set_thickness(canvas, std::max(0.0f, _command->strokeWidth));
            gp_set_curve_quality(canvas, 24);
            gp_set_ellipse_quality(canvas, 64);
            gp_set_rect_quality(canvas, 16);

            if(_fill == true)
            {
                gp_begin_fill(canvas);
            }

            bool appended = false;
            for(std::size_t index = 0; index != _pathCount; ++index)
            {
                const CanvasPathDesc & path = _paths[index];
                if(path.commandsDecoded == false || path.commands.empty() == true)
                {
                    continue;
                }

                if(appendPathToCanvas(canvas, _command->rect, path, _fill) == false)
                {
                    gp_canvas_destroy(canvas);
                    return false;
                }

                appended = true;
            }

            if(_fill == true)
            {
                gp_end_fill(canvas);
            }

            bool result = false;
            if(appended == true)
            {
                result = renderGraphicsCanvasToCommand(_memory, canvas, _command);
            }

            gp_canvas_destroy(canvas);
            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        struct PathPaintFillDesc
        {
            const CanvasPathDesc * path = nullptr;
            const CanvasPaint * paint = nullptr;
        };
        //////////////////////////////////////////////////////////////////////////
        using PathPaintFillVector = FigmaVector<PathPaintFillDesc>;
        //////////////////////////////////////////////////////////////////////////
        static bool buildPathPaintMesh(FigmaMemoryResource * _memory, RenderCommand * const _command, const PathPaintFillVector & _fills, float _opacity)
        {
            GraphicsMemoryContext context{_memory};
            gp_canvas_t * canvas = nullptr;
            if(gp_canvas_create(&canvas, &graphicsAlloc, &graphicsRealloc, &graphicsFree, &context) == GP_FAILURE || canvas == nullptr)
            {
                return false;
            }

            gp_set_penumbra(canvas, 0.0f);
            gp_set_thickness(canvas, std::max(0.0f, _command->strokeWidth));
            gp_set_curve_quality(canvas, 24);
            gp_set_ellipse_quality(canvas, 64);
            gp_set_rect_quality(canvas, 16);

            bool appended = false;
            for(const PathPaintFillDesc & fill : _fills)
            {
                if(fill.path == nullptr || fill.paint == nullptr || fill.path->commandsDecoded == false || fill.path->commands.empty() == true)
                {
                    continue;
                }

                const Color color = fill.paint->color;
                const float alpha = clamp01(color.a * _opacity * clamp01(fill.paint->opacity));
                gp_set_color(canvas, color.r, color.g, color.b, alpha);
                gp_begin_fill(canvas);
                if(appendPathToCanvas(canvas, _command->rect, *fill.path, true) == false)
                {
                    gp_canvas_destroy(canvas);
                    return false;
                }
                gp_end_fill(canvas);

                appended = true;
            }

            bool result = false;
            if(appended == true)
            {
                result = renderGraphicsCanvasToCommand(_memory, canvas, _command);
            }

            gp_canvas_destroy(canvas);
            return result;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool buildPathMesh(FigmaMemoryResource * _memory, RenderCommand * const _command, const CanvasPathVector & _paths, bool _fill)
        {
            return buildPathMeshSpan(_memory, _command, _paths.data(), _paths.size(), _fill);
        }
        //////////////////////////////////////////////////////////////////////////
        static bool buildPathMesh(FigmaMemoryResource * _memory, RenderCommand * const _command, const CanvasPathDesc & _path, bool _fill)
        {
            return buildPathMeshSpan(_memory, _command, &_path, 1, _fill);
        }
        //////////////////////////////////////////////////////////////////////////
        static bool containsText(const FigmaString & _value, FigmaStringView _needle)
        {
            return _value.find(_needle.data(), 0, _needle.size()) != FigmaString::npos;
        }
        //////////////////////////////////////////////////////////////////////////
        static int fontWeightFromStyle(const CanvasNodeDesc & _node)
        {
            if(containsText(_node.fontStyle, "Black") == true || containsText(_node.fontStyle, "Heavy") == true)
            {
                return 900;
            }

            if(containsText(_node.fontStyle, "ExtraBold") == true || containsText(_node.fontStyle, "UltraBold") == true)
            {
                return 800;
            }

            if(containsText(_node.fontStyle, "Bold") == true)
            {
                return 700;
            }

            if(containsText(_node.fontStyle, "SemiBold") == true || containsText(_node.fontStyle, "DemiBold") == true)
            {
                return 600;
            }

            if(containsText(_node.fontStyle, "Medium") == true)
            {
                return 500;
            }

            if(containsText(_node.fontStyle, "Light") == true)
            {
                return 300;
            }

            if(containsText(_node.fontStyle, "Thin") == true)
            {
                return 200;
            }

            return _node.fontWeight;
        }
        //////////////////////////////////////////////////////////////////////////
        static ERenderBlendMode renderBlendMode(ECanvasBlendMode _mode)
        {
            switch(_mode)
            {
            case ECanvasBlendMode::PassThrough:
                return ERenderBlendMode::PassThrough;
            case ECanvasBlendMode::Normal:
                return ERenderBlendMode::Normal;
            case ECanvasBlendMode::Multiply:
                return ERenderBlendMode::Multiply;
            case ECanvasBlendMode::Screen:
                return ERenderBlendMode::Screen;
            case ECanvasBlendMode::Overlay:
                return ERenderBlendMode::Overlay;
            case ECanvasBlendMode::Darken:
                return ERenderBlendMode::Darken;
            case ECanvasBlendMode::Lighten:
                return ERenderBlendMode::Lighten;
            case ECanvasBlendMode::ColorDodge:
                return ERenderBlendMode::ColorDodge;
            case ECanvasBlendMode::ColorBurn:
                return ERenderBlendMode::ColorBurn;
            case ECanvasBlendMode::SoftLight:
                return ERenderBlendMode::SoftLight;
            case ECanvasBlendMode::HardLight:
                return ERenderBlendMode::HardLight;
            case ECanvasBlendMode::Difference:
                return ERenderBlendMode::Difference;
            case ECanvasBlendMode::Exclusion:
                return ERenderBlendMode::Exclusion;
            case ECanvasBlendMode::Hue:
                return ERenderBlendMode::Hue;
            case ECanvasBlendMode::Saturation:
                return ERenderBlendMode::Saturation;
            case ECanvasBlendMode::Color:
                return ERenderBlendMode::Color;
            case ECanvasBlendMode::Luminosity:
                return ERenderBlendMode::Luminosity;
            case ECanvasBlendMode::Unsupported:
                break;
            }

            return ERenderBlendMode::Unsupported;
        }
        //////////////////////////////////////////////////////////////////////////
        static ERenderImageScaleMode renderImageScaleMode(ECanvasImageScaleMode _mode)
        {
            switch(_mode)
            {
            case ECanvasImageScaleMode::Stretch:
                return ERenderImageScaleMode::Stretch;
            case ECanvasImageScaleMode::Fit:
                return ERenderImageScaleMode::Fit;
            case ECanvasImageScaleMode::Fill:
                return ERenderImageScaleMode::Fill;
            case ECanvasImageScaleMode::Tile:
                return ERenderImageScaleMode::Tile;
            case ECanvasImageScaleMode::Unknown:
                break;
            }

            return ERenderImageScaleMode::Unknown;
        }
        //////////////////////////////////////////////////////////////////////////
        static void assignPaintMetadata(const CanvasPaint & _paint, RenderCommand * const _command)
        {
            _command->blendMode = renderBlendMode(_paint.blendMode);
            _command->imageScaleMode = renderImageScaleMode(_paint.imageScaleMode);
            _command->originalImageWidth = _paint.originalImageWidth;
            _command->originalImageHeight = _paint.originalImageHeight;
            _command->hasImageTransformValue = _paint.hasTransformValue;
            _command->hasFilterColorAdjustValue = _paint.hasFilterColorAdjustValue;
            _command->hasPaintFilterValue = _paint.hasPaintFilterValue;

            std::copy(std::begin(_paint.transform), std::end(_paint.transform), std::begin(_command->imageTransform));
            std::copy(std::begin(_paint.filterColorAdjust), std::end(_paint.filterColorAdjust), std::begin(_command->filterColorAdjust));
            std::copy(std::begin(_paint.paintFilter), std::end(_paint.paintFilter), std::begin(_command->paintFilter));
        }
        //////////////////////////////////////////////////////////////////////////
        static void applyNodeBlendMode(const CanvasNodeDesc & _node, RenderCommand * const _command)
        {
            if(_node.blendMode == ECanvasBlendMode::Normal || _node.blendMode == ECanvasBlendMode::PassThrough)
            {
                return;
            }

            if(_command->blendMode == ERenderBlendMode::Normal || _command->blendMode == ERenderBlendMode::PassThrough)
            {
                _command->blendMode = renderBlendMode(_node.blendMode);
            }
        }
        //////////////////////////////////////////////////////////////////////////
        static bool isFilterValueActive(float _value)
        {
            return std::fabs(_value) > 0.0001f;
        }
        //////////////////////////////////////////////////////////////////////////
        static void addUnsupportedImageFilterDiagnostics(const CanvasPaint & _paint, const CanvasNodeDesc & _node, DiagnosticsInterface * const _diagnostics)
        {
            struct FilterDiagnosticDesc
            {
                std::size_t index;
                const Char * code;
                const Char * message;
            };

            constexpr FilterDiagnosticDesc UnsupportedFilterColorAdjust[] = {
                {3, "fig_filter_detail_unsupported", "Decoded filterColorAdjust.detail is not implemented by the current viewer"},
                {5, "fig_filter_vignette_unsupported", "Decoded filterColorAdjust.vignette is not implemented by the current viewer"},
            };
            constexpr FilterDiagnosticDesc UnsupportedPaintFilter[] = {
                {3, "fig_filter_detail_unsupported", "Decoded paintFilter.detail is not implemented by the current viewer"},
                {5, "fig_filter_vignette_unsupported", "Decoded paintFilter.vignette is not implemented by the current viewer"},
            };

            if(_paint.hasFilterColorAdjustValue == true)
            {
                for(const FilterDiagnosticDesc & desc : UnsupportedFilterColorAdjust)
                {
                    if(isFilterValueActive(_paint.filterColorAdjust[desc.index]) == true)
                    {
                        _diagnostics->add(EDiagnosticSeverity::Warning, desc.code, desc.message, _node.id.c_str());
                    }
                }
            }

            if(_paint.hasPaintFilterValue == true)
            {
                for(const FilterDiagnosticDesc & desc : UnsupportedPaintFilter)
                {
                    if(isFilterValueActive(_paint.paintFilter[desc.index]) == true)
                    {
                        _diagnostics->add(EDiagnosticSeverity::Warning, desc.code, desc.message, _node.id.c_str());
                    }
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        static void assignArcData(const CanvasNodeDesc & _node, RenderCommand * const _command)
        {
            if(_node.arcData.valid == false)
            {
                return;
            }

            _command->hasArcDataValue = true;
            _command->arcStartingAngle = _node.arcData.startingAngle;
            _command->arcEndingAngle = _node.arcData.endingAngle;
            _command->arcInnerRadius = _node.arcData.innerRadius;
        }
        //////////////////////////////////////////////////////////////////////////
        static void insetImageUvsToTexelCenters(RenderCommand * const _command, float _sourceWidth, float _sourceHeight)
        {
            if(_command == nullptr || _command->vertices.empty() == true)
            {
                return;
            }

            float minU = _command->vertices.front().u;
            float maxU = _command->vertices.front().u;
            float minV = _command->vertices.front().v;
            float maxV = _command->vertices.front().v;
            for(const RenderVertex & vertex : _command->vertices)
            {
                minU = std::min(minU, vertex.u);
                maxU = std::max(maxU, vertex.u);
                minV = std::min(minV, vertex.v);
                maxV = std::max(maxV, vertex.v);
            }

            const float rangeU = maxU - minU;
            const float rangeV = maxV - minV;
            const float insetU = std::min(rangeU * 0.49f, 0.5f / std::max(1.0f, _sourceWidth));
            const float insetV = std::min(rangeV * 0.49f, 0.5f / std::max(1.0f, _sourceHeight));

            for(RenderVertex & vertex : _command->vertices)
            {
                if(rangeU > 0.000001f && insetU > 0.0f)
                {
                    const float t = (vertex.u - minU) / rangeU;
                    vertex.u = minU + insetU + t * std::max(0.0f, rangeU - insetU * 2.0f);
                }

                if(rangeV > 0.000001f && insetV > 0.0f)
                {
                    const float t = (vertex.v - minV) / rangeV;
                    vertex.v = minV + insetV + t * std::max(0.0f, rangeV - insetV * 2.0f);
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        static void addImageQuad(RenderCommand * const _command, const AssetDesc * _asset, const CanvasNodeDesc & _node)
        {
            const Rectf rect = _command->rect;
            float x0 = rect.x;
            float y0 = rect.y;
            float x1 = rect.x + rect.w;
            float y1 = rect.y + rect.h;
            float u0 = 0.0f;
            float v0 = 0.0f;
            float u1 = 1.0f;
            float v1 = 1.0f;

            const float sourceWidth = _asset != nullptr && _asset->width > 0 ? static_cast<float>(_asset->width) : std::max(1.0f, _command->rect.w);
            const float sourceHeight = _asset != nullptr && _asset->height > 0 ? static_cast<float>(_asset->height) : std::max(1.0f, _command->rect.h);
            const float sourceAspect = sourceWidth / std::max(1.0f, sourceHeight);
            const float targetAspect = rect.w / std::max(1.0f, rect.h);

            if(_command->imageScaleMode == ERenderImageScaleMode::Fill)
            {
                if(sourceAspect > targetAspect)
                {
                    const float croppedWidth = sourceHeight * targetAspect;
                    const float crop = (sourceWidth - croppedWidth) * 0.5f / sourceWidth;
                    u0 = crop;
                    u1 = 1.0f - crop;
                }
                else if(sourceAspect < targetAspect)
                {
                    const float croppedHeight = sourceWidth / std::max(0.001f, targetAspect);
                    const float crop = (sourceHeight - croppedHeight) * 0.5f / sourceHeight;
                    v0 = crop;
                    v1 = 1.0f - crop;
                }
            }
            else if(_command->imageScaleMode == ERenderImageScaleMode::Fit)
            {
                if(sourceAspect > targetAspect)
                {
                    const float height = rect.w / std::max(0.001f, sourceAspect);
                    y0 = rect.y + (rect.h - height) * 0.5f;
                    y1 = y0 + height;
                }
                else
                {
                    const float width = rect.h * sourceAspect;
                    x0 = rect.x + (rect.w - width) * 0.5f;
                    x1 = x0 + width;
                }

                _command->rect = {x0, y0, x1 - x0, y1 - y0};
            }

            const Color white{1.0f, 1.0f, 1.0f, _command->opacity};
            _command->vertices.resize(4);
            _command->vertices[0] = {_node.quad[0].x, _node.quad[0].y, u0, v0, white};
            _command->vertices[1] = {_node.quad[1].x, _node.quad[1].y, u1, v0, white};
            _command->vertices[2] = {_node.quad[2].x, _node.quad[2].y, u1, v1, white};
            _command->vertices[3] = {_node.quad[3].x, _node.quad[3].y, u0, v1, white};

            const float shiftX = rect.x - _node.rect.x;
            const float shiftY = rect.y - _node.rect.y;
            if(std::fabs(shiftX) > 0.0001f || std::fabs(shiftY) > 0.0001f)
            {
                for(RenderVertex & vertex : _command->vertices)
                {
                    vertex.x += shiftX;
                    vertex.y += shiftY;
                }
            }

            if(_command->hasImageTransformValue == true)
            {
                constexpr float LocalX[4] = {0.0f, 1.0f, 1.0f, 0.0f};
                constexpr float LocalY[4] = {0.0f, 0.0f, 1.0f, 1.0f};
                const std::size_t vertexSize = _command->vertices.size();
                for(std::size_t index = 0; index != vertexSize; ++index)
                {
                    RenderVertex & vertex = _command->vertices[index];
                    const float nx = LocalX[index];
                    const float ny = LocalY[index];
                    vertex.u = _command->imageTransform[0] * nx + _command->imageTransform[1] * ny + _command->imageTransform[2];
                    vertex.v = _command->imageTransform[3] * nx + _command->imageTransform[4] * ny + _command->imageTransform[5];
                }
            }

            insetImageUvsToTexelCenters(_command, sourceWidth, sourceHeight);
            _command->indices = {0, 1, 2, 0, 2, 3};
        }
        //////////////////////////////////////////////////////////////////////////
        static void addTextQuad(RenderCommand * const _command)
        {
            const Rectf rect = _command->rect;
            const float x0 = rect.x;
            const float y0 = rect.y;
            const float x1 = rect.x + rect.w;
            const float y1 = rect.y + rect.h;
            const Color white{1.0f, 1.0f, 1.0f, 1.0f};

            _command->vertices.resize(4);
            _command->vertices[0] = {x0, y0, 0.0f, 0.0f, white};
            _command->vertices[1] = {x1, y0, 1.0f, 0.0f, white};
            _command->vertices[2] = {x1, y1, 1.0f, 1.0f, white};
            _command->vertices[3] = {x0, y1, 0.0f, 1.0f, white};
            _command->indices = {0, 1, 2, 0, 2, 3};
        }
        //////////////////////////////////////////////////////////////////////////
        static const PrototypeActionDesc * findPrototypeTargetAction(const PrototypeInteractionDesc & _interaction)
        {
            for(const PrototypeActionDesc & action : _interaction.actions)
            {
                if(action.targetNodeId.empty() == true)
                {
                    continue;
                }

                if(action.connectionType == EPrototypeConnectionType::InternalNode &&
                    (action.navigationType == EPrototypeNavigationType::Navigate ||
                        action.navigationType == EPrototypeNavigationType::Overlay ||
                        action.navigationType == EPrototypeNavigationType::Swap))
                {
                    return &action;
                }
            }

            return nullptr;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool isTargetlessInternalPrototypeAction(const PrototypeActionDesc & _action)
        {
            return _action.targetNodeId.empty() == true && _action.unsupportedFields.empty() == true &&
                _action.connectionType == EPrototypeConnectionType::InternalNode &&
                (_action.navigationType == EPrototypeNavigationType::Navigate ||
                    _action.navigationType == EPrototypeNavigationType::Overlay ||
                    _action.navigationType == EPrototypeNavigationType::Swap);
        }
        //////////////////////////////////////////////////////////////////////////
        static bool hasOnlyTargetlessInternalPrototypeActions(const PrototypeInteractionDesc & _interaction)
        {
            if(_interaction.actions.empty() == true)
            {
                return false;
            }

            for(const PrototypeActionDesc & action : _interaction.actions)
            {
                if(isTargetlessInternalPrototypeAction(action) == false)
                {
                    return false;
                }
            }

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        static void addUnsupportedPrototypeFieldDiagnostics(const UnsupportedFieldVector & _fields, const Char * _code, const Char * _message, const CanvasNodeDesc & _node, DiagnosticsInterface * const _diagnostics)
        {
            for(const FigmaString & field : _fields)
            {
                FigmaString message(field.get_allocator().resource());
                message = _message;
                message += ": ";
                message += field;
                _diagnostics->add(EDiagnosticSeverity::Warning, _code, message.c_str(), _node.id.c_str());
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    Player::Hotspot::Hotspot(FigmaMemoryResource * _memory)
        : nodeId(_memory)
        , actionId(_memory)
        , targetFrameId(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    Player::AnimationRenderContext::AnimationRenderContext(FigmaMemoryResource * _memory)
        : targetNodes(_memory)
        , matchedNodeIds(_memory)
        , skipNodeIds(_memory)
        , opaqueNodeIds(_memory)
        , rootNodeId(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    Player::NodeSwapState::NodeSwapState(FigmaMemoryResource * _memory)
        : currentNodeId(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    Player::LocalAnimationState::LocalAnimationState(FigmaMemoryResource * _memory)
        : sourceNodeId(_memory)
        , fromNodeId(_memory)
        , targetNodeId(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    Player::Player(Document & _document, const PlayerDesc & _desc, FigmaMemoryResource * _memory)
        : m_document(_document)
        , m_desc(_desc)
        , m_memory(_memory)
        , m_renderList(_memory)
        , m_diagnostics(_memory)
        , m_hotspots(_memory)
        , m_hoveredNodeIds(_memory)
        , m_nodeSwaps(_memory)
        , m_localAnimations(_memory)
        , m_overrides(_memory)
        , m_currentFrameId(_memory)
        , m_animationState(_memory)
    {
        const CanvasNodeDesc * initialFrame = this->resolveInitialFrame();
        if(initialFrame != nullptr)
        {
            m_currentFrameId = initialFrame->id;
        }

        this->update(0.0f);
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::destroy()
    {
        delete this;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::setActionRouter(ActionRouterInterface * _router)
    {
        m_actionRouter = _router;
        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::setDataContext(DataContextInterface * _context)
    {
        m_dataContext = _context;
        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::inputPointer(const PointerEvent & _event)
    {
        if(m_animationState.active == true)
        {
            return EResult::Ok;
        }

        if(_event.type == EPointerEventType::Move)
        {
            NodeIdSet hoveredNow(m_memory);
            for(const Hotspot & hotspot : m_hotspots)
            {
                if(hotspot.eventType != EPrototypeEventType::Hover || Detail::contains(hotspot.rect, _event.x, _event.y) == false)
                {
                    continue;
                }

                hoveredNow.emplace(hotspot.nodeId);
                if(m_hoveredNodeIds.find(hotspot.nodeId) == m_hoveredNodeIds.end())
                {
                    EResult result = this->routePointerAction(hotspot, _event);
                    if(result != EResult::Ok)
                    {
                        return result;
                    }
                }
            }

            for(auto it = m_hoveredNodeIds.begin(); it != m_hoveredNodeIds.end();)
            {
                if(hoveredNow.find(*it) == hoveredNow.end())
                {
                    it = m_hoveredNodeIds.erase(it);
                }
                else
                {
                    ++it;
                }
            }

            for(const FigmaString & nodeId : hoveredNow)
            {
                m_hoveredNodeIds.emplace(nodeId);
            }

            return EResult::Ok;
        }

        if(_event.type != EPointerEventType::Up)
        {
            return EResult::Ok;
        }

        for(const Hotspot & hotspot : m_hotspots)
        {
            if(hotspot.eventType != EPrototypeEventType::Click)
            {
                continue;
            }

            if(Detail::contains(hotspot.rect, _event.x, _event.y) == true)
            {
                return this->routePointerAction(hotspot, _event);
            }
        }

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::inputKey(const KeyEvent & _event)
    {
        if(m_animationState.active == true)
        {
            return EResult::Ok;
        }

        if(_event.type != EKeyEventType::Down || m_actionRouter == nullptr)
        {
            return EResult::Ok;
        }

        ActionEvent event;
        event.inputKind = EActionInputKind::Key;
        event.currentFrameId = FigmaStringView(m_currentFrameId.data(), m_currentFrameId.size());
        event.key = _event;
        event.ud = m_desc.ud;

        ActionResponse response;
        return m_actionRouter->routeAction(event, &response);
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::update(float _dt)
    {
        m_time += std::max(0.0f, _dt);

        m_diagnostics.clear();
        const DiagnosticsInterface * documentDiagnostics = m_document.getDiagnostics();
        if(documentDiagnostics == nullptr)
        {
            return EResult::Ok;
        }

        for(const Diagnostic & diagnostic : documentDiagnostics->getItems())
        {
            m_diagnostics.add(diagnostic.severity, diagnostic.code.c_str(), diagnostic.message.c_str(), diagnostic.nodeId.c_str());
        }

        const float dt = std::max(0.0f, _dt);

        this->updateLocalAnimations(dt);

        if(m_animationState.active == true)
        {
            this->updateAnimation(dt);
        }
        else
        {
            this->updatePrototypeTimers(dt);
        }
        this->rebuildHotspots();
        this->rebuildRenderList();

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::restart()
    {
        m_currentFrameId.clear();
        if(const CanvasNodeDesc * initialFrame = this->resolveInitialFrame())
        {
            m_currentFrameId = initialFrame->id;
        }

        m_animationState = PlayerAnimationStateDesc(m_memory);
        m_hoveredNodeIds.clear();
        m_nodeSwaps.clear();
        m_localAnimations.clear();
        m_time = 0.0f;

        return this->update(0.0f);
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::setText(FigmaStringView _key, FigmaStringView _value)
    {
        BindingValue value(m_memory);
        value.type = EBindingValueType::Text;
        value.stringValue = FigmaString(_value.begin(), _value.end(), m_memory);
        return this->setBindingValue(_key, value);
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::setNumber(FigmaStringView _key, double _value)
    {
        BindingValue value(m_memory);
        value.type = EBindingValueType::Number;
        value.numberValue = _value;
        return this->setBindingValue(_key, value);
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::setVisible(FigmaStringView _key, bool _value)
    {
        BindingValue value(m_memory);
        value.type = EBindingValueType::Boolean;
        value.boolValue = _value;
        return this->setBindingValue(_key, value);
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::setEnabled(FigmaStringView _key, bool _value)
    {
        BindingValue value(m_memory);
        value.type = EBindingValueType::Boolean;
        value.boolValue = _value;
        return this->setBindingValue(_key, value);
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::setImage(FigmaStringView _key, FigmaStringView _assetId)
    {
        BindingValue value(m_memory);
        value.type = EBindingValueType::Image;
        value.stringValue = FigmaString(_assetId.begin(), _assetId.end(), m_memory);
        return this->setBindingValue(_key, value);
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::setState(FigmaStringView _key, bool _value)
    {
        BindingValue value(m_memory);
        value.type = EBindingValueType::Boolean;
        value.boolValue = _value;
        return this->setBindingValue(_key, value);
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::setBindingValue(FigmaStringView _key, const BindingValue & _value)
    {
        if(_key.empty() == true)
        {
            return EResult::InvalidArgument;
        }

        FigmaString key(_key.begin(), _key.end(), m_memory);
        m_overrides.erase(key);
        m_overrides.emplace(std::move(key), Detail::copyValue(m_memory, _value));

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::clearBindingValue(FigmaStringView _key)
    {
        if(_key.empty() == true)
        {
            return EResult::InvalidArgument;
        }

        FigmaString key(_key.begin(), _key.end(), m_memory);
        m_overrides.erase(key);

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    const RenderListInterface * Player::getRenderList() const
    {
        return &m_renderList;
    }
    //////////////////////////////////////////////////////////////////////////
    const DiagnosticsInterface * Player::getDiagnostics() const
    {
        return &m_diagnostics;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::routePointerAction(const Hotspot & _hotspot, const PointerEvent & _event)
    {
        if(m_actionRouter == nullptr)
        {
            if(_hotspot.prototypeAction != nullptr && _hotspot.prototypeAction->navigationType == EPrototypeNavigationType::Swap)
            {
                return this->swapNodeState(_hotspot.nodeId, _hotspot.nodeId, *_hotspot.prototypeAction, 0.0f);
            }

            if(_hotspot.targetFrameId.empty() == false)
            {
                return this->navigateToFrame(_hotspot.targetFrameId, _hotspot.prototypeAction, _hotspot.nodeId, 0.0f);
            }

            return EResult::Ok;
        }

        ActionEvent event;
        event.inputKind = EActionInputKind::Pointer;
        event.actionId = FigmaStringView(_hotspot.actionId.data(), _hotspot.actionId.size());
        event.sourceNodeId = FigmaStringView(_hotspot.nodeId.data(), _hotspot.nodeId.size());
        event.currentFrameId = FigmaStringView(m_currentFrameId.data(), m_currentFrameId.size());
        event.pointer = _event;
        event.ud = m_desc.ud;

        ActionResponse response;
        EResult result = m_actionRouter->routeAction(event, &response);
        if(result != EResult::Ok)
        {
            return result;
        }

        if(response.result == EActionResult::NavigateFrame || response.result == EActionResult::OpenOverlay)
        {
            if(response.targetFrameId.empty() == false)
            {
                const FigmaStringView hotspotTargetFrameId(_hotspot.targetFrameId.data(), _hotspot.targetFrameId.size());
                const PrototypeActionDesc * action = response.targetFrameId == hotspotTargetFrameId ? _hotspot.prototypeAction : nullptr;
                if(action != nullptr && action->navigationType == EPrototypeNavigationType::Swap)
                {
                    return this->swapNodeState(_hotspot.nodeId, _hotspot.nodeId, *action, 0.0f);
                }
                return this->navigateToFrame(response.targetFrameId, action, _hotspot.nodeId, 0.0f);
            }
            else if(_hotspot.targetFrameId.empty() == false)
            {
                if(_hotspot.prototypeAction != nullptr && _hotspot.prototypeAction->navigationType == EPrototypeNavigationType::Swap)
                {
                    return this->swapNodeState(_hotspot.nodeId, _hotspot.nodeId, *_hotspot.prototypeAction, 0.0f);
                }
                return this->navigateToFrame(_hotspot.targetFrameId, _hotspot.prototypeAction, _hotspot.nodeId, 0.0f);
            }
        }
        else if(response.result == EActionResult::AllowDefault && _hotspot.targetFrameId.empty() == false)
        {
            if(_hotspot.prototypeAction != nullptr && _hotspot.prototypeAction->navigationType == EPrototypeNavigationType::Swap)
            {
                return this->swapNodeState(_hotspot.nodeId, _hotspot.nodeId, *_hotspot.prototypeAction, 0.0f);
            }
            return this->navigateToFrame(_hotspot.targetFrameId, _hotspot.prototypeAction, _hotspot.nodeId, 0.0f);
        }

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::navigateToFrame(FigmaStringView _targetFrameId, const PrototypeActionDesc * _action, FigmaStringView _sourceNodeId, float _initialElapsed)
    {
        if(_targetFrameId.empty() == true)
        {
            return EResult::InvalidArgument;
        }

        if(_action != nullptr && _action->navigationType == EPrototypeNavigationType::Swap)
        {
            return this->swapNodeState(_sourceNodeId, _sourceNodeId, *_action, 0.0f);
        }

        const CanvasNodeDesc * target = m_document.findCanvasNodeDesc(_targetFrameId);
        if(target == nullptr)
        {
            m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_target_missing", "Prototype action target node was not found in decoded document", FigmaString(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory).c_str());
            return EResult::NotFound;
        }

        const CanvasNodeDesc * source = nullptr;
        if(m_currentFrameId.empty() == false)
        {
            source = m_document.findCanvasNodeDesc(m_currentFrameId);
        }

        if(source == nullptr)
        {
            source = m_document.getPrototypeStartFrameDesc();
        }

        if(source != nullptr && _action != nullptr && (_action->smartAnimate == true || _action->transitionType == EPrototypeTransitionType::SmartAnimate || _action->transitionDuration > 0.0f))
        {
            return this->beginPrototypeAnimation(*source, *target, *_action, _sourceNodeId, _initialElapsed);
        }

        m_currentFrameId = target->id;
        m_nodeSwaps.clear();
        m_localAnimations.clear();
        m_hoveredNodeIds.clear();
        m_time = 0.0f;

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::swapNodeState(FigmaStringView _sourceNodeId, FigmaStringView _fromNodeId, const PrototypeActionDesc & _action, float _initialElapsed)
    {
        if(_sourceNodeId.empty() == true || _action.targetNodeId.empty() == true)
        {
            return EResult::InvalidArgument;
        }

        const CanvasNodeDesc * target = m_document.findCanvasNodeDesc(_action.targetNodeId);
        if(target == nullptr)
        {
            m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_target_missing", "SWAP_STATE target node was not found in decoded document", FigmaString(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory).c_str());
            return EResult::NotFound;
        }

        if(_action.transitionEasing == EAnimationEasing::Unsupported)
        {
            m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_animation_easing_unsupported", "SWAP_STATE easing is not implemented; state is changed without visual tweening", FigmaString(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory).c_str());
        }

        const float duration = std::max(0.0f, _action.transitionDuration);
        const float initialElapsed = std::max(0.0f, _initialElapsed);

        if(duration > 0.0001f && _action.transitionEasing != EAnimationEasing::Unsupported && initialElapsed < duration)
        {
            return this->beginLocalAnimation(_sourceNodeId, _fromNodeId, *target, _action, initialElapsed);
        }

        FigmaString key(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory);
        auto [it, inserted] = m_nodeSwaps.try_emplace(std::move(key), m_memory);
        (void)inserted;
        it->second.currentNodeId = target->id;
        it->second.startedAt = m_time - std::max(0.0f, initialElapsed - duration);

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::beginPrototypeAnimation(const CanvasNodeDesc & _sourceFrame, const CanvasNodeDesc & _targetFrame, const PrototypeActionDesc & _action, FigmaStringView _sourceNodeId, float _initialElapsed)
    {
        const float duration = std::max(0.0f, _action.transitionDuration);
        const float initialElapsed = std::max(0.0f, _initialElapsed);
        if(duration <= 0.0001f || initialElapsed >= duration)
        {
            m_currentFrameId = _targetFrame.id;
            m_nodeSwaps.clear();
            m_localAnimations.clear();
            m_hoveredNodeIds.clear();
            m_time = std::max(0.0f, initialElapsed - duration);
            return EResult::Ok;
        }

        m_animationState = PlayerAnimationStateDesc(m_memory);
        m_animationState.active = true;
        m_animationState.elapsed = std::min(duration, initialElapsed);
        m_animationState.progress = Detail::clamp01(m_animationState.elapsed / std::max(0.0001f, duration));
        m_animationState.clip.id = _sourceFrame.id + FigmaString("->", m_memory) + _targetFrame.id;
        m_animationState.clip.sourceFrameId = _sourceFrame.id;
        m_animationState.clip.targetFrameId = _targetFrame.id;
        m_animationState.clip.sourceNodeId = FigmaString(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory);
        m_animationState.clip.duration = duration;
        const bool smartAnimate = _action.smartAnimate == true || _action.transitionType == EPrototypeTransitionType::SmartAnimate;
        m_animationState.clip.smartAnimate = smartAnimate;
        m_animationState.clip.source = smartAnimate == true ? EAnimationSource::SmartAnimate : EAnimationSource::PrototypeTransition;
        m_animationState.clip.transitionType = _action.transitionType;
        m_animationState.clip.transitionDirection = _action.transitionDirection;
        m_animationState.clip.easing = _action.transitionEasing;
        m_time = 0.0f;

        if(smartAnimate == true)
        {
            this->collectSmartAnimateTracks(_sourceFrame, _targetFrame);
        }

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::beginLocalAnimation(FigmaStringView _sourceNodeId, FigmaStringView _fromNodeId, const CanvasNodeDesc & _targetNode, const PrototypeActionDesc & _action, float _initialElapsed)
    {
        const CanvasNodeDesc * fromNode = m_document.findCanvasNodeDesc(_fromNodeId);
        if(fromNode == nullptr)
        {
            fromNode = m_document.findCanvasNodeDesc(_sourceNodeId);
        }

        if(fromNode == nullptr)
        {
            m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_source_missing", "SWAP_STATE source node was not found in decoded document", FigmaString(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory).c_str());
            return EResult::NotFound;
        }

        for(auto it = m_localAnimations.begin(); it != m_localAnimations.end();)
        {
            if(it->sourceNodeId == _sourceNodeId)
            {
                it = m_localAnimations.erase(it);
            }
            else
            {
                ++it;
            }
        }

        LocalAnimationState animation(m_memory);
        animation.sourceNodeId = FigmaString(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory);
        animation.fromNodeId = fromNode->id;
        animation.targetNodeId = _targetNode.id;
        animation.transitionType = _action.transitionType;
        animation.easing = _action.transitionEasing;
        animation.duration = std::max(0.0f, _action.transitionDuration);
        animation.elapsed = std::max(0.0f, std::min(animation.duration, _initialElapsed));
        animation.progress = Detail::clamp01(animation.elapsed / std::max(0.0001f, animation.duration));
        animation.active = true;
        m_localAnimations.emplace_back(std::move(animation));

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::completeAnimation()
    {
        if(m_animationState.active == false)
        {
            return;
        }

        m_currentFrameId = m_animationState.clip.targetFrameId;
        m_animationState = PlayerAnimationStateDesc(m_memory);
        m_nodeSwaps.clear();
        m_localAnimations.clear();
        m_hoveredNodeIds.clear();
        m_time = 0.0f;
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::completeLocalAnimation(LocalAnimationState * const _animation)
    {
        if(_animation == nullptr || _animation->active == false)
        {
            return;
        }

        FigmaString key(_animation->sourceNodeId.begin(), _animation->sourceNodeId.end(), m_memory);
        auto [it, inserted] = m_nodeSwaps.try_emplace(std::move(key), m_memory);
        (void)inserted;
        it->second.currentNodeId = _animation->targetNodeId;
        it->second.startedAt = m_time;
        _animation->active = false;
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::updateAnimation(float _dt)
    {
        if(m_animationState.active == false)
        {
            return;
        }

        m_animationState.elapsed += std::max(0.0f, _dt);
        m_animationState.progress = Detail::clamp01(m_animationState.elapsed / std::max(0.0001f, m_animationState.clip.duration));

        if(m_animationState.progress >= 1.0f)
        {
            this->completeAnimation();
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::updateLocalAnimations(float _dt)
    {
        for(LocalAnimationState & animation : m_localAnimations)
        {
            if(animation.active == false)
            {
                continue;
            }

            animation.elapsed += std::max(0.0f, _dt);
            animation.progress = Detail::clamp01(animation.elapsed / std::max(0.0001f, animation.duration));
            if(animation.progress >= 1.0f)
            {
                this->completeLocalAnimation(&animation);
            }
        }

        for(auto it = m_localAnimations.begin(); it != m_localAnimations.end();)
        {
            if(it->active == false)
            {
                it = m_localAnimations.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::collectSmartAnimateTracks(const CanvasNodeDesc & _sourceNode, const CanvasNodeDesc & _targetFrame)
    {
        Detail::CanvasNodeDescPtrVector usedTargets(m_memory);
        std::size_t sourceIndex = 0;
        for(const CanvasNodeDesc & child : _sourceNode.children)
        {
            const CanvasNodeDesc * targetChild = Detail::findSmartAnimateChildMatch(child, _targetFrame, sourceIndex, usedTargets);
            if(targetChild != nullptr)
            {
                usedTargets.emplace_back(targetChild);
                this->collectSmartAnimateTracksForPair(child, *targetChild, _sourceNode.rect, _targetFrame.rect);
            }

            ++sourceIndex;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::collectSmartAnimateTracksForPair(const CanvasNodeDesc & _sourceNode, const CanvasNodeDesc & _targetNode, const Rectf & _sourceFrameRect, const Rectf & _targetFrameRect)
    {
        if(_sourceNode.id.empty() == false && _targetNode.id.empty() == false)
        {
            const Rectf sourceRectInTargetFrame = {
                _targetFrameRect.x + (_sourceNode.rect.x - _sourceFrameRect.x),
                _targetFrameRect.y + (_sourceNode.rect.y - _sourceFrameRect.y),
                _sourceNode.rect.w,
                _sourceNode.rect.h
            };

            AnimationTrackDesc rectTrack(m_memory);
            rectTrack.nodeId = _sourceNode.id;
            rectTrack.targetNodeId = _targetNode.id;
            rectTrack.type = EAnimationTrackType::Rect;
            rectTrack.from[0] = sourceRectInTargetFrame.x;
            rectTrack.from[1] = sourceRectInTargetFrame.y;
            rectTrack.from[2] = sourceRectInTargetFrame.w;
            rectTrack.from[3] = sourceRectInTargetFrame.h;
            rectTrack.to[0] = _targetNode.rect.x;
            rectTrack.to[1] = _targetNode.rect.y;
            rectTrack.to[2] = _targetNode.rect.w;
            rectTrack.to[3] = _targetNode.rect.h;
            m_animationState.clip.tracks.emplace_back(std::move(rectTrack));

            AnimationTrackDesc opacityTrack(m_memory);
            opacityTrack.nodeId = _sourceNode.id;
            opacityTrack.targetNodeId = _targetNode.id;
            opacityTrack.type = EAnimationTrackType::Opacity;
            opacityTrack.from[0] = _sourceNode.opacity;
            opacityTrack.to[0] = _targetNode.opacity;
            m_animationState.clip.tracks.emplace_back(std::move(opacityTrack));
        }

        Detail::CanvasNodeDescPtrVector usedTargets(m_memory);
        std::size_t sourceIndex = 0;
        for(const CanvasNodeDesc & sourceChild : _sourceNode.children)
        {
            const CanvasNodeDesc * targetChild = Detail::findSmartAnimateChildMatch(sourceChild, _targetNode, sourceIndex, usedTargets);
            if(targetChild != nullptr)
            {
                usedTargets.emplace_back(targetChild);
                this->collectSmartAnimateTracksForPair(sourceChild, *targetChild, _sourceFrameRect, _targetFrameRect);
            }

            ++sourceIndex;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::updatePrototypeTimers(float _dt)
    {
        const CanvasNodeDesc * frame = nullptr;
        if(m_currentFrameId.empty() == false)
        {
            frame = m_document.findCanvasNodeDesc(m_currentFrameId);
        }

        if(frame == nullptr)
        {
            frame = m_document.getPrototypeStartFrameDesc();
        }

        if(frame == nullptr)
        {
            return;
        }

        for(const PrototypeInteractionDesc & interaction : frame->prototypeInteractions)
        {
            if(interaction.eventType != EPrototypeEventType::AfterTimeout)
            {
                continue;
            }

            if(m_time < std::max(0.0f, interaction.transitionTimeout))
            {
                continue;
            }

            const PrototypeActionDesc * action = Detail::findPrototypeTargetAction(interaction);
            if(action == nullptr)
            {
                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_action_unsupported", "AFTER_TIMEOUT interaction has no supported internal target action", frame->id.c_str());
                continue;
            }

            const CanvasNodeDesc * target = m_document.findCanvasNodeDesc(action->targetNodeId);
            if(target == nullptr)
            {
                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_target_missing", "Prototype action target node was not found in decoded document", frame->id.c_str());
                continue;
            }

            (void)target;
            const float triggerTime = std::max(0.0f, interaction.transitionTimeout);
            const float previousTime = std::max(0.0f, m_time - std::max(0.0f, _dt));
            const float initialElapsed = previousTime < triggerTime ? m_time - triggerTime : 0.0f;
            if(action->navigationType == EPrototypeNavigationType::Swap)
            {
                this->swapNodeState(frame->id, frame->id, *action, initialElapsed);
            }
            else
            {
                this->navigateToFrame(action->targetNodeId, action, frame->id, initialElapsed);
            }
            return;
        }

        for(const CanvasNodeDesc & child : frame->children)
        {
            if(this->updatePrototypeTimersForNode(child, *frame, child.id, 0.0f, _dt) == true)
            {
                return;
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    bool Player::updatePrototypeTimersForNode(const CanvasNodeDesc & _node, const CanvasNodeDesc & _frame, FigmaStringView _sourceNodeId, float _startedAt, float _dt)
    {
        (void)_frame;

        if(this->findLocalAnimation(_sourceNodeId) != nullptr)
        {
            return false;
        }

        const CanvasNodeDesc * currentNode = &_node;
        float startedAt = _startedAt;

        const NodeSwapState * swap = this->findNodeSwapState(_sourceNodeId);
        if(swap != nullptr)
        {
            const CanvasNodeDesc * swappedNode = m_document.findCanvasNodeDesc(swap->currentNodeId);
            if(swappedNode == nullptr)
            {
                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_target_missing", "SWAP_STATE current node was not found in decoded document", FigmaString(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory).c_str());
                return false;
            }

            currentNode = swappedNode;
            startedAt = swap->startedAt;
        }

        for(const PrototypeInteractionDesc & interaction : currentNode->prototypeInteractions)
        {
            if(interaction.eventType != EPrototypeEventType::AfterTimeout)
            {
                continue;
            }

            if(m_time - startedAt < std::max(0.0f, interaction.transitionTimeout))
            {
                continue;
            }

            const PrototypeActionDesc * action = Detail::findPrototypeTargetAction(interaction);
            if(action == nullptr)
            {
                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_action_unsupported", "Nested AFTER_TIMEOUT interaction has no supported internal target action", currentNode->id.c_str());
                continue;
            }

            const CanvasNodeDesc * target = m_document.findCanvasNodeDesc(action->targetNodeId);
            if(target == nullptr)
            {
                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_target_missing", "Prototype action target node was not found in decoded document", currentNode->id.c_str());
                continue;
            }

            (void)target;
            const float triggerTime = startedAt + std::max(0.0f, interaction.transitionTimeout);
            const float previousTime = std::max(0.0f, m_time - std::max(0.0f, _dt));
            const float initialElapsed = previousTime < triggerTime ? m_time - triggerTime : 0.0f;
            if(action->navigationType == EPrototypeNavigationType::Swap)
            {
                this->swapNodeState(_sourceNodeId, currentNode->id, *action, initialElapsed);
            }
            else
            {
                this->navigateToFrame(action->targetNodeId, action, currentNode->id, initialElapsed);
            }

            return true;
        }

        for(const CanvasNodeDesc & child : currentNode->children)
        {
            if(this->updatePrototypeTimersForNode(child, _frame, child.id, startedAt, _dt) == true)
            {
                return true;
            }
        }

        return false;
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::rebuildRenderList()
    {
        m_renderList.clear();

        const float width = std::max(1.0f, m_desc.viewport.width);
        const float height = std::max(1.0f, m_desc.viewport.height);

        const CanvasNodeDesc * prototypeFrame = nullptr;
        if(m_currentFrameId.empty() == false)
        {
            prototypeFrame = m_document.findCanvasNodeDesc(m_currentFrameId);
        }

        if(prototypeFrame == nullptr)
        {
            prototypeFrame = m_document.getPrototypeStartFrameDesc();
        }

        if(prototypeFrame == nullptr)
        {
            m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_start_missing", "Decoded prototypeStartNodeID/prototypeStartingPoint was not found; render list is empty");
            return;
        }

        RenderCommand & fill = m_renderList.addCommand(ERenderCommandType::Mesh);
        fill.id = "figma.viewport_fill";
        fill.rect = {0.0f, 0.0f, width, height};
        fill.color = {0.0f, 0.0f, 0.0f, 1.0f};
        fill.opacity = 1.0f;
        Detail::buildShapeMesh(m_memory, &fill, true);

        if(m_animationState.active == true)
        {
            const CanvasNodeDesc * sourceFrame = m_document.findCanvasNodeDesc(m_animationState.clip.sourceFrameId);
            const CanvasNodeDesc * targetFrame = m_document.findCanvasNodeDesc(m_animationState.clip.targetFrameId);
            const float progress = Detail::applyEasing(m_animationState.clip.easing, m_animationState.progress);

            if(sourceFrame == nullptr || targetFrame == nullptr)
            {
                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_animation_frame_missing", "Animation source or target frame was not found; completing transition", m_animationState.clip.sourceNodeId.c_str());
                this->completeAnimation();
                prototypeFrame = m_document.findCanvasNodeDesc(m_currentFrameId);
                if(prototypeFrame != nullptr)
                {
                    this->appendCanvasNode(*prototypeFrame, 1.0f, -prototypeFrame->rect.x, -prototypeFrame->rect.y, nullptr);
                }
            }
            else if(m_animationState.clip.easing == EAnimationEasing::Unsupported)
            {
                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_animation_easing_unsupported", "Prototype transition easing is not implemented; visual animation is skipped until transition completes", m_animationState.clip.sourceNodeId.c_str());
                this->appendCanvasNode(*sourceFrame, 1.0f, -sourceFrame->rect.x, -sourceFrame->rect.y, nullptr);
            }
            else if(m_animationState.clip.smartAnimate == true)
            {
                if(m_animationState.clip.tracks.empty() == true)
                {
                    m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_smart_animate_match_missing", "Smart Animate has no decoded node id or sibling layer matches in the decoded target frame; visual animation is skipped until transition completes", m_animationState.clip.sourceNodeId.c_str());
                    this->appendCanvasNode(*sourceFrame, 1.0f, -sourceFrame->rect.x, -sourceFrame->rect.y, nullptr);
                }
                else
                {
                    AnimationRenderContext sourceAnimation(m_memory);
                    sourceAnimation.rootNodeId = sourceFrame->id;
                    sourceAnimation.progress = progress;
                    sourceAnimation.smartAnimate = true;
                    sourceAnimation.targetPass = false;
                    sourceAnimation.skipRootGeometry = true;

                    AnimationRenderContext targetAnimation(m_memory);
                    targetAnimation.rootNodeId = targetFrame->id;
                    targetAnimation.progress = progress;
                    targetAnimation.smartAnimate = true;
                    targetAnimation.targetPass = true;

                    for(const AnimationTrackDesc & track : m_animationState.clip.tracks)
                    {
                        const FigmaString & targetNodeId = track.targetNodeId.empty() == false ? track.targetNodeId : track.nodeId;
                        sourceAnimation.matchedNodeIds.emplace(track.nodeId);
                        targetAnimation.matchedNodeIds.emplace(targetNodeId);
                        AnimatedNodeDesc & node = targetAnimation.targetNodes[targetNodeId];
                        node.matched = true;

                        if(track.type == EAnimationTrackType::Rect)
                        {
                            const Rectf from{track.from[0], track.from[1], track.from[2], track.from[3]};
                            const Rectf to{track.to[0], track.to[1], track.to[2], track.to[3]};
                            node.rect = Detail::lerpRect(from, to, progress);
                            node.hasRect = true;
                        }
                        else if(track.type == EAnimationTrackType::Opacity)
                        {
                            node.opacity = Detail::lerp(track.from[0], track.to[0], progress);
                            node.hasOpacity = true;
                        }
                    }

                    this->appendCanvasNode(*sourceFrame, 1.0f, -sourceFrame->rect.x, -sourceFrame->rect.y, &sourceAnimation);
                    this->appendCanvasNode(*targetFrame, 1.0f, -targetFrame->rect.x, -targetFrame->rect.y, &targetAnimation);
                }
            }
            else if(m_animationState.clip.transitionType == EPrototypeTransitionType::Dissolve)
            {
                Detail::CanvasNodeDescPtrVector usedTargets(m_memory);
                Detail::DissolvePersistentNodeVector persistentNodes(m_memory);
                Detail::collectDissolvePersistentNodes(*sourceFrame, *targetFrame, sourceFrame->rect, targetFrame->rect, &usedTargets, &persistentNodes);

                AnimationRenderContext sourceLayer(m_memory);
                sourceLayer.renderLayerId = 1;
                sourceLayer.renderLayerOpacity = 1.0f - progress;

                AnimationRenderContext targetLayer(m_memory);
                targetLayer.renderLayerId = 2;
                targetLayer.renderLayerOpacity = progress;

                for(const Detail::DissolvePersistentNodeDesc & persistentNode : persistentNodes)
                {
                    if(persistentNode.source != nullptr && persistentNode.source->id.empty() == false)
                    {
                        sourceLayer.skipNodeIds.emplace(persistentNode.source->id);
                    }

                    if(persistentNode.target != nullptr && persistentNode.target->id.empty() == false)
                    {
                        targetLayer.opaqueNodeIds.emplace(persistentNode.target->id);
                    }
                }

                this->appendCanvasNode(*sourceFrame, 1.0f, -sourceFrame->rect.x, -sourceFrame->rect.y, &sourceLayer);
                this->appendCanvasNode(*targetFrame, 1.0f, -targetFrame->rect.x, -targetFrame->rect.y, &targetLayer);
            }
            else
            {
                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_transition_type_unsupported", "Prototype transition type is not implemented; visual animation is skipped until transition completes", m_animationState.clip.sourceNodeId.c_str());
                this->appendCanvasNode(*sourceFrame, 1.0f, -sourceFrame->rect.x, -sourceFrame->rect.y, nullptr);
            }
        }
        else
        {
            this->appendCanvasNode(*prototypeFrame, 1.0f, -prototypeFrame->rect.x, -prototypeFrame->rect.y, nullptr);
        }

        for(const BindingDesc & itemDesc : m_document.getBindings())
        {
            const CanvasNodeDesc * node = m_document.findCanvasNodeDesc(itemDesc.nodeId);
            if(node == nullptr)
            {
                m_diagnostics.add(EDiagnosticSeverity::Warning, "ux_binding_node_missing", "Binding target node was not found in decoded document", itemDesc.nodeId.c_str());
            }
        }

        for(const Hotspot & hotspot : m_hotspots)
        {
            RenderCommand & command = m_renderList.addCommand(ERenderCommandType::DebugHotspot);
            command.id = "figma.hotspot";
            command.nodeId = hotspot.nodeId;
            command.text = hotspot.actionId;
            command.rect = hotspot.rect;
            command.color = {1.0f, 0.62f, 0.12f, 0.32f};
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::appendCanvasNode(const CanvasNodeDesc & _node, float _parentOpacity, float _offsetX, float _offsetY, const AnimationRenderContext * _animation, bool _renderLayerEnabled)
    {
        if(_node.visible == false || this->isNodeVisibleByBinding(_node) == false)
        {
            return;
        }

        if(_animation != nullptr && _node.id.empty() == false && _animation->skipNodeIds.find(_node.id) != _animation->skipNodeIds.end())
        {
            return;
        }

        bool renderLayerEnabled = _renderLayerEnabled;
        if(_animation != nullptr && _node.id.empty() == false && _animation->opaqueNodeIds.find(_node.id) != _animation->opaqueNodeIds.end())
        {
            renderLayerEnabled = false;
        }

        Rectf nodeRect = _node.rect;
        float nodeOpacity = _node.opacity;
        const bool animationRoot = _animation != nullptr && _animation->smartAnimate == true && _animation->rootNodeId == _node.id;
        bool skipOwnGeometry = false;
        const bool animationMatched = _animation != nullptr && _node.id.empty() == false && _animation->matchedNodeIds.find(_node.id) != _animation->matchedNodeIds.end();
        if(_animation != nullptr && _animation->smartAnimate == true)
        {
            if(_animation->targetPass == false)
            {
                if(animationMatched == true)
                {
                    return;
                }

                if(animationRoot == false)
                {
                    nodeOpacity *= 1.0f - _animation->progress;
                }
            }
            else
            {
                const auto found = _animation->targetNodes.find(_node.id);
                if(found != _animation->targetNodes.end())
                {
                    if(found->second.hasRect == true)
                    {
                        nodeRect = found->second.rect;
                    }

                    if(found->second.hasOpacity == true)
                    {
                        nodeOpacity = found->second.opacity;
                    }
                }
                else if(animationRoot == false)
                {
                    nodeOpacity *= _animation->progress;
                }
            }
        }

        auto assignRenderLayer = [_animation, renderLayerEnabled](RenderCommand * const _command)
        {
            if(renderLayerEnabled == true && _animation != nullptr && _animation->renderLayerId != 0)
            {
                _command->renderLayerId = _animation->renderLayerId;
                _command->renderLayerOpacity = _animation->renderLayerOpacity;
            }
        };

        if(_animation == nullptr)
        {
            const LocalAnimationState * localAnimation = this->findLocalAnimation(_node.id);
            if(localAnimation != nullptr)
            {
                const CanvasNodeDesc * fromNode = m_document.findCanvasNodeDesc(localAnimation->fromNodeId);
                const CanvasNodeDesc * targetNode = m_document.findCanvasNodeDesc(localAnimation->targetNodeId);
                if(fromNode == nullptr || targetNode == nullptr)
                {
                    m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_target_missing", "SWAP_STATE animation source or target node was not found in decoded document", _node.id.c_str());
                    return;
                }

                const float progress = Detail::applyEasing(localAnimation->easing, localAnimation->progress);
                AnimationRenderContext localRender(m_memory);
                this->appendCanvasNode(*fromNode, _parentOpacity * (1.0f - progress), _offsetX + nodeRect.x - fromNode->rect.x, _offsetY + nodeRect.y - fromNode->rect.y, &localRender, renderLayerEnabled);
                this->appendCanvasNode(*targetNode, _parentOpacity * progress, _offsetX + nodeRect.x - targetNode->rect.x, _offsetY + nodeRect.y - targetNode->rect.y, &localRender, renderLayerEnabled);
                return;
            }

            const NodeSwapState * swap = this->findNodeSwapState(_node.id);
            if(swap != nullptr)
            {
                const CanvasNodeDesc * swappedNode = m_document.findCanvasNodeDesc(swap->currentNodeId);
                if(swappedNode == nullptr)
                {
                    m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_target_missing", "SWAP_STATE current node was not found in decoded document", _node.id.c_str());
                    return;
                }

                AnimationRenderContext localRender(m_memory);
                this->appendCanvasNode(*swappedNode, _parentOpacity, _offsetX + nodeRect.x - swappedNode->rect.x, _offsetY + nodeRect.y - swappedNode->rect.y, &localRender, renderLayerEnabled);
                return;
            }
        }

        const float opacity = _parentOpacity * Detail::clamp01(nodeOpacity);
        if(opacity <= 0.001f)
        {
            return;
        }

        if(_node.mask == true)
        {
            m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_mask_unsupported", "Decoded mask node is not rendered until mask/clip composition is implemented", _node.id.c_str());
            return;
        }

        for(const PrototypeInteractionDesc & interaction : _node.prototypeInteractions)
        {
            if(interaction.eventType == EPrototypeEventType::Unsupported)
            {
                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_event_unsupported", "Prototype event type is not implemented; interaction skipped", _node.id.c_str());
            }
            Detail::addUnsupportedPrototypeFieldDiagnostics(interaction.unsupportedFields, "fig_prototype_event_field_unsupported", "Prototype event field is not implemented", _node, &m_diagnostics);

            for(const PrototypeActionDesc & action : interaction.actions)
            {
                Detail::addUnsupportedPrototypeFieldDiagnostics(action.unsupportedFields, "fig_prototype_action_field_unsupported", "Prototype action field is not implemented", _node, &m_diagnostics);
                if(action.connectionType == EPrototypeConnectionType::Unsupported || action.navigationType == EPrototypeNavigationType::Unsupported ||
                    action.navigationType == EPrototypeNavigationType::ScrollTo)
                {
                    m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_action_unsupported", "Prototype action field is not implemented; action skipped", _node.id.c_str());
                }
                else if(action.transitionType == EPrototypeTransitionType::Unsupported)
                {
                    m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_transition_type_unsupported", "Prototype transition type is not implemented; action will not draw a guessed transition", _node.id.c_str());
                }
                if(action.transitionPreserveScroll == true)
                {
                    m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_transition_preserve_scroll_unsupported", "Prototype transition preserve-scroll behavior is not implemented", _node.id.c_str());
                }
                if(action.transitionResetVideoPosition == true)
                {
                    m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_transition_reset_video_unsupported", "Prototype transition reset-video behavior is not implemented", _node.id.c_str());
                }
            }
        }

        const bool usePrimitiveShapeGeometry = Detail::canRenderPrimitiveShapeGeometry(_node);
        const bool hasFillPathGeometry = Detail::hasDecodedPathGeometry(_node.fillGeometry);
        const bool hasStrokePathGeometry = Detail::hasDecodedPathGeometry(_node.strokeGeometry);
        bool useFillPathGeometry = hasFillPathGeometry == true && usePrimitiveShapeGeometry == false;
        bool useStrokePathGeometry = hasStrokePathGeometry == true && usePrimitiveShapeGeometry == false;
        const bool unsupportedCompoundFillGeometry = useFillPathGeometry == true && Detail::hasCompoundPathGeometry(_node.fillGeometry) == true;
        if(unsupportedCompoundFillGeometry == true)
        {
            m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_path_compound_fill_unsupported", "Decoded fillGeometry has multiple contours; compound winding/hole triangulation is not implemented", _node.id.c_str());
            useFillPathGeometry = false;
        }
        const bool renderOwnGeometry = (_node.hasVectorDataValue == false && _node.type != ECanvasNodeType::Vector) || hasFillPathGeometry == true ||
            hasStrokePathGeometry == true || Detail::canRenderPrimitiveShapeGeometry(_node) == true;
        const bool unsupportedNodeBlendMode = renderOwnGeometry == true && _node.blendMode == ECanvasBlendMode::Unsupported;
        if(renderOwnGeometry == false)
        {
            m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_vector_network_unsupported", "Decoded vectorData has no decoded fillGeometry/strokeGeometry path commands; vectorNetworkBlob is not implemented", _node.id.c_str());
        }
        if(unsupportedNodeBlendMode == true)
        {
            m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_node_blend_mode_unsupported", "Node blendMode is not implemented; node geometry command skipped", _node.id.c_str());
            useFillPathGeometry = false;
            useStrokePathGeometry = false;
        }

        skipOwnGeometry = skipOwnGeometry == true || (animationRoot == true && _animation->skipRootGeometry == true);
        if(renderOwnGeometry == true && unsupportedNodeBlendMode == false && skipOwnGeometry == false)
        {
            const Rectf rect = {
                nodeRect.x + _offsetX,
                nodeRect.y + _offsetY,
                nodeRect.w,
                nodeRect.h
            };

            if(_node.type != ECanvasNodeType::Text)
            {
                const bool useFillPathPaints = useFillPathGeometry == true && Detail::hasPathPaints(_node.fillGeometry) == true;
                auto appendPathFill = [&](const CanvasPaint & paint, const CanvasPathDesc & path) {
                    if(paint.visible == false)
                    {
                        return;
                    }

                    if(paint.type == ECanvasPaintType::Unsupported)
                    {
                        m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_paint_type_unsupported", "Paint type is not implemented; command skipped", _node.id.c_str());
                        return;
                    }

                    if(paint.blendMode == ECanvasBlendMode::Unsupported)
                    {
                        m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_blend_mode_unsupported", "Paint blendMode is not implemented; command skipped", _node.id.c_str());
                        return;
                    }

                    if(paint.type != ECanvasPaintType::Solid)
                    {
                        m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_path_fill_paint_unsupported", "Path-level fill paint type is not implemented; command skipped", _node.id.c_str());
                        return;
                    }

                    RenderCommand & fill = m_renderList.addCommand(ERenderCommandType::Mesh);
                    assignRenderLayer(&fill);
                    fill.id = "figma.path_fill";
                    fill.nodeId = _node.id;
                    fill.rect = rect;
                    fill.color = paint.color;
                    fill.shape = Detail::renderShapeFromCanvasNode(_node.type);
                    fill.cornerRadius = _node.cornerRadius;
                    fill.opacity = opacity * std::max(0.0f, std::min(1.0f, paint.opacity));
                    Detail::assignPaintMetadata(paint, &fill);
                    Detail::applyNodeBlendMode(_node, &fill);
                    Detail::assignArcData(_node, &fill);
                    if(Detail::buildPathMesh(m_memory, &fill, path, true) == false)
                    {
                        m_renderList.removeLastCommand();
                        m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_shape_geometry_failed", "Unable to build graphics mesh for decoded path fill geometry; command skipped", _node.id.c_str());
                    }
                };

                auto collectGroupedPathFills = [&](Detail::PathPaintFillVector * const fills) {
                    if(_node.blendMode == ECanvasBlendMode::Normal || _node.blendMode == ECanvasBlendMode::PassThrough)
                    {
                        return false;
                    }

                    for(const CanvasPathDesc & path : _node.fillGeometry)
                    {
                        const CanvasPaintVector & paints = path.paints.empty() == false ? path.paints : _node.fills;
                        for(const CanvasPaint & paint : paints)
                        {
                            if(paint.visible == false)
                            {
                                continue;
                            }

                            if(paint.type != ECanvasPaintType::Solid)
                            {
                                return false;
                            }

                            if(paint.blendMode != ECanvasBlendMode::Normal && paint.blendMode != ECanvasBlendMode::PassThrough)
                            {
                                return false;
                            }

                            fills->push_back({&path, &paint});
                        }
                    }

                    return fills->size() > 1;
                };

                if(useFillPathPaints == true)
                {
                    Detail::PathPaintFillVector groupedPathFills(m_memory);
                    if(collectGroupedPathFills(&groupedPathFills) == true)
                    {
                        RenderCommand & fill = m_renderList.addCommand(ERenderCommandType::Mesh);
                        assignRenderLayer(&fill);
                        fill.id = "figma.path_fill_group";
                        fill.nodeId = _node.id;
                        fill.rect = rect;
                        fill.color = groupedPathFills.front().paint->color;
                        fill.shape = Detail::renderShapeFromCanvasNode(_node.type);
                        fill.cornerRadius = _node.cornerRadius;
                        fill.opacity = 1.0f;
                        Detail::applyNodeBlendMode(_node, &fill);
                        Detail::assignArcData(_node, &fill);
                        if(Detail::buildPathPaintMesh(m_memory, &fill, groupedPathFills, opacity) == false)
                        {
                            m_renderList.removeLastCommand();
                            m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_shape_geometry_failed", "Unable to build graphics mesh for decoded path fill geometry; command skipped", _node.id.c_str());
                        }
                    }
                    else
                    {
                        for(const CanvasPathDesc & path : _node.fillGeometry)
                        {
                            const CanvasPaintVector & paints = path.paints.empty() == false ? path.paints : _node.fills;
                            for(const CanvasPaint & paint : paints)
                            {
                                appendPathFill(paint, path);
                            }
                        }
                    }
                }
                else
                {
                    for(const CanvasPaint & paint : _node.fills)
                    {
                        if(paint.visible == false)
                        {
                            continue;
                        }

                        if(paint.type == ECanvasPaintType::Unsupported)
                        {
                            m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_paint_type_unsupported", "Paint type is not implemented; command skipped", _node.id.c_str());
                            continue;
                        }

                        if(paint.blendMode == ECanvasBlendMode::Unsupported)
                        {
                            m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_blend_mode_unsupported", "Paint blendMode is not implemented; command skipped", _node.id.c_str());
                            continue;
                        }

                        if(paint.type == ECanvasPaintType::Image)
                        {
                            if(paint.assetId.empty() == true)
                            {
                                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_image_asset_missing", "Image paint has no decoded image asset id; command skipped", _node.id.c_str());
                                continue;
                            }

                            const AssetDesc * asset = m_document.findAsset(paint.assetId);
                            if(asset == nullptr)
                            {
                                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_image_asset_missing", "Decoded image asset is not present in the .fig archive; command skipped", _node.id.c_str());
                                continue;
                            }

                            if(paint.imageScaleMode == ECanvasImageScaleMode::Tile || paint.imageScaleMode == ECanvasImageScaleMode::Unknown)
                            {
                                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_image_scale_mode_unsupported", "Image scale mode is not implemented; command skipped", _node.id.c_str());
                                continue;
                            }

                            FigmaString boundAssetId(m_memory);
                            const FigmaString * assetId = &paint.assetId;
                            if(this->resolveImageBinding(_node, &boundAssetId) == true)
                            {
                                const AssetDesc * boundAsset = m_document.findAsset(boundAssetId);
                                if(boundAsset == nullptr)
                                {
                                    m_diagnostics.add(EDiagnosticSeverity::Warning, "ux_binding_image_asset_missing", "Image binding resolved to an unknown asset id; decoded image command kept unchanged", _node.id.c_str());
                                }
                                else
                                {
                                    asset = boundAsset;
                                    assetId = &boundAssetId;
                                }
                            }

                            Detail::addUnsupportedImageFilterDiagnostics(paint, _node, &m_diagnostics);

                            RenderCommand & image = m_renderList.addCommand(ERenderCommandType::Image);
                            assignRenderLayer(&image);
                            image.id = "figma.image_fill";
                            image.nodeId = _node.id;
                            image.assetId = *assetId;
                            image.rect = rect;
                            image.shape = Detail::renderShapeFromCanvasNode(_node.type);
                            image.cornerRadius = _node.cornerRadius;
                            image.opacity = opacity * std::max(0.0f, std::min(1.0f, paint.opacity));
                            Detail::assignPaintMetadata(paint, &image);
                            Detail::applyNodeBlendMode(_node, &image);
                            Detail::addImageQuad(&image, asset, _node);
                        }
                        else if(paint.type == ECanvasPaintType::Solid)
                        {
                            if(useFillPathGeometry == false && usePrimitiveShapeGeometry == false)
                            {
                                if(unsupportedCompoundFillGeometry == false)
                                {
                                    m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_path_fill_unsupported", "Decoded fillGeometry cannot be rendered as a supported primitive or simple path", _node.id.c_str());
                                }
                                continue;
                            }

                            RenderCommand & fill = m_renderList.addCommand(ERenderCommandType::Mesh);
                            assignRenderLayer(&fill);
                            fill.id = useFillPathGeometry == true ? "figma.path_fill" : "figma.solid_fill";
                            fill.nodeId = _node.id;
                            fill.rect = rect;
                            fill.color = paint.color;
                            fill.shape = Detail::renderShapeFromCanvasNode(_node.type);
                            fill.cornerRadius = _node.cornerRadius;
                            fill.opacity = opacity * std::max(0.0f, std::min(1.0f, paint.opacity));
                            Detail::assignPaintMetadata(paint, &fill);
                            Detail::applyNodeBlendMode(_node, &fill);
                            Detail::assignArcData(_node, &fill);
                            const bool meshBuilt = useFillPathGeometry == true ? Detail::buildPathMesh(m_memory, &fill, _node.fillGeometry, true) : Detail::buildShapeMesh(m_memory, &fill, true);
                            if(meshBuilt == false)
                            {
                                m_renderList.removeLastCommand();
                                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_shape_geometry_failed", "Unable to build graphics mesh for decoded fill geometry; command skipped", _node.id.c_str());
                            }
                        }
                    }
                }

                for(const CanvasPaint & paint : _node.strokes)
                {
                    if(paint.visible == false)
                    {
                        continue;
                    }

                    if(_node.strokeWeight <= 0.001f)
                    {
                        continue;
                    }

                    if(paint.type != ECanvasPaintType::Solid)
                    {
                        m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_stroke_paint_unsupported", "Only solid stroke paints are implemented; stroke command skipped", _node.id.c_str());
                        continue;
                    }

                    if(paint.blendMode == ECanvasBlendMode::Unsupported)
                    {
                        m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_blend_mode_unsupported", "Paint blendMode is not implemented; stroke command skipped", _node.id.c_str());
                        continue;
                    }

                    if(usePrimitiveShapeGeometry == true && (_node.strokeAlign == ECanvasStrokeAlign::Unsupported ||
                        _node.strokeCap == ECanvasStrokeCap::Unsupported || _node.strokeJoin == ECanvasStrokeJoin::Unsupported ||
                        _node.dashPattern.empty() == false))
                    {
                        m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_stroke_metadata_unsupported", "Decoded stroke metadata is not implemented; stroke command skipped", _node.id.c_str());
                        continue;
                    }

                    if(useStrokePathGeometry == false && usePrimitiveShapeGeometry == false)
                    {
                        m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_path_stroke_unsupported", "Decoded strokeGeometry cannot be rendered as a supported primitive or path", _node.id.c_str());
                        continue;
                    }

                    RenderCommand & stroke = m_renderList.addCommand(ERenderCommandType::Mesh);
                    assignRenderLayer(&stroke);
                    stroke.id = useStrokePathGeometry == true ? "figma.path_stroke" : "figma.stroke";
                    stroke.nodeId = _node.id;
                    stroke.rect = rect;
                    stroke.color = paint.color;
                    stroke.shape = Detail::renderShapeFromCanvasNode(_node.type);
                    stroke.cornerRadius = _node.cornerRadius;
                    stroke.strokeWidth = _node.strokeWeight;
                    stroke.opacity = opacity * std::max(0.0f, std::min(1.0f, paint.opacity));
                    Detail::assignPaintMetadata(paint, &stroke);
                    Detail::applyNodeBlendMode(_node, &stroke);
                    Detail::assignArcData(_node, &stroke);
                    const bool meshBuilt = useStrokePathGeometry == true ? Detail::buildPathMesh(m_memory, &stroke, _node.strokeGeometry, true) : Detail::buildShapeMesh(m_memory, &stroke, false, _node.strokeAlign);
                    if(meshBuilt == false)
                    {
                        m_renderList.removeLastCommand();
                        m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_stroke_geometry_failed", "Unable to build graphics mesh for decoded stroke geometry; command skipped", _node.id.c_str());
                    }
                }
            }

            if(_node.type == ECanvasNodeType::Text && _node.text.empty() == false)
            {
                if(_node.textLines.empty() == true)
                {
                    m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_text_layout_missing", "Text node has no decoded derivedTextData/baselines; command skipped", _node.id.c_str());
                    return;
                }

                const CanvasPaint * textPaint = nullptr;
                for(const CanvasPaint & paint : _node.fills)
                {
                    if(paint.visible == false)
                    {
                        continue;
                    }

                    textPaint = &paint;
                    break;
                }

                if(textPaint == nullptr)
                {
                    m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_text_fill_missing", "Text node has no visible decoded fill paint; command skipped", _node.id.c_str());
                    return;
                }

                if(textPaint->type != ECanvasPaintType::Solid)
                {
                    m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_text_paint_unsupported", "Only solid text fill paints are implemented; text command skipped", _node.id.c_str());
                    return;
                }

                if(textPaint->blendMode == ECanvasBlendMode::Unsupported)
                {
                    m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_text_blend_mode_unsupported", "Text fill blendMode is not implemented; text command skipped", _node.id.c_str());
                    return;
                }

                RenderCommand & text = m_renderList.addCommand(ERenderCommandType::Text);
                assignRenderLayer(&text);
                text.id = "figma.text";
                text.nodeId = _node.id;
                text.text = _node.text;
                text.rect = rect;
                text.color = textPaint->color;
                text.fontSize = _node.fontSize;
                text.lineHeight = _node.lineHeight;
                text.fontWeight = Detail::fontWeightFromStyle(_node);
                text.fontFamily = _node.fontFamily;
                text.fontStyle = _node.fontStyle;
                text.fontPostscriptName = _node.fontPostscriptName;
                text.textAlignHorizontal = Detail::renderHorizontalAlign(_node.textAlignHorizontal);
                text.textAlignVertical = Detail::renderVerticalAlign(_node.textAlignVertical);
                text.opacity = opacity * std::max(0.0f, std::min(1.0f, textPaint->opacity));
                Detail::assignPaintMetadata(*textPaint, &text);
                Detail::applyNodeBlendMode(_node, &text);

                text.textLines.reserve(_node.textLines.size());
                for(const CanvasTextLineDesc & canvasLine : _node.textLines)
                {
                    RenderTextLineDesc line(m_memory);
                    line.text = canvasLine.text;
                    line.x = canvasLine.x;
                    line.y = canvasLine.y;
                    line.width = canvasLine.width;
                    line.lineHeight = canvasLine.lineHeight;
                    line.lineAscent = canvasLine.lineAscent;
                    text.textLines.emplace_back(std::move(line));
                }

                FigmaString boundText(m_memory);
                bool textCommandRemoved = false;
                if(this->resolveTextBinding(_node, &boundText) == true)
                {
                    if(text.textLines.size() == 1)
                    {
                        text.text = boundText;
                        text.textLines.front().text = boundText;
                    }
                    else
                    {
                        m_renderList.removeLastCommand();
                        textCommandRemoved = true;
                        m_diagnostics.add(EDiagnosticSeverity::Warning, "ux_binding_text_layout_unsupported", "Text binding requires reshaping/reflow for this decoded text node; command skipped", _node.id.c_str());
                    }
                }

                if(textCommandRemoved == false)
                {
                    Detail::addTextQuad(&text);
                }
            }
        }

        for(auto it = _node.children.rbegin(); it != _node.children.rend(); ++it)
        {
            this->appendCanvasNode(*it, opacity, _offsetX, _offsetY, _animation, renderLayerEnabled);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::appendPrototypeHotspots(const CanvasNodeDesc & _node, const CanvasNodeDesc & _frame)
    {
        if(_node.visible == false || this->isNodeVisibleByBinding(_node) == false || this->isNodeEnabledByBinding(_node) == false)
        {
            return;
        }

        for(const PrototypeInteractionDesc & interaction : _node.prototypeInteractions)
        {
            if(interaction.eventType != EPrototypeEventType::Click && interaction.eventType != EPrototypeEventType::Hover)
            {
                continue;
            }

            const PrototypeActionDesc * action = Detail::findPrototypeTargetAction(interaction);
            if(action == nullptr)
            {
                if(Detail::hasOnlyTargetlessInternalPrototypeActions(interaction) == true)
                {
                    continue;
                }

                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_action_unsupported", "ON_CLICK interaction has no supported internal target action", _node.id.c_str());
                continue;
            }

            const CanvasNodeDesc * target = m_document.findCanvasNodeDesc(action->targetNodeId);
            if(target == nullptr)
            {
                m_diagnostics.add(EDiagnosticSeverity::Warning, "fig_prototype_target_missing", "Prototype action target node was not found in decoded document", _node.id.c_str());
                continue;
            }

            Hotspot hotspot(m_memory);
            hotspot.rect = {
                _node.rect.x - _frame.rect.x,
                _node.rect.y - _frame.rect.y,
                _node.rect.w,
                _node.rect.h
            };
            hotspot.nodeId = _node.id;
            hotspot.actionId = "figma.prototype.navigate";
            hotspot.targetFrameId = target->id;
            hotspot.prototypeAction = action;
            hotspot.eventType = interaction.eventType;
            m_hotspots.emplace_back(std::move(hotspot));
        }

        for(const CanvasNodeDesc & child : _node.children)
        {
            this->appendPrototypeHotspots(child, _frame);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::rebuildHotspots()
    {
        m_hotspots.clear();

        const CanvasNodeDesc * prototypeFrame = nullptr;
        if(m_currentFrameId.empty() == false)
        {
            prototypeFrame = m_document.findCanvasNodeDesc(m_currentFrameId);
        }

        if(prototypeFrame == nullptr)
        {
            prototypeFrame = m_document.getPrototypeStartFrameDesc();
        }

        if(prototypeFrame == nullptr)
        {
            return;
        }

        this->appendPrototypeHotspots(*prototypeFrame, *prototypeFrame);

        const ActionVector & actions = m_document.getActions();
        for(const ActionDesc & action : actions)
        {
            const CanvasNodeDesc * node = m_document.findCanvasNodeDesc(action.nodeId);
            if(node == nullptr)
            {
                m_diagnostics.add(EDiagnosticSeverity::Warning, "ux_action_node_missing", "Action target node was not found in decoded document", action.nodeId.c_str());
                continue;
            }

            if(node->visible == false || this->isNodeVisibleByBinding(*node) == false || this->isNodeEnabledByBinding(*node) == false)
            {
                continue;
            }

            Hotspot hotspot(m_memory);
            hotspot.rect = {
                node->rect.x - prototypeFrame->rect.x,
                node->rect.y - prototypeFrame->rect.y,
                node->rect.w,
                node->rect.h
            };
            hotspot.nodeId = action.nodeId;
            hotspot.actionId = action.actionId;
            hotspot.targetFrameId = action.targetFrameId;
            m_hotspots.emplace_back(std::move(hotspot));
        }
    }
    //////////////////////////////////////////////////////////////////////////
    const Player::NodeSwapState * Player::findNodeSwapState(FigmaStringView _nodeId) const
    {
        FigmaString key(_nodeId.begin(), _nodeId.end(), m_memory);
        const auto it = m_nodeSwaps.find(key);
        if(it == m_nodeSwaps.end())
        {
            return nullptr;
        }

        return &it->second;
    }
    //////////////////////////////////////////////////////////////////////////
    const Player::LocalAnimationState * Player::findLocalAnimation(FigmaStringView _nodeId) const
    {
        for(const LocalAnimationState & animation : m_localAnimations)
        {
            if(animation.active == true && animation.sourceNodeId == _nodeId)
            {
                return &animation;
            }
        }

        return nullptr;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Player::isNodeVisibleByBinding(const CanvasNodeDesc & _node)
    {
        for(const BindingDesc & itemDesc : m_document.getBindings())
        {
            if(itemDesc.nodeId != _node.id || itemDesc.property != EBindingProperty::Visible)
            {
                continue;
            }

            const BindingValue value = this->resolveBindingValue(itemDesc);
            if(value.type == EBindingValueType::None)
            {
                return true;
            }

            return Detail::valueAsVisible(value);
        }

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Player::isNodeEnabledByBinding(const CanvasNodeDesc & _node)
    {
        for(const BindingDesc & itemDesc : m_document.getBindings())
        {
            if(itemDesc.nodeId != _node.id || itemDesc.property != EBindingProperty::Enabled)
            {
                continue;
            }

            const BindingValue value = this->resolveBindingValue(itemDesc);
            if(value.type == EBindingValueType::None)
            {
                return true;
            }

            return Detail::valueAsVisible(value);
        }

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Player::resolveTextBinding(const CanvasNodeDesc & _node, FigmaString * const _text)
    {
        for(const BindingDesc & itemDesc : m_document.getBindings())
        {
            if(itemDesc.nodeId != _node.id || itemDesc.property != EBindingProperty::Text)
            {
                continue;
            }

            if(_node.type != ECanvasNodeType::Text)
            {
                m_diagnostics.add(EDiagnosticSeverity::Warning, "ux_binding_text_target_invalid", "Text binding target is not a decoded text node; command skipped", _node.id.c_str());
                return false;
            }

            const BindingValue value = this->resolveBindingValue(itemDesc);
            if(value.type == EBindingValueType::None)
            {
                return false;
            }

            *_text = Detail::valueAsText(m_memory, value);
            return true;
        }

        return false;
    }
    //////////////////////////////////////////////////////////////////////////
    bool Player::resolveImageBinding(const CanvasNodeDesc & _node, FigmaString * const _assetId)
    {
        for(const BindingDesc & itemDesc : m_document.getBindings())
        {
            if(itemDesc.nodeId != _node.id || itemDesc.property != EBindingProperty::Image)
            {
                continue;
            }

            const BindingValue value = this->resolveBindingValue(itemDesc);
            if(value.type == EBindingValueType::None)
            {
                return false;
            }

            if(value.type != EBindingValueType::Image && value.type != EBindingValueType::Text)
            {
                m_diagnostics.add(EDiagnosticSeverity::Warning, "ux_binding_image_value_invalid", "Image binding did not resolve to an asset id string; decoded image command kept unchanged", _node.id.c_str());
                return false;
            }

            *_assetId = value.stringValue;
            return _assetId->empty() == false;
        }

        return false;
    }
    //////////////////////////////////////////////////////////////////////////
    BindingValue Player::resolveBindingValue(const BindingDesc & _item)
    {
        auto it = m_overrides.find(_item.key);
        if(it != m_overrides.end())
        {
            return Detail::copyValue(m_memory, it->second);
        }

        BindingValue value(m_memory);
        if(m_dataContext != nullptr && m_dataContext->getBindingValue(_item.key, &value) == true)
        {
            return value;
        }

        return value;
    }
    //////////////////////////////////////////////////////////////////////////
    const CanvasNodeDesc * Player::resolveInitialFrame() const
    {
        if(m_desc.startFrameId != nullptr && m_desc.startFrameId[0] != '\0')
        {
            return m_document.findCanvasNodeDesc(m_desc.startFrameId);
        }

        return m_document.getPrototypeStartFrameDesc();
    }
    //////////////////////////////////////////////////////////////////////////
    EResult createPlayerImpl(DocumentInterface * const _document, const PlayerDesc & _desc, PlayerInterface ** const _player)
    {
        if(_document == nullptr || _player == nullptr)
        {
            return EResult::InvalidArgument;
        }

        *_player = nullptr;

        Document * const document = static_cast<Document *>(_document);

        if(_desc.startFrameId != nullptr && _desc.startFrameId[0] != '\0' && document->findCanvasNodeDesc(_desc.startFrameId) == nullptr)
        {
            return EResult::NotFound;
        }

        try
        {
            *_player = new Player(*document, _desc, document->getMemory());
        }
        catch(const std::bad_alloc &)
        {
            return EResult::OutOfMemory;
        }

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
}

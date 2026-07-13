#include "Player.h"

#include "Figma/ActionRouter.h"

#include "DiagnosticsMacros.h"
#include "Memory.h"
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
#include <stdexcept>
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
        static float cross(const Vec2f & _a, const Vec2f & _b, float _x, float _y)
        {
            return (_b.x - _a.x) * (_y - _a.y) - (_b.y - _a.y) * (_x - _a.x);
        }
        //////////////////////////////////////////////////////////////////////////
        static bool containsQuad(const Vec2f (&_quad)[4], float _x, float _y)
        {
            bool positive = false;
            bool negative = false;

            for(std::size_t index = 0; index != 4; ++index)
            {
                const Vec2f & a = _quad[index];
                const Vec2f & b = _quad[(index + 1) % 4];
                const float value = cross(a, b, _x, _y);

                if(value > 0.0001f)
                {
                    positive = true;
                }

                if(value < -0.0001f)
                {
                    negative = true;
                }
            }

            if(positive == true && negative == true)
            {
                return false;
            }

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        static bool containsHotspot(const Rectf & _rect, const Vec2f (&_quad)[4], bool _hasClip, const Rectf & _clip, float _x, float _y)
        {
            if(_hasClip == true && contains(_clip, _x, _y) == false)
            {
                return false;
            }

            if(contains(_rect, _x, _y) == false)
            {
                return false;
            }

            if(containsQuad(_quad, _x, _y) == false)
            {
                return false;
            }

            return true;
        }
        //////////////////////////////////////////////////////////////////////////
        static ETriggerType makeTriggerType(EPrototypeEventType _type)
        {
            switch(_type)
            {
            case EPrototypeEventType::Click:
                return ETriggerType::Click;
            case EPrototypeEventType::HoverEnter:
                return ETriggerType::HoverEnter;
            case EPrototypeEventType::HoverLeave:
                return ETriggerType::HoverLeave;
            case EPrototypeEventType::Press:
                return ETriggerType::Press;
            case EPrototypeEventType::PointerDown:
                return ETriggerType::PointerDown;
            case EPrototypeEventType::PointerUp:
                return ETriggerType::PointerUp;
            case EPrototypeEventType::AfterTimeout:
                return ETriggerType::AfterTimeout;
            case EPrototypeEventType::KeyDown:
                return ETriggerType::KeyDown;
            case EPrototypeEventType::Unsupported:
                break;
            }

            return ETriggerType::Unsupported;
        }
        //////////////////////////////////////////////////////////////////////////
        static EConnectionType makeConnectionType(EPrototypeConnectionType _type)
        {
            switch(_type)
            {
            case EPrototypeConnectionType::None:
                return EConnectionType::None;
            case EPrototypeConnectionType::InternalNode:
                return EConnectionType::InternalNode;
            case EPrototypeConnectionType::Back:
                return EConnectionType::Back;
            case EPrototypeConnectionType::Close:
                return EConnectionType::Close;
            case EPrototypeConnectionType::Unsupported:
                break;
            }

            return EConnectionType::Unsupported;
        }
        //////////////////////////////////////////////////////////////////////////
        static ENavigationType makeNavigationType(EPrototypeNavigationType _type)
        {
            switch(_type)
            {
            case EPrototypeNavigationType::Navigate:
                return ENavigationType::Navigate;
            case EPrototypeNavigationType::Overlay:
                return ENavigationType::Overlay;
            case EPrototypeNavigationType::Swap:
                return ENavigationType::Swap;
            case EPrototypeNavigationType::ScrollTo:
                return ENavigationType::ScrollTo;
            case EPrototypeNavigationType::Unsupported:
                break;
            }

            return ENavigationType::Unsupported;
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
        static void * graphicsAlloc(gp_size_t _size, void * _userData)
        {
            GraphicsMemoryContext * context = static_cast<GraphicsMemoryContext *>(_userData);
            if(context == nullptr || context->memory == nullptr)
            {
                return nullptr;
            }

            return allocateMemoryBlock( context->memory, _size );
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

            deallocateMemoryBlock( context->memory, _ptr );
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

            const std::size_t oldSize = getMemoryBlockSize( _ptr );
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
                        FIGMA_DIAGNOSTICS_ADD_POINTER(_diagnostics, EDiagnosticSeverity::Warning, desc.code, desc.message, _node.id.c_str());
                    }
                }
            }

            if(_paint.hasPaintFilterValue == true)
            {
                for(const FilterDiagnosticDesc & desc : UnsupportedPaintFilter)
                {
                    if(isFilterValueActive(_paint.paintFilter[desc.index]) == true)
                    {
                        FIGMA_DIAGNOSTICS_ADD_POINTER(_diagnostics, EDiagnosticSeverity::Warning, desc.code, desc.message, _node.id.c_str());
                    }
                }
            }
        }
        //////////////////////////////////////////////////////////////////////////
        static void assignArcData(const CanvasArcDataDesc & _arcData, RenderCommand * const _command)
        {
            if(_arcData.valid == false)
            {
                return;
            }

            _command->hasArcDataValue = true;
            _command->arcStartingAngle = _arcData.startingAngle;
            _command->arcEndingAngle = _arcData.endingAngle;
            _command->arcInnerRadius = _arcData.innerRadius;
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
        static void makeRenderQuad(const Rectf & _nodeRect, const Vec2f * const _nodeQuad, const Rectf & _renderRect, Vec2f * const _renderQuad);
        //////////////////////////////////////////////////////////////////////////
        static void addImageQuad(RenderCommand * const _command, const AssetDesc * _asset, const Rectf & _nodeRect, const Vec2f * const _nodeQuad)
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

            const Color white{1.0f, 1.0f, 1.0f, 1.0f};
            Vec2f quad[4];
            makeRenderQuad(_nodeRect, _nodeQuad, _command->rect, quad);
            _command->vertices.resize(4);
            _command->vertices[0] = {quad[0].x, quad[0].y, u0, v0, white};
            _command->vertices[1] = {quad[1].x, quad[1].y, u1, v0, white};
            _command->vertices[2] = {quad[2].x, quad[2].y, u1, v1, white};
            _command->vertices[3] = {quad[3].x, quad[3].y, u0, v1, white};

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
        static void makeRenderQuad(const Rectf & _nodeRect, const Vec2f * const _nodeQuad, const Rectf & _renderRect, Vec2f * const _renderQuad)
        {
            if(_nodeQuad == nullptr || _renderQuad == nullptr)
            {
                return;
            }

            if(std::fabs(_nodeRect.w) <= 0.0001f || std::fabs(_nodeRect.h) <= 0.0001f)
            {
                _renderQuad[0] = {_renderRect.x, _renderRect.y};
                _renderQuad[1] = {_renderRect.x + _renderRect.w, _renderRect.y};
                _renderQuad[2] = {_renderRect.x + _renderRect.w, _renderRect.y + _renderRect.h};
                _renderQuad[3] = {_renderRect.x, _renderRect.y + _renderRect.h};
                return;
            }

            for(std::size_t index = 0; index != 4; ++index)
            {
                const float normalizedX = (_nodeQuad[index].x - _nodeRect.x) / _nodeRect.w;
                const float normalizedY = (_nodeQuad[index].y - _nodeRect.y) / _nodeRect.h;
                _renderQuad[index].x = _renderRect.x + normalizedX * _renderRect.w;
                _renderQuad[index].y = _renderRect.y + normalizedY * _renderRect.h;
            }
        }
        //////////////////////////////////////////////////////////////////////////
        static void applyNodeQuad(RenderCommand * const _command, const Rectf & _nodeRect, const Vec2f * const _nodeQuad)
        {
            if(_command == nullptr || _command->vertices.empty() == true || std::fabs(_command->rect.w) <= 0.0001f || std::fabs(_command->rect.h) <= 0.0001f)
            {
                return;
            }

            Vec2f quad[4];
            makeRenderQuad(_nodeRect, _nodeQuad, _command->rect, quad);

            for(RenderVertex & vertex : _command->vertices)
            {
                const float normalizedX = (vertex.x - _command->rect.x) / _command->rect.w;
                const float normalizedY = (vertex.y - _command->rect.y) / _command->rect.h;
                const float topX = Detail::lerp(quad[0].x, quad[1].x, normalizedX);
                const float topY = Detail::lerp(quad[0].y, quad[1].y, normalizedX);
                const float bottomX = Detail::lerp(quad[3].x, quad[2].x, normalizedX);
                const float bottomY = Detail::lerp(quad[3].y, quad[2].y, normalizedX);
                vertex.x = Detail::lerp(topX, bottomX, normalizedY);
                vertex.y = Detail::lerp(topY, bottomY, normalizedY);
            }
        }
        //////////////////////////////////////////////////////////////////////////
        static void applyNodePathQuad(RenderCommand * const _command, const CanvasNodeDesc & _node, const Rectf & _nodeRect, const Vec2f * const _nodeQuad)
        {
            if(_command == nullptr || _command->vertices.empty() == true)
            {
                return;
            }

            const float localWidth = _node.size.x;
            const float localHeight = _node.size.y;
            if(localWidth <= 0.0001f || localHeight <= 0.0001f)
            {
                return;
            }

            Vec2f quad[4];
            makeRenderQuad(_nodeRect, _nodeQuad, _command->rect, quad);

            for(RenderVertex & vertex : _command->vertices)
            {
                const float normalizedX = (vertex.x - _command->rect.x) / localWidth;
                const float normalizedY = (vertex.y - _command->rect.y) / localHeight;
                const float topX = Detail::lerp(quad[0].x, quad[1].x, normalizedX);
                const float topY = Detail::lerp(quad[0].y, quad[1].y, normalizedX);
                const float bottomX = Detail::lerp(quad[3].x, quad[2].x, normalizedX);
                const float bottomY = Detail::lerp(quad[3].y, quad[2].y, normalizedX);
                vertex.x = Detail::lerp(topX, bottomX, normalizedY);
                vertex.y = Detail::lerp(topY, bottomY, normalizedY);
            }
        }
        //////////////////////////////////////////////////////////////////////////
        static void addTextQuad(RenderCommand * const _command, const Rectf & _nodeRect, const Vec2f * const _nodeQuad)
        {
            Vec2f quad[4];
            makeRenderQuad(_nodeRect, _nodeQuad, _command->rect, quad);
            const Color white{1.0f, 1.0f, 1.0f, 1.0f};

            _command->vertices.resize(4);
            _command->vertices[0] = {quad[0].x, quad[0].y, 0.0f, 0.0f, white};
            _command->vertices[1] = {quad[1].x, quad[1].y, 1.0f, 0.0f, white};
            _command->vertices[2] = {quad[2].x, quad[2].y, 1.0f, 1.0f, white};
            _command->vertices[3] = {quad[3].x, quad[3].y, 0.0f, 1.0f, white};
            _command->indices = {0, 1, 2, 0, 2, 3};
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
                FIGMA_DIAGNOSTICS_ADD_POINTER(_diagnostics, EDiagnosticSeverity::Warning, _code, message.c_str(), _node.id.c_str());
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
        , tracks(_memory)
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
        , m_firedTimerInteractionIds(_memory)
        , m_nodeSwaps(_memory)
        , m_localAnimations(_memory)
        , m_overrides(_memory)
        , m_pointerCaptures(_memory)
        , m_navigationHistory(_memory)
        , m_overlayFrames(_memory)
        , m_overlayStartTimes(_memory)
        , m_currentFrameId(_memory)
        , m_animationState(_memory)
    {
        const CanvasNodeDesc * initialFrame = this->resolveInitialFrame();
        this->setCurrentFrame(initialFrame);

        const EResult result = this->update(0.0f);
        if(result == EResult::OutOfMemory)
        {
            throw std::bad_alloc();
        }

        if(result != EResult::Ok)
        {
            throw std::runtime_error("Unable to initialize Figma player");
        }
    }
    //////////////////////////////////////////////////////////////////////////
    Player::~Player()
    {
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::destroy()
    {
        FigmaMemoryResource * memory = m_memory;

        this->~Player();
        memory->deallocate(this, sizeof(Player), alignof(Player));
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
    EResult Player::setViewport(const ViewportDesc & _viewport)
    {
        m_desc.viewport = _viewport;

        m_hotspotsDirty = true;
        this->rebuildHotspots();
        this->rebuildRenderList();

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::hitTest(float _x, float _y, bool * const _hit) const
    {
        if(_hit == nullptr)
        {
            return EResult::InvalidArgument;
        }

        *_hit = false;

        if(m_pointerCaptures.empty() == false)
        {
            *_hit = true;
            return EResult::Ok;
        }

        for(auto it = m_hotspots.rbegin(); it != m_hotspots.rend(); ++it)
        {
            const Hotspot & hotspot = *it;
            if(Detail::containsHotspot(hotspot.rect, hotspot.quad, hotspot.hasClip, hotspot.clip, _x, _y) == false)
            {
                continue;
            }

            *_hit = true;
            break;
        }

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::inputPointer(const PointerEvent & _event, InputDispatchResult * const _dispatch)
    {
        InputDispatchResult dispatch;
        this->hitTest(_event.x, _event.y, &dispatch.hit);

        auto captureIt = m_pointerCaptures.find(_event.pointerId);
        dispatch.captured = captureIt != m_pointerCaptures.end();

        if(m_animationState.active == true)
        {
            if(_dispatch != nullptr)
            {
                *_dispatch = dispatch;
            }

            return EResult::Ok;
        }

        EResult result = EResult::Ok;

        if(_event.type == EPointerEventType::Move)
        {
            const Hotspot * hotspot = this->findHotspot(_event.x, _event.y, EPrototypeEventType::HoverEnter);
            NodeIdSet hoveredNow(m_memory);

            if(hotspot != nullptr)
            {
                hoveredNow.emplace(hotspot->nodeId);
                if(m_hoveredNodeIds.find(hotspot->nodeId) == m_hoveredNodeIds.end())
                {
                    result = this->routeHotspot(*hotspot, EActionInputKind::Pointer, &_event, nullptr);
                    dispatch.handled = result == EResult::Ok;
                }
            }

            for(const FigmaString & nodeId : m_hoveredNodeIds)
            {
                if(hoveredNow.find(nodeId) != hoveredNow.end())
                {
                    continue;
                }

                for(auto it = m_hotspots.rbegin(); it != m_hotspots.rend(); ++it)
                {
                    if(it->nodeId == nodeId && it->eventType == EPrototypeEventType::HoverLeave)
                    {
                        result = this->routeHotspot(*it, EActionInputKind::Pointer, &_event, nullptr);
                        dispatch.handled = result == EResult::Ok;
                        break;
                    }
                }
            }

            m_hoveredNodeIds = std::move(hoveredNow);
        }
        else if(_event.type == EPointerEventType::Down)
        {
            const Hotspot * downHotspot = this->findHotspot(_event.x, _event.y, EPrototypeEventType::PointerDown);
            if(downHotspot == nullptr)
            {
                downHotspot = this->findHotspot(_event.x, _event.y, EPrototypeEventType::Press);
            }

            const Hotspot * clickHotspot = this->findHotspot(_event.x, _event.y, EPrototypeEventType::Click);
            const Hotspot * captureHotspot = clickHotspot != nullptr ? clickHotspot : downHotspot;
            if(captureHotspot != nullptr)
            {
                PointerCapture capture(m_memory);
                capture.nodeId = captureHotspot->nodeId;
                capture.button = _event.button;
                if(captureHotspot->interaction != nullptr)
                {
                    capture.interactionId = captureHotspot->interaction->id;
                }
                else
                {
                    capture.interactionId = captureHotspot->actionId;
                }

                m_pointerCaptures.erase(_event.pointerId);
                m_pointerCaptures.emplace(_event.pointerId, std::move(capture));
                dispatch.captured = true;
                dispatch.handled = true;
            }

            if(downHotspot != nullptr)
            {
                result = this->routeHotspot(*downHotspot, EActionInputKind::Pointer, &_event, nullptr);
                dispatch.handled = result == EResult::Ok;
            }
        }
        else if(_event.type == EPointerEventType::Up)
        {
            const Hotspot * upHotspot = this->findHotspot(_event.x, _event.y, EPrototypeEventType::PointerUp);
            if(upHotspot != nullptr)
            {
                result = this->routeHotspot(*upHotspot, EActionInputKind::Pointer, &_event, nullptr);
                dispatch.handled = result == EResult::Ok;
            }

            if(captureIt != m_pointerCaptures.end())
            {
                const PointerCapture capture = captureIt->second;
                m_pointerCaptures.erase(captureIt);
                dispatch.handled = true;
                dispatch.captured = false;

                if(capture.button == _event.button)
                {
                    for(auto it = m_hotspots.rbegin(); it != m_hotspots.rend(); ++it)
                    {
                        const Hotspot & clickHotspot = *it;
                        if(clickHotspot.eventType != EPrototypeEventType::Click || clickHotspot.nodeId != capture.nodeId)
                        {
                            continue;
                        }

                        if(Detail::containsHotspot(clickHotspot.rect, clickHotspot.quad, clickHotspot.hasClip, clickHotspot.clip, _event.x, _event.y) == false)
                        {
                            continue;
                        }

                        result = this->routeHotspot(clickHotspot, EActionInputKind::Pointer, &_event, nullptr);
                        dispatch.handled = result == EResult::Ok;
                        if(result != EResult::Ok)
                        {
                            break;
                        }
                    }
                }
            }
        }
        else if(_event.type == EPointerEventType::Cancel)
        {
            if(captureIt != m_pointerCaptures.end())
            {
                m_pointerCaptures.erase(captureIt);
                dispatch.handled = true;
                dispatch.captured = false;
            }
        }

        if(_dispatch != nullptr)
        {
            *_dispatch = dispatch;
        }

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::inputKey(const KeyEvent & _event, InputDispatchResult * const _dispatch)
    {
        InputDispatchResult dispatch;

        if(m_animationState.active == false && _event.type == EKeyEventType::Down)
        {
            for(auto it = m_hotspots.rbegin(); it != m_hotspots.rend(); ++it)
            {
                const Hotspot & hotspot = *it;
                if(hotspot.eventType != EPrototypeEventType::KeyDown)
                {
                    continue;
                }

                if(hotspot.keyCode != 0 && hotspot.keyCode != _event.keyCode)
                {
                    continue;
                }

                const EResult result = this->routeHotspot(hotspot, EActionInputKind::Key, nullptr, &_event);
                dispatch.handled = result == EResult::Ok;
                if(_dispatch != nullptr)
                {
                    *_dispatch = dispatch;
                }

                return result;
            }
        }

        if(_dispatch != nullptr)
        {
            *_dispatch = dispatch;
        }

        return EResult::Ok;
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
            FIGMA_DIAGNOSTICS_ADD(m_diagnostics, diagnostic.severity, diagnostic.code.c_str(), diagnostic.message.c_str(), diagnostic.nodeId.c_str());
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
        if(m_hotspotsDirty == true)
        {
            this->rebuildHotspots();
        }
        this->rebuildRenderList();

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::restart()
    {
        const CanvasNodeDesc * initialFrame = this->resolveInitialFrame();
        this->setCurrentFrame(initialFrame);

        m_animationState = PlayerAnimationStateDesc(m_memory);
        m_hoveredNodeIds.clear();
        m_firedTimerInteractionIds.clear();
        m_pointerCaptures.clear();
        m_nodeSwaps.clear();
        m_localAnimations.clear();
        m_navigationHistory.clear();
        m_overlayFrames.clear();
        m_overlayStartTimes.clear();
        m_hotspotsDirty = true;
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
        m_hotspotsDirty = true;

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
        m_hotspotsDirty = true;

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
    const Player::Hotspot * Player::findHotspot(float _x, float _y, EPrototypeEventType _eventType) const
    {
        for(auto it = m_hotspots.rbegin(); it != m_hotspots.rend(); ++it)
        {
            const Hotspot & hotspot = *it;
            if(hotspot.eventType != _eventType)
            {
                continue;
            }

            if(Detail::containsHotspot(hotspot.rect, hotspot.quad, hotspot.hasClip, hotspot.clip, _x, _y) == false)
            {
                continue;
            }

            return &hotspot;
        }

        return nullptr;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::routeHotspot(const Hotspot & _hotspot, EActionInputKind _inputKind, const PointerEvent * _pointer, const KeyEvent * _key, float _initialElapsed)
    {
        if(m_actionRouter != nullptr)
        {
            TriggerEvent trigger;
            trigger.inputKind = _inputKind;
            trigger.triggerType = Detail::makeTriggerType(_hotspot.eventType);
            if(_hotspot.interaction != nullptr)
            {
                trigger.interactionId = _hotspot.interaction->id;
            }
            trigger.sourceNodeId = _hotspot.nodeId;
            trigger.currentFrameId = m_currentFrameId;
            if(_pointer != nullptr)
            {
                trigger.pointer = *_pointer;
            }
            if(_key != nullptr)
            {
                trigger.key = *_key;
            }
            trigger.ud = m_desc.ud;

            const EResult triggerResult = m_actionRouter->routeTrigger(trigger);
            if(triggerResult != EResult::Ok)
            {
                return triggerResult;
            }
        }

        if(_hotspot.uxAction == true)
        {
            ActionResponse response;
            if(m_actionRouter != nullptr)
            {
                ActionEvent event;
                event.inputKind = _inputKind;
                event.triggerType = Detail::makeTriggerType(_hotspot.eventType);
                event.actionId = _hotspot.actionId;
                event.sourceNodeId = _hotspot.nodeId;
                event.currentFrameId = m_currentFrameId;
                event.targetFrameId = _hotspot.targetFrameId;
                if(_pointer != nullptr)
                {
                    event.pointer = *_pointer;
                }
                if(_key != nullptr)
                {
                    event.key = *_key;
                }
                event.ud = m_desc.ud;

                const EResult actionResult = m_actionRouter->routeAction(event, &response);
                if(actionResult != EResult::Ok)
                {
                    return actionResult;
                }
            }

            return this->executeActionResponse(response, nullptr, _hotspot.nodeId, _hotspot.targetFrameId, _initialElapsed);
        }

        if(_hotspot.interaction == nullptr)
        {
            return EResult::Ok;
        }

        for(const PrototypeActionDesc & action : _hotspot.interaction->actions)
        {
            const EResult result = this->routePrototypeAction(_hotspot, action, _inputKind, _pointer, _key, _initialElapsed);
            if(result != EResult::Ok)
            {
                return result;
            }
        }

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::routePrototypeAction(const Hotspot & _hotspot, const PrototypeActionDesc & _action, EActionInputKind _inputKind, const PointerEvent * _pointer, const KeyEvent * _key, float _initialElapsed)
    {
        ActionResponse response;
        if(m_actionRouter != nullptr)
        {
            const Char * actionId = "figma.prototype.navigate";
            if(_action.connectionType == EPrototypeConnectionType::Back)
            {
                actionId = "figma.prototype.back";
            }
            else if(_action.connectionType == EPrototypeConnectionType::Close)
            {
                actionId = "figma.prototype.close";
            }
            else if(_action.navigationType == EPrototypeNavigationType::Overlay)
            {
                actionId = "figma.prototype.overlay";
            }
            else if(_action.navigationType == EPrototypeNavigationType::Swap)
            {
                actionId = "figma.prototype.swap";
            }

            ActionEvent event;
            event.inputKind = _inputKind;
            event.triggerType = Detail::makeTriggerType(_hotspot.eventType);
            event.connectionType = Detail::makeConnectionType(_action.connectionType);
            event.navigationType = Detail::makeNavigationType(_action.navigationType);
            event.actionId = actionId;
            if(_hotspot.interaction != nullptr)
            {
                event.interactionId = _hotspot.interaction->id;
            }
            event.sourceNodeId = _hotspot.nodeId;
            event.currentFrameId = m_currentFrameId;
            event.targetFrameId = _action.targetNodeId;
            if(_pointer != nullptr)
            {
                event.pointer = *_pointer;
            }
            if(_key != nullptr)
            {
                event.key = *_key;
            }
            event.ud = m_desc.ud;

            const EResult actionResult = m_actionRouter->routeAction(event, &response);
            if(actionResult != EResult::Ok)
            {
                return actionResult;
            }
        }

        return this->executeActionResponse(response, &_action, _hotspot.nodeId, _action.targetNodeId, _initialElapsed);
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::executeActionResponse(const ActionResponse & _response, const PrototypeActionDesc * _action, FigmaStringView _sourceNodeId, FigmaStringView _defaultTargetFrameId, float _initialElapsed)
    {
        if(_response.result == EActionResult::Consume)
        {
            return EResult::Ok;
        }

        if(_response.result == EActionResult::CloseOverlay)
        {
            return this->closeOverlay();
        }

        const FigmaStringView targetFrameId = _response.targetFrameId.empty() == false ? _response.targetFrameId : _defaultTargetFrameId;
        if(_response.result == EActionResult::OpenOverlay)
        {
            return this->openOverlay(targetFrameId);
        }

        if(_response.result == EActionResult::NavigateFrame)
        {
            return this->navigateToFrame(targetFrameId, nullptr, _sourceNodeId, _initialElapsed);
        }

        if(_action != nullptr)
        {
            return this->executePrototypeAction(*_action, _sourceNodeId, _initialElapsed);
        }

        if(targetFrameId.empty() == false)
        {
            return this->navigateToFrame(targetFrameId, nullptr, _sourceNodeId, _initialElapsed);
        }

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::executePrototypeAction(const PrototypeActionDesc & _action, FigmaStringView _sourceNodeId, float _initialElapsed)
    {
        if(_action.connectionType == EPrototypeConnectionType::Back)
        {
            return this->goBack();
        }

        if(_action.connectionType == EPrototypeConnectionType::Close)
        {
            return this->closeOverlay();
        }

        if(_action.navigationType == EPrototypeNavigationType::Overlay)
        {
            return this->openOverlay(_action.targetNodeId);
        }

        if(_action.navigationType == EPrototypeNavigationType::Swap)
        {
            return this->swapNodeState(_sourceNodeId, _sourceNodeId, _action, _initialElapsed);
        }

        if(_action.navigationType == EPrototypeNavigationType::ScrollTo || _action.navigationType == EPrototypeNavigationType::Unsupported)
        {
            return EResult::InvalidState;
        }

        return this->navigateToFrame(_action.targetNodeId, &_action, _sourceNodeId, _initialElapsed);
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::navigateToFrame(FigmaStringView _targetFrameId)
    {
        return this->navigateToFrame(_targetFrameId, nullptr, FigmaStringView(), 0.0f);
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::openOverlay(FigmaStringView _targetFrameId)
    {
        if(_targetFrameId.empty() == true)
        {
            return EResult::InvalidArgument;
        }

        const CanvasNodeDesc * target = m_document.findCanvasNodeDesc(_targetFrameId);
        if(target == nullptr)
        {
            return EResult::NotFound;
        }

        m_overlayFrames.emplace_back(target);
        m_overlayStartTimes.emplace_back(m_time);
        m_pointerCaptures.clear();
        m_hoveredNodeIds.clear();
        m_hotspotsDirty = true;
        this->rebuildHotspots();
        this->rebuildRenderList();

        if(m_actionRouter != nullptr)
        {
            m_actionRouter->onOverlayOpened(target->id);
        }

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::closeOverlay()
    {
        if(m_overlayFrames.empty() == true)
        {
            return EResult::NotFound;
        }

        const CanvasNodeDesc * overlay = m_overlayFrames.back();
        m_overlayFrames.pop_back();
        m_overlayStartTimes.pop_back();
        m_pointerCaptures.clear();
        m_hoveredNodeIds.clear();
        m_hotspotsDirty = true;
        this->rebuildHotspots();
        this->rebuildRenderList();

        if(m_actionRouter != nullptr)
        {
            m_actionRouter->onOverlayClosed(overlay->id);
        }

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::goBack()
    {
        if(m_overlayFrames.empty() == false)
        {
            return this->closeOverlay();
        }

        if(m_navigationHistory.empty() == true)
        {
            return EResult::NotFound;
        }

        const CanvasNodeDesc * frame = m_navigationHistory.back();
        m_navigationHistory.pop_back();
        this->setCurrentFrame(frame);
        m_nodeSwaps.clear();
        m_localAnimations.clear();
        m_pointerCaptures.clear();
        m_hoveredNodeIds.clear();
        m_time = 0.0f;

        return this->update(0.0f);
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
            FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_prototype_target_missing", "Prototype action target node was not found in decoded document", FigmaString(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory).c_str());
            return EResult::NotFound;
        }

        const CanvasNodeDesc * source = m_currentFrame;

        if(source != nullptr && source != target)
        {
            m_navigationHistory.emplace_back(source);
        }

        while(m_overlayFrames.empty() == false)
        {
            const CanvasNodeDesc * overlay = m_overlayFrames.back();
            m_overlayFrames.pop_back();
            m_overlayStartTimes.pop_back();
            if(m_actionRouter != nullptr)
            {
                m_actionRouter->onOverlayClosed(overlay->id);
            }
        }

        if(source != nullptr && _action != nullptr && (_action->smartAnimate == true || _action->transitionType == EPrototypeTransitionType::SmartAnimate || _action->transitionDuration > 0.0f))
        {
            return this->beginPrototypeAnimation(*source, *target, *_action, _sourceNodeId, _initialElapsed);
        }

        this->setCurrentFrame(target);
        m_nodeSwaps.clear();
        m_localAnimations.clear();
        m_hoveredNodeIds.clear();
        m_pointerCaptures.clear();
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
            FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_prototype_target_missing", "SWAP_STATE target node was not found in decoded document", FigmaString(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory).c_str());
            return EResult::NotFound;
        }

        if(_action.transitionEasing == EAnimationEasing::Unsupported)
        {
            FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_animation_easing_unsupported", "SWAP_STATE easing is not implemented; state is changed without visual tweening", FigmaString(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory).c_str());
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
        FigmaString previousState(m_memory);
        if(inserted == false)
        {
            previousState = it->second.currentNodeId;
        }
        else
        {
            previousState = FigmaString(_fromNodeId.begin(), _fromNodeId.end(), m_memory);
        }
        it->second.currentNodeId = target->id;
        it->second.startedAt = m_time - std::max(0.0f, initialElapsed - duration);
        m_hotspotsDirty = true;

        if(m_actionRouter != nullptr)
        {
            m_actionRouter->onStateChanged(_sourceNodeId, previousState, target->id);
        }

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult Player::beginPrototypeAnimation(const CanvasNodeDesc & _sourceFrame, const CanvasNodeDesc & _targetFrame, const PrototypeActionDesc & _action, FigmaStringView _sourceNodeId, float _initialElapsed)
    {
        const float duration = std::max(0.0f, _action.transitionDuration);
        const float initialElapsed = std::max(0.0f, _initialElapsed);
        if(duration <= 0.0001f || initialElapsed >= duration)
        {
            this->setCurrentFrame(&_targetFrame);
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
        m_hotspotsDirty = true;

        if(smartAnimate == true)
        {
            this->collectSmartAnimateTracks(_sourceFrame, _targetFrame, &m_animationState.clip.tracks);
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
            FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_prototype_source_missing", "SWAP_STATE source node was not found in decoded document", FigmaString(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory).c_str());
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
        animation.smartAnimate = _action.smartAnimate == true || _action.transitionType == EPrototypeTransitionType::SmartAnimate;
        animation.duration = std::max(0.0f, _action.transitionDuration);
        animation.elapsed = std::max(0.0f, std::min(animation.duration, _initialElapsed));
        animation.progress = Detail::clamp01(animation.elapsed / std::max(0.0001f, animation.duration));
        animation.active = true;

        if(animation.smartAnimate == true)
        {
            this->collectSmartAnimateTracks(*fromNode, _targetNode, &animation.tracks);
        }

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

        const CanvasNodeDesc * targetFrame = m_document.findCanvasNodeDesc(m_animationState.clip.targetFrameId);
        this->setCurrentFrame(targetFrame);
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
        m_hotspotsDirty = true;

        if(m_actionRouter != nullptr)
        {
            m_actionRouter->onStateChanged(_animation->sourceNodeId, _animation->fromNodeId, _animation->targetNodeId);
        }
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
    void Player::collectSmartAnimateTracks(const CanvasNodeDesc & _sourceNode, const CanvasNodeDesc & _targetFrame, AnimationTrackVector * const _tracks)
    {
        if(_tracks == nullptr)
        {
            return;
        }

        Detail::CanvasNodeDescPtrVector usedTargets(m_memory);
        std::size_t sourceIndex = 0;
        for(const CanvasNodeDesc & child : _sourceNode.children)
        {
            const CanvasNodeDesc * targetChild = Detail::findSmartAnimateChildMatch(child, _targetFrame, sourceIndex, usedTargets);
            if(targetChild != nullptr)
            {
                usedTargets.emplace_back(targetChild);
                this->collectSmartAnimateTracksForPair(child, *targetChild, _sourceNode.rect, _targetFrame.rect, _tracks);
            }

            ++sourceIndex;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::collectSmartAnimateTracksForPair(const CanvasNodeDesc & _sourceNode, const CanvasNodeDesc & _targetNode, const Rectf & _sourceFrameRect, const Rectf & _targetFrameRect, AnimationTrackVector * const _tracks)
    {
        if(_tracks == nullptr)
        {
            return;
        }

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

            const float sourceOffsetX = _targetFrameRect.x - _sourceFrameRect.x;
            const float sourceOffsetY = _targetFrameRect.y - _sourceFrameRect.y;
            for(std::size_t index = 0; index != 4; ++index)
            {
                rectTrack.fromQuad[index].x = _sourceNode.quad[index].x + sourceOffsetX;
                rectTrack.fromQuad[index].y = _sourceNode.quad[index].y + sourceOffsetY;
                rectTrack.toQuad[index] = _targetNode.quad[index];
            }

            rectTrack.hasQuad = true;
            _tracks->emplace_back(std::move(rectTrack));

            AnimationTrackDesc opacityTrack(m_memory);
            opacityTrack.nodeId = _sourceNode.id;
            opacityTrack.targetNodeId = _targetNode.id;
            opacityTrack.type = EAnimationTrackType::Opacity;
            opacityTrack.from[0] = _sourceNode.opacity;
            opacityTrack.to[0] = _targetNode.opacity;
            _tracks->emplace_back(std::move(opacityTrack));

            if(_sourceNode.arcData.valid == true && _targetNode.arcData.valid == true)
            {
                AnimationTrackDesc arcTrack(m_memory);
                arcTrack.nodeId = _sourceNode.id;
                arcTrack.targetNodeId = _targetNode.id;
                arcTrack.type = EAnimationTrackType::Arc;
                arcTrack.from[0] = _sourceNode.arcData.startingAngle;
                arcTrack.from[1] = _sourceNode.arcData.endingAngle;
                arcTrack.from[2] = _sourceNode.arcData.innerRadius;
                arcTrack.to[0] = _targetNode.arcData.startingAngle;
                arcTrack.to[1] = _targetNode.arcData.endingAngle;
                arcTrack.to[2] = _targetNode.arcData.innerRadius;
                _tracks->emplace_back(std::move(arcTrack));
            }
        }

        Detail::CanvasNodeDescPtrVector usedTargets(m_memory);
        std::size_t sourceIndex = 0;
        for(const CanvasNodeDesc & sourceChild : _sourceNode.children)
        {
            const CanvasNodeDesc * targetChild = Detail::findSmartAnimateChildMatch(sourceChild, _targetNode, sourceIndex, usedTargets);
            if(targetChild != nullptr)
            {
                usedTargets.emplace_back(targetChild);
                this->collectSmartAnimateTracksForPair(sourceChild, *targetChild, _sourceFrameRect, _targetFrameRect, _tracks);
            }

            ++sourceIndex;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::updatePrototypeTimers(float _dt)
    {
        const CanvasNodeDesc * frame = m_overlayFrames.empty() == false ? m_overlayFrames.back() : m_currentFrame;
        const float frameStartedAt = m_overlayStartTimes.empty() == false ? m_overlayStartTimes.back() : 0.0f;

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

            if(m_time - frameStartedAt < std::max(0.0f, interaction.transitionTimeout))
            {
                continue;
            }

            const float triggerTime = frameStartedAt + std::max(0.0f, interaction.transitionTimeout);
            const float previousTime = std::max(0.0f, m_time - std::max(0.0f, _dt));
            const float initialElapsed = previousTime < triggerTime ? m_time - triggerTime : 0.0f;
            FigmaString timerId = frame->id + FigmaString( ":", m_memory ) + interaction.id;
            if(m_firedTimerInteractionIds.find(timerId) != m_firedTimerInteractionIds.end())
            {
                continue;
            }
            m_firedTimerInteractionIds.emplace(std::move(timerId));
            Hotspot hotspot(m_memory);
            hotspot.nodeId = frame->id;
            hotspot.interaction = &interaction;
            hotspot.eventType = EPrototypeEventType::AfterTimeout;
            this->routeHotspot(hotspot, EActionInputKind::Timer, nullptr, nullptr, initialElapsed);
            return;
        }

        for(const CanvasNodeDesc & child : frame->children)
        {
            if(this->updatePrototypeTimersForNode(child, *frame, child.id, frameStartedAt, _dt) == true)
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
                FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_prototype_target_missing", "SWAP_STATE current node was not found in decoded document", FigmaString(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory).c_str());
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

            const float triggerTime = startedAt + std::max(0.0f, interaction.transitionTimeout);
            const float previousTime = std::max(0.0f, m_time - std::max(0.0f, _dt));
            const float initialElapsed = previousTime < triggerTime ? m_time - triggerTime : 0.0f;
            FigmaString timerId(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory);
            timerId += ":";
            timerId += interaction.id;
            if(m_firedTimerInteractionIds.find(timerId) != m_firedTimerInteractionIds.end())
            {
                continue;
            }
            m_firedTimerInteractionIds.emplace(std::move(timerId));
            Hotspot hotspot(m_memory);
            hotspot.nodeId = FigmaString(_sourceNodeId.begin(), _sourceNodeId.end(), m_memory);
            hotspot.interaction = &interaction;
            hotspot.eventType = EPrototypeEventType::AfterTimeout;
            this->routeHotspot(hotspot, EActionInputKind::Timer, nullptr, nullptr, initialElapsed);

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

        const CanvasNodeDesc * prototypeFrame = m_currentFrame;

        if(prototypeFrame == nullptr)
        {
            FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_prototype_start_missing", "Decoded prototypeStartNodeID/prototypeStartingPoint was not found; render list is empty");
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
                FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_animation_frame_missing", "Animation source or target frame was not found; completing transition", m_animationState.clip.sourceNodeId.c_str());
                this->completeAnimation();
                prototypeFrame = m_currentFrame;
                if(prototypeFrame != nullptr)
                {
                    this->appendCanvasNode(*prototypeFrame, 1.0f, -prototypeFrame->rect.x, -prototypeFrame->rect.y, nullptr);
                }
            }
            else if(m_animationState.clip.easing == EAnimationEasing::Unsupported)
            {
                FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_animation_easing_unsupported", "Prototype transition easing is not implemented; visual animation is skipped until transition completes", m_animationState.clip.sourceNodeId.c_str());
                this->appendCanvasNode(*sourceFrame, 1.0f, -sourceFrame->rect.x, -sourceFrame->rect.y, nullptr);
            }
            else if(m_animationState.clip.smartAnimate == true)
            {
                if(m_animationState.clip.tracks.empty() == true)
                {
                    FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_smart_animate_match_missing", "Smart Animate has no decoded node id or sibling layer matches in the decoded target frame; visual animation is skipped until transition completes", m_animationState.clip.sourceNodeId.c_str());
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

                            if(track.hasQuad == true)
                            {
                                for(std::size_t index = 0; index != 4; ++index)
                                {
                                    node.quad[index].x = Detail::lerp(track.fromQuad[index].x, track.toQuad[index].x, progress);
                                    node.quad[index].y = Detail::lerp(track.fromQuad[index].y, track.toQuad[index].y, progress);
                                }

                                node.hasQuad = true;
                            }
                        }
                        else if(track.type == EAnimationTrackType::Opacity)
                        {
                            node.opacity = Detail::lerp(track.from[0], track.to[0], progress);
                            node.hasOpacity = true;
                        }
                        else if(track.type == EAnimationTrackType::Arc)
                        {
                            node.arcData.startingAngle = Detail::lerp(track.from[0], track.to[0], progress);
                            node.arcData.endingAngle = Detail::lerp(track.from[1], track.to[1], progress);
                            node.arcData.innerRadius = Detail::lerp(track.from[2], track.to[2], progress);
                            node.arcData.valid = true;
                            node.hasArcData = true;
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
                sourceLayer.renderLayerOpacity = 1.0f;

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
            else if(m_animationState.clip.transitionType == EPrototypeTransitionType::MoveIn ||
                m_animationState.clip.transitionType == EPrototypeTransitionType::MoveOut ||
                m_animationState.clip.transitionType == EPrototypeTransitionType::Push ||
                m_animationState.clip.transitionType == EPrototypeTransitionType::SlideIn ||
                m_animationState.clip.transitionType == EPrototypeTransitionType::SlideOut)
            {
                float directionX = 0.0f;
                float directionY = 0.0f;
                if(m_animationState.clip.transitionDirection == EPrototypeTransitionDirection::Left)
                {
                    directionX = m_desc.viewport.width;
                }
                else if(m_animationState.clip.transitionDirection == EPrototypeTransitionDirection::Right)
                {
                    directionX = -m_desc.viewport.width;
                }
                else if(m_animationState.clip.transitionDirection == EPrototypeTransitionDirection::Up)
                {
                    directionY = m_desc.viewport.height;
                }
                else if(m_animationState.clip.transitionDirection == EPrototypeTransitionDirection::Down)
                {
                    directionY = -m_desc.viewport.height;
                }

                const bool moveOut = m_animationState.clip.transitionType == EPrototypeTransitionType::MoveOut ||
                    m_animationState.clip.transitionType == EPrototypeTransitionType::SlideOut;
                const bool push = m_animationState.clip.transitionType == EPrototypeTransitionType::Push;

                if(moveOut == true)
                {
                    this->appendCanvasNode(*targetFrame, 1.0f, -targetFrame->rect.x, -targetFrame->rect.y, nullptr);
                    this->appendCanvasNode(*sourceFrame, 1.0f, -sourceFrame->rect.x - directionX * progress, -sourceFrame->rect.y - directionY * progress, nullptr);
                }
                else
                {
                    if(push == true)
                    {
                        this->appendCanvasNode(*sourceFrame, 1.0f, -sourceFrame->rect.x - directionX * progress, -sourceFrame->rect.y - directionY * progress, nullptr);
                    }
                    else
                    {
                        this->appendCanvasNode(*sourceFrame, 1.0f, -sourceFrame->rect.x, -sourceFrame->rect.y, nullptr);
                    }

                    this->appendCanvasNode(*targetFrame, 1.0f, -targetFrame->rect.x + directionX * (1.0f - progress), -targetFrame->rect.y + directionY * (1.0f - progress), nullptr);
                }
            }
            else
            {
                FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_transition_type_unsupported", "Prototype transition type is not implemented; visual animation is skipped until transition completes", m_animationState.clip.sourceNodeId.c_str());
                this->appendCanvasNode(*sourceFrame, 1.0f, -sourceFrame->rect.x, -sourceFrame->rect.y, nullptr);
            }
        }
        else
        {
            this->appendCanvasNode(*prototypeFrame, 1.0f, -prototypeFrame->rect.x, -prototypeFrame->rect.y, nullptr);
        }

        for(const CanvasNodeDesc * overlay : m_overlayFrames)
        {
            const float offsetX = -overlay->rect.x + (m_desc.viewport.width - overlay->rect.w) * 0.5f;
            const float offsetY = -overlay->rect.y + (m_desc.viewport.height - overlay->rect.h) * 0.5f;
            this->appendCanvasNode(*overlay, 1.0f, offsetX, offsetY, nullptr);
        }

        for(const BindingDesc & itemDesc : m_document.getBindings())
        {
            const CanvasNodeDesc * node = m_document.findCanvasNodeDesc(itemDesc.nodeId);
            if(node == nullptr)
            {
                FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "ux_binding_node_missing", "Binding target node was not found in decoded document", itemDesc.nodeId.c_str());
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
        Vec2f nodeQuad[4];
        for(std::size_t index = 0; index != 4; ++index)
        {
            nodeQuad[index] = _node.quad[index];
        }

        float nodeOpacity = _node.opacity;
        CanvasArcDataDesc nodeArcData = _node.arcData;
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

                    if(found->second.hasQuad == true)
                    {
                        for(std::size_t index = 0; index != 4; ++index)
                        {
                            nodeQuad[index] = found->second.quad[index];
                        }
                    }

                    if(found->second.hasOpacity == true)
                    {
                        nodeOpacity = found->second.opacity;
                    }

                    if(found->second.hasArcData == true)
                    {
                        nodeArcData = found->second.arcData;
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
                    FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_prototype_target_missing", "SWAP_STATE animation source or target node was not found in decoded document", _node.id.c_str());
                    return;
                }

                const float progress = Detail::applyEasing(localAnimation->easing, localAnimation->progress);
                if(localAnimation->smartAnimate == true && localAnimation->tracks.empty() == false)
                {
                    AnimationRenderContext sourceAnimation(m_memory);
                    sourceAnimation.rootNodeId = fromNode->id;
                    sourceAnimation.progress = progress;
                    sourceAnimation.smartAnimate = true;
                    sourceAnimation.targetPass = false;
                    sourceAnimation.skipRootGeometry = true;

                    AnimationRenderContext targetAnimation(m_memory);
                    targetAnimation.rootNodeId = targetNode->id;
                    targetAnimation.progress = progress;
                    targetAnimation.smartAnimate = true;
                    targetAnimation.targetPass = true;

                    for(const AnimationTrackDesc & track : localAnimation->tracks)
                    {
                        const FigmaString & targetNodeId = track.targetNodeId.empty() == false ? track.targetNodeId : track.nodeId;
                        sourceAnimation.matchedNodeIds.emplace(track.nodeId);
                        targetAnimation.matchedNodeIds.emplace(targetNodeId);
                        AnimatedNodeDesc & animatedNode = targetAnimation.targetNodes[targetNodeId];
                        animatedNode.matched = true;

                        if(track.type == EAnimationTrackType::Rect)
                        {
                            const Rectf from{track.from[0], track.from[1], track.from[2], track.from[3]};
                            const Rectf to{track.to[0], track.to[1], track.to[2], track.to[3]};
                            animatedNode.rect = Detail::lerpRect(from, to, progress);
                            animatedNode.hasRect = true;

                            if(track.hasQuad == true)
                            {
                                for(std::size_t index = 0; index != 4; ++index)
                                {
                                    animatedNode.quad[index].x = Detail::lerp(track.fromQuad[index].x, track.toQuad[index].x, progress);
                                    animatedNode.quad[index].y = Detail::lerp(track.fromQuad[index].y, track.toQuad[index].y, progress);
                                }

                                animatedNode.hasQuad = true;
                            }
                        }
                        else if(track.type == EAnimationTrackType::Opacity)
                        {
                            animatedNode.opacity = Detail::lerp(track.from[0], track.to[0], progress);
                            animatedNode.hasOpacity = true;
                        }
                        else if(track.type == EAnimationTrackType::Arc)
                        {
                            animatedNode.arcData.startingAngle = Detail::lerp(track.from[0], track.to[0], progress);
                            animatedNode.arcData.endingAngle = Detail::lerp(track.from[1], track.to[1], progress);
                            animatedNode.arcData.innerRadius = Detail::lerp(track.from[2], track.to[2], progress);
                            animatedNode.arcData.valid = true;
                            animatedNode.hasArcData = true;
                        }
                    }

                    AnimatedNodeDesc & targetRootNode = targetAnimation.targetNodes[targetNode->id];
                    targetRootNode.opacity = Detail::lerp(fromNode->opacity, targetNode->opacity, progress);
                    targetRootNode.hasOpacity = true;

                    this->appendCanvasNode(*fromNode, _parentOpacity, _offsetX + nodeRect.x - fromNode->rect.x, _offsetY + nodeRect.y - fromNode->rect.y, &sourceAnimation, renderLayerEnabled);
                    this->appendCanvasNode(*targetNode, _parentOpacity, _offsetX + nodeRect.x - targetNode->rect.x, _offsetY + nodeRect.y - targetNode->rect.y, &targetAnimation, renderLayerEnabled);
                    return;
                }

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
                    FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_prototype_target_missing", "SWAP_STATE current node was not found in decoded document", _node.id.c_str());
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
            FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_mask_unsupported", "Decoded mask node is not rendered until mask/clip composition is implemented", _node.id.c_str());
            return;
        }

        for(const PrototypeInteractionDesc & interaction : _node.prototypeInteractions)
        {
            if(interaction.eventType == EPrototypeEventType::Unsupported)
            {
                FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_prototype_event_unsupported", "Prototype event type is not implemented; interaction skipped", _node.id.c_str());
            }
            Detail::addUnsupportedPrototypeFieldDiagnostics(interaction.unsupportedFields, "fig_prototype_event_field_unsupported", "Prototype event field is not implemented", _node, &m_diagnostics);

            for(const PrototypeActionDesc & action : interaction.actions)
            {
                Detail::addUnsupportedPrototypeFieldDiagnostics(action.unsupportedFields, "fig_prototype_action_field_unsupported", "Prototype action field is not implemented", _node, &m_diagnostics);
                if(action.connectionType == EPrototypeConnectionType::Unsupported || action.navigationType == EPrototypeNavigationType::Unsupported ||
                    action.navigationType == EPrototypeNavigationType::ScrollTo)
                {
                    FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_prototype_action_unsupported", "Prototype action field is not implemented; action skipped", _node.id.c_str());
                }
                else if(action.transitionType == EPrototypeTransitionType::Unsupported)
                {
                    FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_transition_type_unsupported", "Prototype transition type is not implemented; action will not draw a guessed transition", _node.id.c_str());
                }
                if(action.transitionPreserveScroll == true)
                {
                    FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_transition_preserve_scroll_unsupported", "Prototype transition preserve-scroll behavior is not implemented", _node.id.c_str());
                }
                if(action.transitionResetVideoPosition == true)
                {
                    FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_transition_reset_video_unsupported", "Prototype transition reset-video behavior is not implemented", _node.id.c_str());
                }
            }
        }

        const Rectf rect = {
            nodeRect.x + _offsetX,
            nodeRect.y + _offsetY,
            nodeRect.w,
            nodeRect.h
        };

        const bool usePrimitiveShapeGeometry = Detail::canRenderPrimitiveShapeGeometry(_node);
        const bool hasFillPathGeometry = Detail::hasDecodedPathGeometry(_node.fillGeometry);
        const bool hasStrokePathGeometry = Detail::hasDecodedPathGeometry(_node.strokeGeometry);
        bool useFillPathGeometry = hasFillPathGeometry == true && usePrimitiveShapeGeometry == false;
        bool useStrokePathGeometry = hasStrokePathGeometry == true && usePrimitiveShapeGeometry == false;
        const bool unsupportedCompoundFillGeometry = useFillPathGeometry == true && Detail::hasCompoundPathGeometry(_node.fillGeometry) == true;
        if(unsupportedCompoundFillGeometry == true)
        {
            FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_path_compound_fill_unsupported", "Decoded fillGeometry has multiple contours; compound winding/hole triangulation is not implemented", _node.id.c_str());
            useFillPathGeometry = false;
        }
        const bool renderOwnGeometry = (_node.hasVectorDataValue == false && _node.type != ECanvasNodeType::Vector) || hasFillPathGeometry == true ||
            hasStrokePathGeometry == true || Detail::canRenderPrimitiveShapeGeometry(_node) == true;
        const bool unsupportedNodeBlendMode = renderOwnGeometry == true && _node.blendMode == ECanvasBlendMode::Unsupported;
        if(renderOwnGeometry == false)
        {
            FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_vector_network_unsupported", "Decoded vectorData has no decoded fillGeometry/strokeGeometry path commands; vectorNetworkBlob is not implemented", _node.id.c_str());
        }
        if(unsupportedNodeBlendMode == true)
        {
            FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_node_blend_mode_unsupported", "Node blendMode is not implemented; node geometry command skipped", _node.id.c_str());
            useFillPathGeometry = false;
            useStrokePathGeometry = false;
        }

        skipOwnGeometry = skipOwnGeometry == true || (animationRoot == true && _animation->skipRootGeometry == true);
        if(renderOwnGeometry == true && unsupportedNodeBlendMode == false && skipOwnGeometry == false)
        {
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
                        FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_paint_type_unsupported", "Paint type is not implemented; command skipped", _node.id.c_str());
                        return;
                    }

                    if(paint.blendMode == ECanvasBlendMode::Unsupported)
                    {
                        FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_blend_mode_unsupported", "Paint blendMode is not implemented; command skipped", _node.id.c_str());
                        return;
                    }

                    if(paint.type != ECanvasPaintType::Solid)
                    {
                        FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_path_fill_paint_unsupported", "Path-level fill paint type is not implemented; command skipped", _node.id.c_str());
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
                    Detail::assignArcData(nodeArcData, &fill);
                    if(Detail::buildPathMesh(m_memory, &fill, path, true) == false)
                    {
                        m_renderList.removeLastCommand();
                        FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_shape_geometry_failed", "Unable to build graphics mesh for decoded path fill geometry; command skipped", _node.id.c_str());
                    }
                    else
                    {
                        Detail::applyNodePathQuad(&fill, _node, nodeRect, nodeQuad);
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
                        Detail::assignArcData(nodeArcData, &fill);
                        if(Detail::buildPathPaintMesh(m_memory, &fill, groupedPathFills, opacity) == false)
                        {
                            m_renderList.removeLastCommand();
                            FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_shape_geometry_failed", "Unable to build graphics mesh for decoded path fill geometry; command skipped", _node.id.c_str());
                        }
                        else
                        {
                            Detail::applyNodePathQuad(&fill, _node, nodeRect, nodeQuad);
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
                            FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_paint_type_unsupported", "Paint type is not implemented; command skipped", _node.id.c_str());
                            continue;
                        }

                        if(paint.blendMode == ECanvasBlendMode::Unsupported)
                        {
                            FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_blend_mode_unsupported", "Paint blendMode is not implemented; command skipped", _node.id.c_str());
                            continue;
                        }

                        if(paint.type == ECanvasPaintType::Image)
                        {
                            if(paint.assetId.empty() == true)
                            {
                                FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_image_asset_missing", "Image paint has no decoded image asset id; command skipped", _node.id.c_str());
                                continue;
                            }

                            const AssetDesc * asset = m_document.findAsset(paint.assetId);
                            if(asset == nullptr)
                            {
                                FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_image_asset_missing", "Decoded image asset is not present in the .fig archive; command skipped", _node.id.c_str());
                                continue;
                            }

                            if(paint.imageScaleMode == ECanvasImageScaleMode::Tile || paint.imageScaleMode == ECanvasImageScaleMode::Unknown)
                            {
                                FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_image_scale_mode_unsupported", "Image scale mode is not implemented; command skipped", _node.id.c_str());
                                continue;
                            }

                            FigmaString boundAssetId(m_memory);
                            const FigmaString * assetId = &paint.assetId;
                            if(this->resolveImageBinding(_node, &boundAssetId) == true)
                            {
                                const AssetDesc * boundAsset = m_document.findAsset(boundAssetId);
                                if(boundAsset == nullptr)
                                {
                                    FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "ux_binding_image_asset_missing", "Image binding resolved to an unknown asset id; decoded image command kept unchanged", _node.id.c_str());
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
                            Detail::addImageQuad(&image, asset, nodeRect, nodeQuad);
                        }
                        else if(paint.type == ECanvasPaintType::Solid)
                        {
                            if(useFillPathGeometry == false && usePrimitiveShapeGeometry == false)
                            {
                                if(unsupportedCompoundFillGeometry == false)
                                {
                                    FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_path_fill_unsupported", "Decoded fillGeometry cannot be rendered as a supported primitive or simple path", _node.id.c_str());
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
                            Detail::assignArcData(nodeArcData, &fill);
                            const bool meshBuilt = useFillPathGeometry == true ? Detail::buildPathMesh(m_memory, &fill, _node.fillGeometry, true) : Detail::buildShapeMesh(m_memory, &fill, true);
                            if(meshBuilt == false)
                            {
                                m_renderList.removeLastCommand();
                                FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_shape_geometry_failed", "Unable to build graphics mesh for decoded fill geometry; command skipped", _node.id.c_str());
                            }
                            else if(useFillPathGeometry == false)
                            {
                                Detail::applyNodeQuad(&fill, nodeRect, nodeQuad);
                            }
                            else
                            {
                                Detail::applyNodePathQuad(&fill, _node, nodeRect, nodeQuad);
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
                        FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_stroke_paint_unsupported", "Only solid stroke paints are implemented; stroke command skipped", _node.id.c_str());
                        continue;
                    }

                    if(paint.blendMode == ECanvasBlendMode::Unsupported)
                    {
                        FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_blend_mode_unsupported", "Paint blendMode is not implemented; stroke command skipped", _node.id.c_str());
                        continue;
                    }

                    if(usePrimitiveShapeGeometry == true && (_node.strokeAlign == ECanvasStrokeAlign::Unsupported ||
                        _node.strokeCap == ECanvasStrokeCap::Unsupported || _node.strokeJoin == ECanvasStrokeJoin::Unsupported ||
                        _node.dashPattern.empty() == false))
                    {
                        FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_stroke_metadata_unsupported", "Decoded stroke metadata is not implemented; stroke command skipped", _node.id.c_str());
                        continue;
                    }

                    if(useStrokePathGeometry == false && usePrimitiveShapeGeometry == false)
                    {
                        FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_path_stroke_unsupported", "Decoded strokeGeometry cannot be rendered as a supported primitive or path", _node.id.c_str());
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
                    Detail::assignArcData(nodeArcData, &stroke);
                    const bool meshBuilt = useStrokePathGeometry == true ? Detail::buildPathMesh(m_memory, &stroke, _node.strokeGeometry, true) : Detail::buildShapeMesh(m_memory, &stroke, false, _node.strokeAlign);
                    if(meshBuilt == false)
                    {
                        m_renderList.removeLastCommand();
                        FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_stroke_geometry_failed", "Unable to build graphics mesh for decoded stroke geometry; command skipped", _node.id.c_str());
                    }
                    else if(useStrokePathGeometry == false)
                    {
                        Detail::applyNodeQuad(&stroke, nodeRect, nodeQuad);
                    }
                    else
                    {
                        Detail::applyNodePathQuad(&stroke, _node, nodeRect, nodeQuad);
                    }
                }
            }

            if(_node.type == ECanvasNodeType::Text && _node.text.empty() == false)
            {
                if(_node.textLines.empty() == true)
                {
                    FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_text_layout_missing", "Text node has no decoded derivedTextData/baselines; command skipped", _node.id.c_str());
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
                    FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_text_fill_missing", "Text node has no visible decoded fill paint; command skipped", _node.id.c_str());
                    return;
                }

                if(textPaint->type != ECanvasPaintType::Solid)
                {
                    FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_text_paint_unsupported", "Only solid text fill paints are implemented; text command skipped", _node.id.c_str());
                    return;
                }

                if(textPaint->blendMode == ECanvasBlendMode::Unsupported)
                {
                    FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "fig_text_blend_mode_unsupported", "Text fill blendMode is not implemented; text command skipped", _node.id.c_str());
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
                        FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "ux_binding_text_layout_unsupported", "Text binding requires reshaping/reflow for this decoded text node; command skipped", _node.id.c_str());
                    }
                }

                if(textCommandRemoved == false)
                {
                    Detail::addTextQuad(&text, nodeRect, nodeQuad);
                }
            }
        }

        const bool clipChildren = _node.type == ECanvasNodeType::Frame && _node.frameMaskDisabled == false && _node.children.empty() == false;

        if(clipChildren == true)
        {
            RenderCommand & clip = m_renderList.addCommand(ERenderCommandType::ClipBegin);
            assignRenderLayer(&clip);
            clip.id = "figma.clip_begin";
            clip.nodeId = _node.id;
            clip.rect = rect;
        }

        for(auto it = _node.children.rbegin(); it != _node.children.rend(); ++it)
        {
            this->appendCanvasNode(*it, opacity, _offsetX, _offsetY, _animation, renderLayerEnabled);
        }

        if(clipChildren == true)
        {
            RenderCommand & clip = m_renderList.addCommand(ERenderCommandType::ClipEnd);
            assignRenderLayer(&clip);
            clip.id = "figma.clip_end";
            clip.nodeId = _node.id;
            clip.rect = rect;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::appendPrototypeHotspots(const CanvasNodeDesc & _node, const CanvasNodeDesc & _frame, float _offsetX, float _offsetY, const Rectf * _clip)
    {
        (void)_frame;

        if(_node.visible == false || this->isNodeVisibleByBinding(_node) == false || this->isNodeEnabledByBinding(_node) == false)
        {
            return;
        }

        const NodeSwapState * swap = this->findNodeSwapState(_node.id);
        if(swap != nullptr)
        {
            const CanvasNodeDesc * swappedNode = m_document.findCanvasNodeDesc(swap->currentNodeId);
            if(swappedNode != nullptr)
            {
                this->appendPrototypeHotspots(*swappedNode, _frame, _offsetX + _node.rect.x - swappedNode->rect.x, _offsetY + _node.rect.y - swappedNode->rect.y, _clip);
            }
            return;
        }

        Rectf nodeRect{
            _node.rect.x + _offsetX,
            _node.rect.y + _offsetY,
            _node.rect.w,
            _node.rect.h
        };

        Vec2f nodeQuad[4];
        bool hasQuad = false;
        for(std::size_t index = 0; index != 4; ++index)
        {
            nodeQuad[index].x = _node.quad[index].x + _offsetX;
            nodeQuad[index].y = _node.quad[index].y + _offsetY;
            hasQuad = hasQuad == true || _node.quad[index].x != 0.0f || _node.quad[index].y != 0.0f;
        }

        if(hasQuad == false)
        {
            nodeQuad[0] = {nodeRect.x, nodeRect.y};
            nodeQuad[1] = {nodeRect.x + nodeRect.w, nodeRect.y};
            nodeQuad[2] = {nodeRect.x + nodeRect.w, nodeRect.y + nodeRect.h};
            nodeQuad[3] = {nodeRect.x, nodeRect.y + nodeRect.h};
        }

        float quadLeft = nodeQuad[0].x;
        float quadTop = nodeQuad[0].y;
        float quadRight = nodeQuad[0].x;
        float quadBottom = nodeQuad[0].y;
        for(std::size_t index = 1; index != 4; ++index)
        {
            quadLeft = std::min(quadLeft, nodeQuad[index].x);
            quadTop = std::min(quadTop, nodeQuad[index].y);
            quadRight = std::max(quadRight, nodeQuad[index].x);
            quadBottom = std::max(quadBottom, nodeQuad[index].y);
        }
        nodeRect = {quadLeft, quadTop, quadRight - quadLeft, quadBottom - quadTop};

        Rectf childClip{};
        const Rectf * effectiveClip = _clip;
        if(_node.type == ECanvasNodeType::Frame && _node.frameMaskDisabled == false)
        {
            childClip = nodeRect;
            if(_clip != nullptr)
            {
                const float left = std::max(childClip.x, _clip->x);
                const float top = std::max(childClip.y, _clip->y);
                const float right = std::min(childClip.x + childClip.w, _clip->x + _clip->w);
                const float bottom = std::min(childClip.y + childClip.h, _clip->y + _clip->h);
                childClip = {left, top, std::max(0.0f, right - left), std::max(0.0f, bottom - top)};
            }
            effectiveClip = &childClip;
        }

        for(const PrototypeInteractionDesc & interaction : _node.prototypeInteractions)
        {
            if(interaction.eventType == EPrototypeEventType::AfterTimeout || interaction.eventType == EPrototypeEventType::Unsupported)
            {
                continue;
            }

            Hotspot hotspot(m_memory);
            hotspot.rect = nodeRect;
            for(std::size_t index = 0; index != 4; ++index)
            {
                hotspot.quad[index] = nodeQuad[index];
            }
            if(effectiveClip != nullptr)
            {
                hotspot.clip = *effectiveClip;
                hotspot.hasClip = true;
            }
            hotspot.nodeId = _node.id;
            hotspot.interaction = &interaction;
            hotspot.eventType = interaction.eventType;
            hotspot.keyCode = interaction.keyCode;
            m_hotspots.emplace_back(std::move(hotspot));
        }

        for(auto it = _node.children.rbegin(); it != _node.children.rend(); ++it)
        {
            this->appendPrototypeHotspots(*it, _frame, _offsetX, _offsetY, effectiveClip);
        }
    }
    //////////////////////////////////////////////////////////////////////////
    void Player::rebuildHotspots()
    {
        m_hotspots.clear();
        m_hotspotsDirty = false;

        const CanvasNodeDesc * prototypeFrame = m_currentFrame;

        if(prototypeFrame == nullptr)
        {
            return;
        }

        Rectf viewportClip{0.0f, 0.0f, m_desc.viewport.width, m_desc.viewport.height};
        this->appendPrototypeHotspots(*prototypeFrame, *prototypeFrame, -prototypeFrame->rect.x, -prototypeFrame->rect.y, &viewportClip);

        for(const CanvasNodeDesc * overlay : m_overlayFrames)
        {
            const float offsetX = -overlay->rect.x + (m_desc.viewport.width - overlay->rect.w) * 0.5f;
            const float offsetY = -overlay->rect.y + (m_desc.viewport.height - overlay->rect.h) * 0.5f;
            this->appendPrototypeHotspots(*overlay, *overlay, offsetX, offsetY, &viewportClip);
        }

        const ActionVector & actions = m_document.getActions();
        for(const ActionDesc & action : actions)
        {
            if(action.eventType == EPrototypeEventType::Unsupported)
            {
                continue;
            }

            const CanvasNodeDesc * node = m_document.findCanvasNodeDesc(action.nodeId);
            if(node == nullptr)
            {
                FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "ux_action_node_missing", "Action target node was not found in decoded document", action.nodeId.c_str());
                continue;
            }

            const CanvasNodeDesc * ownerFrame = nullptr;
            float offsetX = 0.0f;
            float offsetY = 0.0f;
            if(Detail::containsNode(*prototypeFrame, node) == true)
            {
                ownerFrame = prototypeFrame;
                offsetX = -prototypeFrame->rect.x;
                offsetY = -prototypeFrame->rect.y;
            }

            for(const CanvasNodeDesc * overlay : m_overlayFrames)
            {
                if(Detail::containsNode(*overlay, node) == true)
                {
                    ownerFrame = overlay;
                    offsetX = -overlay->rect.x + (m_desc.viewport.width - overlay->rect.w) * 0.5f;
                    offsetY = -overlay->rect.y + (m_desc.viewport.height - overlay->rect.h) * 0.5f;
                }
            }

            if(ownerFrame == nullptr || node->visible == false || this->isNodeVisibleByBinding(*node) == false || this->isNodeEnabledByBinding(*node) == false)
            {
                continue;
            }

            Hotspot hotspot(m_memory);
            hotspot.rect = {
                node->rect.x + offsetX,
                node->rect.y + offsetY,
                node->rect.w,
                node->rect.h
            };
            bool hasActionQuad = false;
            for(std::size_t index = 0; index != 4; ++index)
            {
                hotspot.quad[index].x = node->quad[index].x + offsetX;
                hotspot.quad[index].y = node->quad[index].y + offsetY;
                if(node->quad[index].x != 0.0f || node->quad[index].y != 0.0f)
                {
                    hasActionQuad = true;
                }
            }
            if(hasActionQuad == false)
            {
                hotspot.quad[0] = {hotspot.rect.x, hotspot.rect.y};
                hotspot.quad[1] = {hotspot.rect.x + hotspot.rect.w, hotspot.rect.y};
                hotspot.quad[2] = {hotspot.rect.x + hotspot.rect.w, hotspot.rect.y + hotspot.rect.h};
                hotspot.quad[3] = {hotspot.rect.x, hotspot.rect.y + hotspot.rect.h};
            }
            hotspot.clip = viewportClip;
            hotspot.hasClip = true;
            hotspot.nodeId = action.nodeId;
            hotspot.actionId = action.actionId;
            hotspot.targetFrameId = action.targetFrameId;
            hotspot.eventType = action.eventType;
            hotspot.keyCode = action.keyCode;
            hotspot.uxAction = true;
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
                FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "ux_binding_text_target_invalid", "Text binding target is not a decoded text node; command skipped", _node.id.c_str());
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
                FIGMA_DIAGNOSTICS_ADD(m_diagnostics, EDiagnosticSeverity::Warning, "ux_binding_image_value_invalid", "Image binding did not resolve to an asset id string; decoded image command kept unchanged", _node.id.c_str());
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
    void Player::setCurrentFrame(const CanvasNodeDesc * _frame)
    {
        FigmaString previousFrameId(m_currentFrameId.begin(), m_currentFrameId.end(), m_memory);
        m_currentFrame = _frame;
        m_currentFrameId.clear();

        if(_frame != nullptr)
        {
            m_currentFrameId = _frame->id;
        }

        m_hotspotsDirty = true;
        m_firedTimerInteractionIds.clear();

        if(m_actionRouter != nullptr && previousFrameId != m_currentFrameId)
        {
            m_actionRouter->onFrameChanged(previousFrameId, m_currentFrameId);
        }
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
    EResult createPlayerFromDocument(DocumentInterface * const _document, const PlayerDesc & _desc, PlayerInterface ** const _player)
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

        FigmaMemoryResource * memory = document->getMemory();
        void * playerMemory = nullptr;

        try
        {
            playerMemory = memory->allocate(sizeof(Player), alignof(Player));
            *_player = new(playerMemory) Player(*document, _desc, memory);
        }
        catch(const std::bad_alloc &)
        {
            if(playerMemory != nullptr)
            {
                memory->deallocate(playerMemory, sizeof(Player), alignof(Player));
            }

            return EResult::OutOfMemory;
        }
        catch(...)
        {
            if(playerMemory != nullptr)
            {
                memory->deallocate(playerMemory, sizeof(Player), alignof(Player));
            }

            throw;
        }

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
}

#include "RenderList.h"

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    namespace Detail
    {
        //////////////////////////////////////////////////////////////////////////
        static ERenderBatchType renderBatchTypeFromCommand(const RenderCommand & _command)
        {
            if(_command.type == ERenderCommandType::ClipBegin)
            {
                return ERenderBatchType::ClipBegin;
            }

            if(_command.type == ERenderCommandType::ClipEnd)
            {
                return ERenderBatchType::ClipEnd;
            }

            return ERenderBatchType::Geometry;
        }
        //////////////////////////////////////////////////////////////////////////
        static ERenderShaderType renderShaderTypeFromCommand(const RenderCommand & _command)
        {
            if(_command.type == ERenderCommandType::Image || _command.type == ERenderCommandType::Text)
            {
                return ERenderShaderType::Texture;
            }

            if(_command.type == ERenderCommandType::DebugHotspot)
            {
                return ERenderShaderType::Debug;
            }

            return ERenderShaderType::Color;
        }
        //////////////////////////////////////////////////////////////////////////
        static ERenderTextureType renderTextureTypeFromCommand(const RenderCommand & _command)
        {
            if(_command.type == ERenderCommandType::Image)
            {
                return ERenderTextureType::Asset;
            }

            if(_command.type == ERenderCommandType::Text)
            {
                return ERenderTextureType::Generated;
            }

            return ERenderTextureType::None;
        }
        //////////////////////////////////////////////////////////////////////////
        static FigmaStringView renderTextureKeyFromCommand(const RenderCommand & _command)
        {
            if(_command.type == ERenderCommandType::Image)
            {
                return FigmaStringView(_command.assetId.data(), _command.assetId.size());
            }

            if(_command.type == ERenderCommandType::Text)
            {
                return FigmaStringView(_command.nodeId.data(), _command.nodeId.size());
            }

            return FigmaStringView();
        }
        //////////////////////////////////////////////////////////////////////////
        static void makeRenderBatchDesc(const RenderCommand & _command, RenderBatchDesc * const _batch)
        {
            _batch->batchType = renderBatchTypeFromCommand(_command);
            _batch->shaderType = renderShaderTypeFromCommand(_command);
            _batch->textureType = renderTextureTypeFromCommand(_command);
            _batch->textureKey = renderTextureKeyFromCommand(_command);
            _batch->blendMode = _command.blendMode;
            _batch->opacity = _command.opacity;
            _batch->renderLayerId = _command.renderLayerId;
            _batch->renderLayerOpacity = _command.renderLayerOpacity;
            for(std::size_t index = 0; index != 8; ++index)
            {
                _batch->filterColorAdjust[index] = _command.filterColorAdjust[index];
            }
            for(std::size_t index = 0; index != 10; ++index)
            {
                _batch->paintFilter[index] = _command.paintFilter[index];
            }
            _batch->clipRect = _command.rect;
            _batch->vertexCount = static_cast<std::uint32_t>(_command.vertices.size());
            _batch->vertices = _command.vertices.empty() == false ? _command.vertices.data() : nullptr;
            _batch->indexCount = static_cast<std::uint32_t>(_command.indices.size());
            _batch->indices = _command.indices.empty() == false ? _command.indices.data() : nullptr;
            _batch->hasFilterColorAdjustValue = _command.hasFilterColorAdjustValue;
            _batch->hasPaintFilterValue = _command.hasPaintFilterValue;
        }
        //////////////////////////////////////////////////////////////////////////
        static FigmaStringView makeStringView(const FigmaString & _value)
        {
            return FigmaStringView(_value.data(), _value.size());
        }
        //////////////////////////////////////////////////////////////////////////
        static void makeGeneratedTextureDesc(const RenderCommand & _command, RenderGeneratedTextureDesc * const _desc)
        {
            _desc->key = makeStringView(_command.nodeId);
            _desc->text = makeStringView(_command.text);
            _desc->fontFamily = makeStringView(_command.fontFamily);
            _desc->fontStyle = makeStringView(_command.fontStyle);
            _desc->fontPostscriptName = makeStringView(_command.fontPostscriptName);
            _desc->rect = _command.rect;
            _desc->color = _command.color;
            _desc->textAlignHorizontal = _command.textAlignHorizontal;
            _desc->textAlignVertical = _command.textAlignVertical;
            _desc->fontSize = _command.fontSize;
            _desc->lineHeight = _command.lineHeight;
            _desc->fontWeight = _command.fontWeight;
            _desc->textLineCount = static_cast<std::uint32_t>(_command.textLines.size());
        }
        //////////////////////////////////////////////////////////////////////////
        static void makeGeneratedTextLineDesc(const RenderTextLineDesc & _source, RenderGeneratedTextLineDesc * const _line)
        {
            _line->text = makeStringView(_source.text);
            _line->x = _source.x;
            _line->y = _source.y;
            _line->width = _source.width;
            _line->lineHeight = _source.lineHeight;
            _line->lineAscent = _source.lineAscent;
        }
    }
    //////////////////////////////////////////////////////////////////////////
    RenderTextLineDesc::RenderTextLineDesc(FigmaMemoryResource * _memory)
        : text(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    RenderCommand::RenderCommand(FigmaMemoryResource * _memory)
        : id(_memory)
        , nodeId(_memory)
        , assetId(_memory)
        , text(_memory)
        , fontFamily(_memory)
        , fontStyle(_memory)
        , fontPostscriptName(_memory)
        , rect{}
        , color{1.0f, 1.0f, 1.0f, 1.0f}
        , textLines(_memory)
        , vertices(_memory)
        , indices(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    RenderList::RenderList(FigmaMemoryResource * _memory)
        : m_memory(_memory)
        , m_commands(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    std::uint32_t RenderList::getBatchCount() const
    {
        return static_cast<std::uint32_t>(m_commands.size());
    }
    //////////////////////////////////////////////////////////////////////////
    EResult RenderList::getBatch(std::uint32_t _index, RenderBatchDesc * const _batch) const
    {
        if(_batch == nullptr)
        {
            return EResult::InvalidArgument;
        }

        if(_index >= m_commands.size())
        {
            return EResult::NotFound;
        }

        Detail::makeRenderBatchDesc(m_commands[_index], _batch);

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult RenderList::getGeneratedTexture(std::uint32_t _index, RenderGeneratedTextureDesc * const _desc) const
    {
        if(_desc == nullptr)
        {
            return EResult::InvalidArgument;
        }

        if(_index >= m_commands.size())
        {
            return EResult::NotFound;
        }

        const RenderCommand & command = m_commands[_index];

        if(command.type != ERenderCommandType::Text)
        {
            return EResult::InvalidArgument;
        }

        Detail::makeGeneratedTextureDesc(command, _desc);

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    EResult RenderList::getGeneratedTextureTextLine(std::uint32_t _index, std::uint32_t _lineIndex, RenderGeneratedTextLineDesc * const _line) const
    {
        if(_line == nullptr)
        {
            return EResult::InvalidArgument;
        }

        if(_index >= m_commands.size())
        {
            return EResult::NotFound;
        }

        const RenderCommand & command = m_commands[_index];

        if(command.type != ERenderCommandType::Text)
        {
            return EResult::InvalidArgument;
        }

        if(_lineIndex >= command.textLines.size())
        {
            return EResult::NotFound;
        }

        Detail::makeGeneratedTextLineDesc(command.textLines[_lineIndex], _line);

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    void RenderList::clear()
    {
        m_commands.clear();
    }
    //////////////////////////////////////////////////////////////////////////
    RenderCommand & RenderList::addCommand(ERenderCommandType _type)
    {
        RenderCommand command(m_memory);
        command.type = _type;
        m_commands.emplace_back(std::move(command));
        return m_commands.back();
    }
    //////////////////////////////////////////////////////////////////////////
    void RenderList::removeLastCommand()
    {
        if(m_commands.empty() == false)
        {
            m_commands.pop_back();
        }
    }
    //////////////////////////////////////////////////////////////////////////
    const RenderCommandVector & RenderList::getCommands() const
    {
        return m_commands;
    }
    //////////////////////////////////////////////////////////////////////////
}

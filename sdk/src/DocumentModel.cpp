#include "DocumentModel.h"

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    CanvasPathDesc::CanvasPathDesc( FigmaMemoryResource * _memory )
        : commands( _memory )
        , paints( _memory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
    CanvasPathStyleOverrideDesc::CanvasPathStyleOverrideDesc( FigmaMemoryResource * _memory )
        : fills( _memory )
        , strokes( _memory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
    CanvasTextLineDesc::CanvasTextLineDesc( FigmaMemoryResource * _memory )
        : text( _memory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
    PrototypeActionDesc::PrototypeActionDesc( FigmaMemoryResource * _memory )
        : targetNodeId( _memory )
        , rawConnectionType( _memory )
        , rawNavigationType( _memory )
        , rawTransitionType( _memory )
        , rawTransitionDirection( _memory )
        , rawTransitionEasing( _memory )
        , unsupportedFields( _memory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
    PrototypeInteractionDesc::PrototypeInteractionDesc( FigmaMemoryResource * _memory )
        : id( _memory )
        , rawEventType( _memory )
        , actions( _memory )
        , unsupportedFields( _memory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
    CanvasNodeDesc::CanvasNodeDesc( FigmaMemoryResource * _memory )
        : id( _memory )
        , name( _memory )
        , rect{}
        , quad{}
        , vectorNormalizedSize{}
        , text( _memory )
        , fontFamily( _memory )
        , fontStyle( _memory )
        , fontPostscriptName( _memory )
        , prototypeStartNodeId( _memory )
        , symbolId( _memory )
        , fillStyleNodeId( _memory )
        , strokeFillStyleNodeId( _memory )
        , rawBlendMode( _memory )
        , dashPattern( _memory )
        , pathStyleOverrides( _memory )
        , fillGeometry( _memory )
        , strokeGeometry( _memory )
        , prototypeInteractions( _memory )
        , textLines( _memory )
        , fills( _memory )
        , strokes( _memory )
        , children( _memory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
}

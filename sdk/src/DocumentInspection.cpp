#include "DocumentInspection.h"

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    AnimationTrackDesc::AnimationTrackDesc( FigmaMemoryResource * _memory )
        : nodeId( _memory )
        , targetNodeId( _memory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
    AnimationClipDesc::AnimationClipDesc( FigmaMemoryResource * _memory )
        : id( _memory )
        , sourceFrameId( _memory )
        , targetFrameId( _memory )
        , sourceNodeId( _memory )
        , tracks( _memory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
    PlayerAnimationStateDesc::PlayerAnimationStateDesc( FigmaMemoryResource * _memory )
        : clip( _memory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
    BindingDesc::BindingDesc( FigmaMemoryResource * _memory )
        : nodeId( _memory )
        , key( _memory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
    ActionDesc::ActionDesc( FigmaMemoryResource * _memory )
        : nodeId( _memory )
        , actionId( _memory )
        , targetFrameId( _memory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
}

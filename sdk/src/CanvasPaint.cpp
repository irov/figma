#include "CanvasPaint.h"

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    CanvasPaint::CanvasPaint(FigmaMemoryResource * _memory)
        : color{1.0f, 1.0f, 1.0f, 1.0f}
        , assetId(_memory)
        , rawType(_memory)
        , rawBlendMode(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
}

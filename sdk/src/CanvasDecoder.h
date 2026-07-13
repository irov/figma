#pragma once

#include "Figma/RuntimeInterface.h"

namespace Figma
{
    class Document;

    bool decodeCanvas( RuntimeInterface * const _runtime, const FigmaByteBuffer & _bytes, Document * const _document, DiagnosticsInterface * const _diagnostics );
}

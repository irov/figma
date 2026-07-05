#pragma once

#include "Document.h"
#include "Figma/RuntimeInterface.h"

namespace Figma
{
    bool decodeCanvas(RuntimeInterface * const _runtime, const FigmaByteBuffer & _bytes, Document * const _document, DiagnosticsInterface * const _diagnostics);
}

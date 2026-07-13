#pragma once

#include "Figma/RuntimeInterface.h"

#include "DocumentInspection.h"

namespace Figma
{
    EResult loadDocumentFromArchiveData(
        RuntimeInterface * const _runtime, const void * _data, std::size_t _size, const LoadOptions & _options, DocumentInterface ** const _document );
    EResult loadDocumentUX(
        FigmaMemoryResource * _memory, DiagnosticsInterface * const _diagnostics, FigmaStringView _data, BindingVector * const _bindings, ActionVector * const _actions );
}

#pragma once

#include "Figma/RuntimeInterface.h"

namespace Figma
{
    EResult loadDocumentFromArchiveDataImpl(RuntimeInterface * const _runtime, const void * _data, std::size_t _size, const LoadOptions & _options, DocumentInterface ** const _document);
}

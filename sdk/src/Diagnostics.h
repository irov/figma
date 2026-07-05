#pragma once

#include "Figma/DiagnosticsInterface.h"

namespace Figma
{
    class Diagnostics final
        : public DiagnosticsInterface
    {
    public:
        explicit Diagnostics(FigmaMemoryResource * _memory = getDefaultMemoryResource());

        void clear() override;
        void add(EDiagnosticSeverity _severity, const Char * _code, const Char * _message, const Char * _nodeId = "") override;
        bool hasErrors() const override;
        const DiagnosticVector & getItems() const override;

    protected:
        FigmaMemoryResource * m_memory;
        DiagnosticVector m_items;
    };
}

#pragma once

#include "Figma/Types.h"

namespace Figma
{
    enum class EDiagnosticSeverity
    {
        Info,
        Warning,
        Error
    };

    struct Diagnostic
    {
        explicit Diagnostic( FigmaMemoryResource * _memory = getDefaultMemoryResource() )
            : code( _memory )
            , message( _memory )
            , nodeId( _memory )
        {
        }

        EDiagnosticSeverity severity = EDiagnosticSeverity::Info;
        FigmaString code;
        FigmaString message;
        FigmaString nodeId;
    };

    using DiagnosticVector = FigmaVector<Diagnostic>;

    class DiagnosticsInterface
    {
    public:
        virtual void clear() = 0;
        virtual void add( EDiagnosticSeverity _severity, const Char * _code, const Char * _message, const Char * _nodeId = "" ) = 0;
        virtual bool hasErrors() const = 0;

        virtual const DiagnosticVector & getItems() const = 0;

    protected:
        ~DiagnosticsInterface() = default;
    };
}

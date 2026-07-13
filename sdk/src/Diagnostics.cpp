#include "Diagnostics.h"

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    Diagnostics::Diagnostics(FigmaMemoryResource * _memory)
        : m_memory(_memory)
        , m_items(_memory)
    {
    }
    //////////////////////////////////////////////////////////////////////////
    void Diagnostics::clear()
    {
        m_items.clear();
    }
    //////////////////////////////////////////////////////////////////////////
    void Diagnostics::add(EDiagnosticSeverity _severity, const Char * _code, const Char * _message, const Char * _nodeId)
    {
#if FIGMA_ENABLE_DIAGNOSTICS
        Diagnostic diagnostic(m_memory);
        diagnostic.severity = _severity;
        diagnostic.code = _code != nullptr ? _code : "";
        diagnostic.message = _message != nullptr ? _message : "";
        diagnostic.nodeId = _nodeId != nullptr ? _nodeId : "";
        m_items.emplace_back(std::move(diagnostic));
#else
        (void)_severity;
        (void)_code;
        (void)_message;
        (void)_nodeId;
#endif
    }
    //////////////////////////////////////////////////////////////////////////
    bool Diagnostics::hasErrors() const
    {
        for(const Diagnostic & diagnostic : m_items)
        {
            if(diagnostic.severity == EDiagnosticSeverity::Error)
            {
                return true;
            }
        }

        return false;
    }
    //////////////////////////////////////////////////////////////////////////
    const DiagnosticVector & Diagnostics::getItems() const
    {
        return m_items;
    }
    //////////////////////////////////////////////////////////////////////////
}

#pragma once

#include "Figma/DiagnosticsInterface.h"
#include "Figma/Types.h"

#include "JsonDocument.h"

#include "json/json.hpp"

namespace Figma
{
    EResult parseJson( FigmaMemoryResource * _memory, FigmaStringView _data, DiagnosticsInterface * const _diagnostics, JsonDocument * const _document );

    const js_element_t * jsonMember( const js_element_t * _value, FigmaStringView _key );
    const js_element_t * jsonIndex( const js_element_t * _value, std::size_t _index );
    FigmaStringView jsonString( const js_element_t * _value, FigmaStringView _defaultValue = {} );
    double jsonNumber( const js_element_t * _value, double _defaultValue = 0.0 );
    bool jsonBool( const js_element_t * _value, bool _defaultValue = false );
}

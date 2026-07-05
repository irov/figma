#pragma once

#include "json/json.hpp"

namespace Figma
{
    class JsonDocument
    {
    public:
        JsonDocument();
        ~JsonDocument();

        JsonDocument(const JsonDocument & _doc) = delete;
        JsonDocument & operator=(const JsonDocument & _doc) = delete;

        js_element_t ** getAddress();
        const js_element_t * getRoot() const;

    protected:
        js_element_t * m_root;
    };
}

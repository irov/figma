#include "JsonUtils.h"

#include "DiagnosticsMacros.h"

#include <cstddef>
#include <cstring>

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    namespace Detail
    {
        struct AllocationHeader
        {
            std::size_t size = 0;
        };

        struct JsonParseContext
        {
            DiagnosticsInterface * diagnostics = nullptr;
        };
        //////////////////////////////////////////////////////////////////////////
        static void * jsonAlloc(std::size_t _size, void * _userData)
        {
            FigmaMemoryResource * memory = static_cast<FigmaMemoryResource *>(_userData);
            const std::size_t total = sizeof(AllocationHeader) + _size;
            void * raw = memory->allocate(total, alignof(std::max_align_t));
            AllocationHeader * header = static_cast<AllocationHeader *>(raw);
            header->size = _size;
            return static_cast<void *>(header + 1);
        }
        //////////////////////////////////////////////////////////////////////////
        static void jsonFree(void * _ptr, void * _userData)
        {
            if(_ptr == nullptr)
            {
                return;
            }

            FigmaMemoryResource * memory = static_cast<FigmaMemoryResource *>(_userData);
            AllocationHeader * header = static_cast<AllocationHeader *>(_ptr) - 1;
            memory->deallocate(header, sizeof(AllocationHeader) + header->size, alignof(std::max_align_t));
        }
        //////////////////////////////////////////////////////////////////////////
        static void jsonFailed(const char * _pointer, const char * _end, const char * _message, void * _userData)
        {
            (void)_pointer;
            (void)_end;

            JsonParseContext * context = static_cast<JsonParseContext *>(_userData);
            if(context != nullptr && context->diagnostics != nullptr)
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER(context->diagnostics, EDiagnosticSeverity::Error, "json_parse_failed", _message != nullptr ? _message : "JSON parse failed");
            }
        }
    }
    //////////////////////////////////////////////////////////////////////////
    EResult parseJson(FigmaMemoryResource * _memory, FigmaStringView _data, DiagnosticsInterface * const _diagnostics, JsonDocument * const _document)
    {
        if(_memory == nullptr || _document == nullptr)
        {
            return EResult::InvalidArgument;
        }

        js_allocator_t allocator;
        js_make_allocator_default(&Detail::jsonAlloc, &Detail::jsonFree, _memory, &allocator);

        Detail::JsonParseContext context;
        context.diagnostics = _diagnostics;

        js_element_t * root = nullptr;
        const js_result_t result = js_parse(allocator, js_flag_none, _data.data(), _data.size(), &Detail::jsonFailed, &context, &root);
        if(result != JS_SUCCESSFUL || root == nullptr)
        {
            if(_diagnostics != nullptr)
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER(_diagnostics, EDiagnosticSeverity::Error, "json_parse_failed", "Unable to parse JSON");
            }

            return EResult::ParseFailed;
        }

        *_document->getAddress() = root;

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    const js_element_t * jsonMember(const js_element_t * _value, FigmaStringView _key)
    {
        if(_value == nullptr || js_is_object(_value) != JS_TRUE)
        {
            return nullptr;
        }

        js_string_t key{_key.data(), _key.size()};
        return js_object_getn(_value, key);
    }
    //////////////////////////////////////////////////////////////////////////
    const js_element_t * jsonIndex(const js_element_t * _value, std::size_t _index)
    {
        if(_value == nullptr || js_is_array(_value) != JS_TRUE)
        {
            return nullptr;
        }

        const std::size_t size = js_array_size(_value);
        if(_index >= size)
        {
            return nullptr;
        }

        return js_array_get(_value, size - _index - 1);
    }
    //////////////////////////////////////////////////////////////////////////
    FigmaStringView jsonString(const js_element_t * _value, FigmaStringView _defaultValue)
    {
        if(_value == nullptr || js_is_string(_value) != JS_TRUE)
        {
            return _defaultValue;
        }

        js_string_t value;
        js_get_string(_value, &value);
        return {value.value, value.size};
    }
    //////////////////////////////////////////////////////////////////////////
    double jsonNumber(const js_element_t * _value, double _defaultValue)
    {
        if(_value == nullptr)
        {
            return _defaultValue;
        }

        if(js_is_integer(_value) == JS_TRUE)
        {
            return static_cast<double>(js_get_integer(_value));
        }

        if(js_is_real(_value) == JS_TRUE)
        {
            return js_get_real(_value);
        }

        return _defaultValue;
    }
    //////////////////////////////////////////////////////////////////////////
    bool jsonBool(const js_element_t * _value, bool _defaultValue)
    {
        if(_value == nullptr || js_is_boolean(_value) != JS_TRUE)
        {
            return _defaultValue;
        }

        return js_get_boolean(_value) == JS_TRUE;
    }
    //////////////////////////////////////////////////////////////////////////
}

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory_resource>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#if !defined(FIGMA_ENABLE_DIAGNOSTICS)
#   if defined(NDEBUG)
#       define FIGMA_ENABLE_DIAGNOSTICS 0
#   else
#       define FIGMA_ENABLE_DIAGNOSTICS 1
#   endif
#endif

#if defined(FIGMA_STATIC)
#   define FIGMA_EXPORT
#elif defined(_WIN32)
#   if defined(FIGMA_BUILDING_SDK)
#       define FIGMA_EXPORT __declspec(dllexport)
#   else
#       define FIGMA_EXPORT __declspec(dllimport)
#   endif
#elif defined(FIGMA_BUILDING_SDK)
#   define FIGMA_EXPORT __attribute__((visibility("default")))
#else
#   define FIGMA_EXPORT
#endif

namespace Figma
{
    inline constexpr std::uint32_t FIGMA_SDK_VERSION = 3;

    using FigmaChar = char;
    using Char = FigmaChar;
    using FigmaMemoryResource = std::pmr::memory_resource;

    template<class T>
    using FigmaAllocator = std::pmr::polymorphic_allocator<T>;

    template<class T>
    struct FigmaVectorStaticAssertionBoolSpecialization
    {
        static_assert(std::is_same<T, bool>::value == false, "figma vector bool specialization is not allowed");

        using type = T;
    };

    template<class T>
    using FigmaVector = std::pmr::vector<typename FigmaVectorStaticAssertionBoolSpecialization<T>::type>;

    template<class TKey, class TValue, class THash = std::hash<TKey>, class TEqual = std::equal_to<TKey>>
    using FigmaUnorderedMap = std::pmr::unordered_map<TKey, TValue, THash, TEqual>;

    template<class TKey, class THash = std::hash<TKey>, class TEqual = std::equal_to<TKey>>
    using FigmaUnorderedSet = std::pmr::unordered_set<TKey, THash, TEqual>;

    using FigmaString = std::basic_string<Char, std::char_traits<Char>, FigmaAllocator<Char>>;
    using FigmaStringView = std::basic_string_view<Char, std::char_traits<Char>>;

    using FigmaByteBuffer = FigmaVector<std::uint8_t>;
    using FigmaGetDefaultMemoryResource = FigmaMemoryResource * (*)() noexcept;
    inline constexpr FigmaGetDefaultMemoryResource getDefaultMemoryResource = std::pmr::get_default_resource;

    enum class EResult
    {
        Ok,
        InvalidArgument,
        OutOfMemory,
        IoFailed,
        ParseFailed,
        UnsupportedFormat,
        MissingEntry,
        NotFound,
        InvalidState,
        VersionMismatch
    };

    struct Vec2f
    {
        float x;
        float y;
    };

    struct Rectf
    {
        float x;
        float y;
        float w;
        float h;
    };

    struct Color
    {
        float r;
        float g;
        float b;
        float a;
    };
}

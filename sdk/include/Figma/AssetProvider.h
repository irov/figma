#pragma once

#include "Figma/Types.h"

namespace Figma
{
    struct AssetDesc;
    using AssetVector = FigmaVector<AssetDesc>;
    using AssetByteBuffer = FigmaByteBuffer;

    struct AssetDesc
    {
        explicit AssetDesc( FigmaMemoryResource * _memory = getDefaultMemoryResource() )
            : id( _memory )
            , path( _memory )
            , mime( _memory )
            , bytes( _memory )
        {
        }

        FigmaString id;
        FigmaString path;
        FigmaString mime;
        AssetByteBuffer bytes;
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::uint8_t colorType = 0;
    };

    class AssetProviderInterface
    {
    public:
        virtual const AssetDesc * findAsset( FigmaStringView _assetId ) const = 0;

    public:
        virtual void destroy() = 0;

    protected:
        ~AssetProviderInterface() = default;
    };
}

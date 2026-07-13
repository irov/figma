#pragma once

#include "Figma/AssetProvider.h"
#include "Figma/DiagnosticsInterface.h"
#include "Figma/Types.h"

namespace Figma
{
    struct LoadOptions
    {
        FigmaStringView sourceName;
        bool extractImageAssets = true;
        bool keepCanvasBytes = true;
    };

    class DocumentInterface
        : public AssetProviderInterface
    {
    public:
        virtual EResult loadUX(FigmaStringView _data) = 0;
        virtual bool getFrameRect(FigmaStringView _nodeId, Rectf * const _rect) const = 0;
        virtual bool getPrototypeStartFrameRect(Rectf * const _rect) const = 0;
        virtual const DiagnosticsInterface * getDiagnostics() const = 0;

    public:
        void destroy() override = 0;

    protected:
        ~DocumentInterface() = default;
    };
}

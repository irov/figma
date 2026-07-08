#pragma once

#include "Figma/DocumentInterface.h"

#include "DocumentModel.h"
#include "Diagnostics.h"

namespace Figma
{
    class RuntimeInterface;
    class Document;

    EResult loadDocumentFromArchiveDataImpl(RuntimeInterface * const _runtime, const void * _data, std::size_t _size, const LoadOptions & _options, DocumentInterface ** const _document);
    bool decodeCanvas(RuntimeInterface * const _runtime, const FigmaByteBuffer & _bytes, Document * const _document, DiagnosticsInterface * const _diagnostics);

    class Document final
        : public DocumentInterface
        , public DocumentInspectionInterface
    {
    public:
        Document(RuntimeInterface * const _runtime, FigmaMemoryResource * _memory);

        void destroy() override;
        EResult loadUX(FigmaStringView _data) override;
        const FigmaString & getPath() const;
        const FigmaString & getFileName() const;
        const Rectf & getRenderCoordinates() const;
        const Vec2f & getThumbnailSize() const;
        const AssetVector & getAssets() const;
        const AssetDesc * findAsset(FigmaStringView _assetId) const override;
        const AssetDesc * getThumbnailAsset() const;
        const DocumentInspectionInterface * getInspection() const;
        const CanvasNodeDesc * findCanvasNode(FigmaStringView _nodeId) const override;
        const CanvasNodeDesc * getCanvasRoot() const override;
        const CanvasNodeDesc * getPrototypeStartFrame() const override;
        const BindingVector & getBindings() const;
        const ActionVector & getActions() const;
        const DiagnosticsInterface * getDiagnostics() const override;
        DiagnosticsInterface * getDiagnostics();
        char getCanvasVersion() const;
        bool hasCanvasBytes() const;
        FigmaMemoryResource * getMemory() const;
        const CanvasNodeDesc * findCanvasNodeDesc(FigmaStringView _nodeId) const;
        const CanvasNodeDesc * getCanvasRootDesc() const;
        const CanvasNodeDesc * getPrototypeStartFrameDesc() const;

    protected:
        friend EResult loadDocumentFromArchiveDataImpl(RuntimeInterface * const _runtime, const void * _data, std::size_t _size, const LoadOptions & _options, DocumentInterface ** const _document);
        friend bool decodeCanvas(RuntimeInterface * const _runtime, const FigmaByteBuffer & _bytes, Document * const _document, DiagnosticsInterface * const _diagnostics);

        RuntimeInterface * m_runtime;
        FigmaMemoryResource * m_memory;
        FigmaString m_path;
        FigmaString m_fileName;
        Rectf m_renderCoordinates;
        Vec2f m_thumbnailSize;
        CanvasNodeDesc m_canvasRoot;
        FigmaString m_prototypeStartNodeId;
        AssetVector m_assets;
        FigmaByteBuffer m_canvasBytes;
        BindingVector m_bindings;
        ActionVector m_actions;
        Diagnostics m_diagnostics;
        char m_canvasVersion = '\0';
        bool m_hasCanvasRoot = false;
    };
}

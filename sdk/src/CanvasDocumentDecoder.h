#pragma once

#include "DocumentModel.h"
#include "CanvasReader.h"

namespace Figma
{
    struct MatrixDesc
    {
        float m00 = 1.0f;
        float m01 = 0.0f;
        float m02 = 0.0f;
        float m10 = 0.0f;
        float m11 = 1.0f;
        float m12 = 0.0f;
    };

    struct ParentIndexDesc
    {
        explicit ParentIndexDesc( FigmaMemoryResource * _memory )
            : id( _memory )
            , position( _memory )
        {
        }

        FigmaString id;
        FigmaString position;
    };

    struct FontNameDesc
    {
        explicit FontNameDesc( FigmaMemoryResource * _memory )
            : family( _memory )
            , style( _memory )
            , postscript( _memory )
        {
        }

        FigmaString family;
        FigmaString style;
        FigmaString postscript;
    };

    struct NumberDesc
    {
        float value = 0.0f;
        bool percent = false;
    };

    struct TextBaselineDesc
    {
        Vec2f position;
        float width = 0.0f;
        float lineY = 0.0f;
        float lineHeight = 0.0f;
        float lineAscent = 0.0f;
        std::uint32_t firstCharacter = 0;
        std::uint32_t endCharacter = 0;
    };

    using CanvasNodeRecordIndexVector = FigmaVector<std::size_t>;
    using CanvasFloatVector = FigmaVector<float>;
    using TextBaselineVector = FigmaVector<TextBaselineDesc>;
    using CanvasBlobVector = FigmaVector<FigmaByteBuffer>;

    struct CanvasNodeRecord
    {
        explicit CanvasNodeRecord( FigmaMemoryResource * _memory )
            : node( _memory )
            , parentId( _memory )
            , position( _memory )
            , symbolId( _memory )
            , size{}
            , children( _memory )
        {
        }

        CanvasNodeDesc node;
        FigmaString parentId;
        FigmaString position;
        FigmaString symbolId;
        std::uint32_t styleId = 0;
        bool fillStyle = false;
        bool strokeStyle = false;
        MatrixDesc transform;
        Vec2f size;
        CanvasNodeRecordIndexVector children;
    };

    using CanvasNodeRecordVector = FigmaVector<CanvasNodeRecord>;
    using CanvasNodeRecordIndexMap = FigmaUnorderedMap<FigmaString, std::size_t>;

    class CanvasDocumentDecoder
        : public CanvasReader
    {
    public:
        CanvasDocumentDecoder( FigmaMemoryResource * _memory, const KiwiSchemaDesc & _schema );
        ~CanvasDocumentDecoder();

        bool decode( KiwiByteReader & _reader );
        CanvasNodeDesc takeCanvasRoot();
        FigmaString takePrototypeStartNodeId();

    protected:
        FigmaString decodeStyleId( KiwiByteReader & _reader );
        FigmaString decodeGuid( KiwiByteReader & _reader );
        Vec2f decodeVector( KiwiByteReader & _reader );
        MatrixDesc decodeMatrix( KiwiByteReader & _reader );
        Color decodeColor( KiwiByteReader & _reader );
        ParentIndexDesc decodeParentIndex( KiwiByteReader & _reader );
        FontNameDesc decodeFontName( KiwiByteReader & _reader );
        NumberDesc decodeNumber( KiwiByteReader & _reader );
        void decodeFloatArray( KiwiByteReader & _reader, CanvasFloatVector * const _values );
        void decodeBlobArray( KiwiByteReader & _reader );
        CanvasPathDesc decodePath( KiwiByteReader & _reader );
        void decodePathArray( KiwiByteReader & _reader, CanvasPathVector * const _paths );
        bool readPathPoint( const FigmaByteBuffer & _blob, std::size_t * const _offset, Vec2f * const _point ) const;
        void decodePathCommands( const FigmaByteBuffer & _blob, CanvasPathDesc * const _path ) const;
        void resolvePathCommands( CanvasPathDesc * const _path ) const;
        void resolveGeometryBlobs();
        void resolvePaintStyleReferences();
        void resolvePathStylePaints();
        const CanvasPaintVector * findPathStyleOverridePaints( const CanvasNodeDesc & _node, std::uint32_t _styleId, bool _fill ) const;
        void decodeFilterColorAdjust( KiwiByteReader & _reader, CanvasPaint * const _paint );
        void decodePaintFilter( KiwiByteReader & _reader, CanvasPaint * const _paint );
        CanvasArcDataDesc decodeArcData( KiwiByteReader & _reader );
        EPrototypeEventType prototypeEventTypeFromString( FigmaStringView _value ) const;
        EPrototypeConnectionType prototypeConnectionTypeFromString( FigmaStringView _value ) const;
        ECanvasBlendMode blendModeFromString( FigmaStringView _value ) const;
        EPrototypeNavigationType prototypeNavigationTypeFromString( FigmaStringView _value ) const;
        EPrototypeTransitionType prototypeTransitionTypeFromString( FigmaStringView _value ) const;
        EPrototypeTransitionDirection prototypeTransitionDirectionFromString( FigmaStringView _value ) const;
        EAnimationEasing animationEasingFromString( FigmaStringView _value ) const;
        void appendUnsupportedField( UnsupportedFieldVector * const _fields, FigmaStringView _name ) const;
        void decodePrototypeEvent( KiwiByteReader & _reader, PrototypeInteractionDesc * const _interaction );
        PrototypeActionDesc decodePrototypeAction( KiwiByteReader & _reader );
        PrototypeInteractionDesc decodePrototypeInteraction( KiwiByteReader & _reader );
        TextBaselineDesc decodeBaseline( KiwiByteReader & _reader );
        void decodeBaselineArray( KiwiByteReader & _reader, TextBaselineVector * const _baselines );
        void decodeTextData( KiwiByteReader & _reader, CanvasNodeDesc * const _node );
        void applyBaselinesToTextNode( const TextBaselineVector & _baselines, CanvasNodeDesc * const _node );
        void decodeDerivedTextData( KiwiByteReader & _reader, CanvasNodeDesc * const _node );
        FigmaString decodeImageHash( KiwiByteReader & _reader );
        FigmaString decodeSymbolData( KiwiByteReader & _reader );
        void decodeVectorData( KiwiByteReader & _reader, CanvasNodeDesc * const _node );
        CanvasPaint decodePaint( KiwiByteReader & _reader );
        void decodePaintArray( KiwiByteReader & _reader, CanvasPaintVector * const _paints );
        ECanvasNodeType nodeTypeFromString( FigmaStringView _type ) const;
        void decodeNodeChange( KiwiByteReader & _reader, CanvasNodeRecord * const _record );
        CanvasNodeDesc copyNodeRecursive( const CanvasNodeRecordVector & _records, std::size_t _index, const MatrixDesc & _parentTransform, bool _rootFrame );
        bool buildDocumentTree();
        void collectPrototypeStartFrame( const CanvasNodeDesc & _node );

    protected:
        CanvasNodeRecordVector m_records;
        CanvasNodeRecordIndexMap m_nodeIndex;
        CanvasNodeDesc m_canvasRoot;
        FigmaString m_prototypeStartNodeId;
        CanvasBlobVector m_blobs;
        std::uint32_t m_blobBaseIndex = 0;
    };
}

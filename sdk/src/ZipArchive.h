#pragma once

#include "Figma/DiagnosticsInterface.h"
#include "Figma/RuntimeInterface.h"

#include <cstddef>
#include <cstdint>

namespace Figma
{
    class ZipArchive
    {
    public:
        explicit ZipArchive( RuntimeInterface * const _runtime );
        ~ZipArchive();

        ZipArchive( const ZipArchive & _archive ) = delete;
        ZipArchive & operator=( const ZipArchive & _archive ) = delete;

        EResult open( const void * _data, std::size_t _size, DiagnosticsInterface * const _diagnostics );
        void close();

        std::size_t getFileCount() const;
        bool getFileName( std::size_t _index, FigmaString * const _name ) const;
        bool isDirectory( std::size_t _index ) const;
        bool extractFile( FigmaStringView _path, FigmaByteBuffer * const _bytes, DiagnosticsInterface * const _diagnostics );
        bool extractFileByIndex( std::size_t _index, FigmaByteBuffer * const _bytes, DiagnosticsInterface * const _diagnostics );

    protected:
        struct Entry
        {
            explicit Entry( FigmaMemoryResource * _memory = getDefaultMemoryResource() );

            FigmaString name;
            std::uint16_t method = 0;
            std::uint32_t crc32 = 0;
            std::uint64_t compressedSize = 0;
            std::uint64_t uncompressedSize = 0;
            std::uint64_t localHeaderOffset = 0;
            bool directory = false;
        };

        using EntryVector = FigmaVector<Entry>;

        bool parseCentralDirectory( DiagnosticsInterface * const _diagnostics );
        bool extractStored( const Entry & _entry, std::uint64_t _dataOffset, FigmaByteBuffer * const _bytes, DiagnosticsInterface * const _diagnostics ) const;
        bool extractDeflated( const Entry & _entry, std::uint64_t _dataOffset, FigmaByteBuffer * const _bytes, DiagnosticsInterface * const _diagnostics ) const;
        bool resolveDataOffset( const Entry & _entry, std::uint64_t * const _offset, DiagnosticsInterface * const _diagnostics ) const;
        static void * zlibAlloc( void * _opaque, unsigned int _items, unsigned int _size );
        static void zlibFree( void * _opaque, void * _address );

        FigmaMemoryResource * m_memory;
        const std::uint8_t * m_data;
        std::size_t m_size;
        EntryVector m_entries;
    };
}

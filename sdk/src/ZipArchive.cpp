#include "ZipArchive.h"

#include "DiagnosticsMacros.h"
#include "Memory.h"

#include <algorithm>
#include <cstring>
#include <limits>

#include <zlib.h>

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    namespace Detail
    {
        constexpr std::uint32_t END_OF_CENTRAL_DIRECTORY_SIGNATURE = 0x06054b50u;
        constexpr std::uint32_t CENTRAL_DIRECTORY_SIGNATURE = 0x02014b50u;
        constexpr std::uint32_t LOCAL_HEADER_SIGNATURE = 0x04034b50u;
        constexpr std::uint16_t METHOD_STORED = 0u;
        constexpr std::uint16_t METHOD_DEFLATED = 8u;
        //////////////////////////////////////////////////////////////////////////
        static std::uint16_t readU16( const std::uint8_t * _data )
        {
            return static_cast<std::uint16_t>( _data[0] ) | static_cast<std::uint16_t>( _data[1] << 8 );
        }
        //////////////////////////////////////////////////////////////////////////
        static std::uint32_t readU32( const std::uint8_t * _data )
        {
            return static_cast<std::uint32_t>( _data[0] ) | ( static_cast<std::uint32_t>( _data[1] ) << 8 ) | ( static_cast<std::uint32_t>( _data[2] ) << 16 ) |
                   ( static_cast<std::uint32_t>( _data[3] ) << 24 );
        }
        //////////////////////////////////////////////////////////////////////////
        static bool checkedRange( std::size_t _size, std::uint64_t _offset, std::uint64_t _length )
        {
            return _offset <= static_cast<std::uint64_t>( _size ) && _length <= static_cast<std::uint64_t>( _size ) - _offset;
        }

        static bool isDirectoryName( FigmaStringView _name )
        {
            return _name.empty() == false && _name.back() == '/';
        }
    }
    //////////////////////////////////////////////////////////////////////////
    ZipArchive::Entry::Entry( FigmaMemoryResource * _memory )
        : name( _memory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
    ZipArchive::ZipArchive( RuntimeInterface * const _runtime )
        : m_memory( _runtime != nullptr ? _runtime->getMemory() : getDefaultMemoryResource() )
        , m_data( nullptr )
        , m_size( 0 )
        , m_entries( m_memory )
    {
    }
    //////////////////////////////////////////////////////////////////////////
    ZipArchive::~ZipArchive()
    {
        this->close();
    }
    //////////////////////////////////////////////////////////////////////////
    EResult ZipArchive::open( const void * _data, std::size_t _size, DiagnosticsInterface * const _diagnostics )
    {
        if( _data == nullptr || _size == 0 )
        {
            return EResult::InvalidArgument;
        }

        this->close();
        m_data = static_cast<const std::uint8_t *>( _data );
        m_size = _size;

        if( this->parseCentralDirectory( _diagnostics ) == false )
        {
            return EResult::ParseFailed;
        }

        return EResult::Ok;
    }
    //////////////////////////////////////////////////////////////////////////
    void ZipArchive::close()
    {
        m_entries.clear();
        m_data = nullptr;
        m_size = 0;
    }
    //////////////////////////////////////////////////////////////////////////
    std::size_t ZipArchive::getFileCount() const
    {
        return m_entries.size();
    }
    //////////////////////////////////////////////////////////////////////////
    bool ZipArchive::getFileName( std::size_t _index, FigmaString * const _name ) const
    {
        if( _name == nullptr || _index >= m_entries.size() )
        {
            return false;
        }

        *_name = m_entries[_index].name;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool ZipArchive::isDirectory( std::size_t _index ) const
    {
        if( _index >= m_entries.size() )
        {
            return false;
        }

        return m_entries[_index].directory;
    }
    //////////////////////////////////////////////////////////////////////////
    bool ZipArchive::extractFile( FigmaStringView _path, FigmaByteBuffer * const _bytes, DiagnosticsInterface * const _diagnostics )
    {
        if( _bytes == nullptr )
        {
            return false;
        }

        const std::size_t entrySize = m_entries.size();
        for( std::size_t index = 0; index != entrySize; ++index )
        {
            if( m_entries[index].name == _path )
            {
                return this->extractFileByIndex( index, _bytes, _diagnostics );
            }
        }

        if( _diagnostics != nullptr )
        {
            FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_entry_missing", "Required ZIP entry is missing" );
        }

        return false;
    }
    //////////////////////////////////////////////////////////////////////////
    bool ZipArchive::extractFileByIndex( std::size_t _index, FigmaByteBuffer * const _bytes, DiagnosticsInterface * const _diagnostics )
    {
        if( _bytes == nullptr || _index >= m_entries.size() )
        {
            return false;
        }

        _bytes->clear();

        const Entry & entry = m_entries[_index];
        if( entry.directory == true )
        {
            _bytes->clear();
            return true;
        }

        if( entry.uncompressedSize > static_cast<std::uint64_t>( std::numeric_limits<std::size_t>::max() ) )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_entry_too_large", "ZIP entry is too large" );
            }

            return false;
        }

        std::uint64_t dataOffset = 0;
        if( this->resolveDataOffset( entry, &dataOffset, _diagnostics ) == false )
        {
            return false;
        }

        if( entry.method == Detail::METHOD_STORED )
        {
            return this->extractStored( entry, dataOffset, _bytes, _diagnostics );
        }

        if( entry.method == Detail::METHOD_DEFLATED )
        {
            return this->extractDeflated( entry, dataOffset, _bytes, _diagnostics );
        }

        if( _diagnostics != nullptr )
        {
            FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_method_unsupported", "ZIP entry compression method is unsupported" );
        }

        return false;
    }
    //////////////////////////////////////////////////////////////////////////
    bool ZipArchive::parseCentralDirectory( DiagnosticsInterface * const _diagnostics )
    {
        constexpr std::size_t EocdSize = 22;
        constexpr std::size_t MaxCommentSize = 0xffff;

        if( m_size < EocdSize )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_invalid", "File is too small to be a ZIP archive" );
            }

            return false;
        }

        const std::size_t searchBegin = m_size > EocdSize + MaxCommentSize ? m_size - EocdSize - MaxCommentSize : 0;
        std::size_t eocdOffset = std::numeric_limits<std::size_t>::max();
        for( std::size_t offset = m_size - EocdSize + 1; offset-- > searchBegin; )
        {
            if( Detail::readU32( m_data + offset ) != Detail::END_OF_CENTRAL_DIRECTORY_SIGNATURE )
            {
                continue;
            }

            const std::uint16_t commentSize = Detail::readU16( m_data + offset + 20 );
            if( offset + EocdSize + commentSize == m_size )
            {
                eocdOffset = offset;
                break;
            }
        }

        if( eocdOffset == std::numeric_limits<std::size_t>::max() )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_invalid", "ZIP end of central directory was not found" );
            }

            return false;
        }

        const std::uint16_t diskNumber = Detail::readU16( m_data + eocdOffset + 4 );
        const std::uint16_t centralDirectoryDisk = Detail::readU16( m_data + eocdOffset + 6 );
        const std::uint16_t diskEntryCount = Detail::readU16( m_data + eocdOffset + 8 );
        const std::uint16_t entryCount = Detail::readU16( m_data + eocdOffset + 10 );
        const std::uint32_t centralDirectorySize = Detail::readU32( m_data + eocdOffset + 12 );
        const std::uint32_t centralDirectoryOffset = Detail::readU32( m_data + eocdOffset + 16 );

        if( diskNumber != 0 || centralDirectoryDisk != 0 || diskEntryCount != entryCount )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_multidisk_unsupported", "Multidisk ZIP archives are unsupported" );
            }

            return false;
        }

        if( entryCount == std::numeric_limits<std::uint16_t>::max() || centralDirectorySize == std::numeric_limits<std::uint32_t>::max() ||
            centralDirectoryOffset == std::numeric_limits<std::uint32_t>::max() )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip64_unsupported", "ZIP64 archives are unsupported" );
            }

            return false;
        }

        if( Detail::checkedRange( m_size, centralDirectoryOffset, centralDirectorySize ) == false )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_invalid", "ZIP central directory points outside the file" );
            }

            return false;
        }

        std::uint64_t offset = centralDirectoryOffset;
        for( std::uint16_t index = 0; index != entryCount; ++index )
        {
            if( Detail::checkedRange( m_size, offset, 46 ) == false || Detail::readU32( m_data + offset ) != Detail::CENTRAL_DIRECTORY_SIGNATURE )
            {
                if( _diagnostics != nullptr )
                {
                    FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_invalid", "ZIP central directory entry is invalid" );
                }

                return false;
            }

            const std::uint16_t flags = Detail::readU16( m_data + offset + 8 );
            const std::uint16_t method = Detail::readU16( m_data + offset + 10 );
            const std::uint32_t crc = Detail::readU32( m_data + offset + 16 );
            const std::uint32_t compressedSize = Detail::readU32( m_data + offset + 20 );
            const std::uint32_t uncompressedSize = Detail::readU32( m_data + offset + 24 );
            const std::uint16_t nameSize = Detail::readU16( m_data + offset + 28 );
            const std::uint16_t extraSize = Detail::readU16( m_data + offset + 30 );
            const std::uint16_t commentSize = Detail::readU16( m_data + offset + 32 );
            const std::uint32_t localHeaderOffset = Detail::readU32( m_data + offset + 42 );
            const std::uint64_t entrySize = 46ull + nameSize + extraSize + commentSize;

            if( compressedSize == std::numeric_limits<std::uint32_t>::max() || uncompressedSize == std::numeric_limits<std::uint32_t>::max() ||
                localHeaderOffset == std::numeric_limits<std::uint32_t>::max() )
            {
                if( _diagnostics != nullptr )
                {
                    FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip64_unsupported", "ZIP64 entries are unsupported" );
                }

                return false;
            }

            if( ( flags & 0x1u ) != 0 )
            {
                if( _diagnostics != nullptr )
                {
                    FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_encrypted_unsupported", "Encrypted ZIP entries are unsupported" );
                }

                return false;
            }

            if( Detail::checkedRange( m_size, offset, entrySize ) == false )
            {
                if( _diagnostics != nullptr )
                {
                    FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_invalid", "ZIP central directory entry exceeds file bounds" );
                }

                return false;
            }

            Entry entry( m_memory );
            entry.name = FigmaString( reinterpret_cast<const char *>( m_data + offset + 46 ), nameSize, m_memory );
            entry.method = method;
            entry.crc32 = crc;
            entry.compressedSize = compressedSize;
            entry.uncompressedSize = uncompressedSize;
            entry.localHeaderOffset = localHeaderOffset;
            entry.directory = Detail::isDirectoryName( entry.name );
            m_entries.emplace_back( std::move( entry ) );

            offset += entrySize;
        }

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool ZipArchive::resolveDataOffset( const Entry & _entry, std::uint64_t * const _offset, DiagnosticsInterface * const _diagnostics ) const
    {
        if( _offset == nullptr )
        {
            return false;
        }

        if( Detail::checkedRange( m_size, _entry.localHeaderOffset, 30 ) == false || Detail::readU32( m_data + _entry.localHeaderOffset ) != Detail::LOCAL_HEADER_SIGNATURE )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_invalid", "ZIP local header is invalid" );
            }

            return false;
        }

        const std::uint16_t nameSize = Detail::readU16( m_data + _entry.localHeaderOffset + 26 );
        const std::uint16_t extraSize = Detail::readU16( m_data + _entry.localHeaderOffset + 28 );
        const std::uint64_t dataOffset = _entry.localHeaderOffset + 30ull + nameSize + extraSize;
        if( Detail::checkedRange( m_size, dataOffset, _entry.compressedSize ) == false )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_invalid", "ZIP entry data exceeds file bounds" );
            }

            return false;
        }

        *_offset = dataOffset;

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool ZipArchive::extractStored( const Entry & _entry, std::uint64_t _dataOffset, FigmaByteBuffer * const _bytes, DiagnosticsInterface * const _diagnostics ) const
    {
        _bytes->clear();

        if( _entry.compressedSize != _entry.uncompressedSize )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_invalid", "Stored ZIP entry has mismatched compressed and uncompressed sizes" );
            }

            _bytes->clear();

            return false;
        }

        const std::uint8_t * const begin = m_data + static_cast<std::size_t>( _dataOffset );
        _bytes->assign( begin, begin + static_cast<std::size_t>( _entry.compressedSize ) );

        const uLong crc = crc32( 0L, _bytes->data(), static_cast<uInt>( _bytes->size() ) );
        if( crc != _entry.crc32 )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_crc_failed", "Stored ZIP entry CRC check failed" );
            }

            _bytes->clear();

            return false;
        }

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    bool ZipArchive::extractDeflated( const Entry & _entry, std::uint64_t _dataOffset, FigmaByteBuffer * const _bytes, DiagnosticsInterface * const _diagnostics ) const
    {
        _bytes->clear();
        _bytes->resize( static_cast<std::size_t>( _entry.uncompressedSize ) );

        z_stream stream;
        std::memset( &stream, 0, sizeof( stream ) );
        stream.zalloc = &ZipArchive::zlibAlloc;
        stream.zfree = &ZipArchive::zlibFree;
        stream.opaque = m_memory;
        stream.next_in = const_cast<Bytef *>( reinterpret_cast<const Bytef *>( m_data + _dataOffset ) );
        stream.avail_in = static_cast<uInt>( _entry.compressedSize );
        Bytef emptyOutput = 0;
        stream.next_out = _bytes->empty() == false ? reinterpret_cast<Bytef *>( _bytes->data() ) : &emptyOutput;
        stream.avail_out = _bytes->empty() == false ? static_cast<uInt>( _bytes->size() ) : 1u;

        if( inflateInit2( &stream, -MAX_WBITS ) != Z_OK )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_inflate_failed", "Unable to initialize zlib inflate" );
            }

            _bytes->clear();

            return false;
        }

        const int result = inflate( &stream, Z_FINISH );
        inflateEnd( &stream );

        if( result != Z_STREAM_END || stream.total_out != _entry.uncompressedSize )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_inflate_failed", "Unable to inflate ZIP entry" );
            }

            _bytes->clear();

            return false;
        }

        const uLong crc = crc32( 0L, _bytes->data(), static_cast<uInt>( _bytes->size() ) );
        if( crc != _entry.crc32 )
        {
            if( _diagnostics != nullptr )
            {
                FIGMA_DIAGNOSTICS_ADD_POINTER( _diagnostics, EDiagnosticSeverity::Error, "fig_zip_crc_failed", "Deflated ZIP entry CRC check failed" );
            }

            _bytes->clear();

            return false;
        }

        return true;
    }
    //////////////////////////////////////////////////////////////////////////
    void * ZipArchive::zlibAlloc( void * _opaque, unsigned int _items, unsigned int _size )
    {
        FigmaMemoryResource * memory = static_cast<FigmaMemoryResource *>( _opaque );
        const std::size_t itemCount = _items;
        const std::size_t itemSize = _size;

        if( itemSize != 0 && itemCount > std::numeric_limits<std::size_t>::max() / itemSize )
        {
            return nullptr;
        }

        return Detail::allocateMemoryBlock( memory, itemCount * itemSize );
    }
    //////////////////////////////////////////////////////////////////////////
    void ZipArchive::zlibFree( void * _opaque, void * _address )
    {
        if( _address == nullptr )
        {
            return;
        }

        FigmaMemoryResource * memory = static_cast<FigmaMemoryResource *>( _opaque );
        Detail::deallocateMemoryBlock( memory, _address );
    }
    //////////////////////////////////////////////////////////////////////////
}

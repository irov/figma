#include "KiwiByteReader.h"

namespace Figma
{
    //////////////////////////////////////////////////////////////////////////
    KiwiByteReader::KiwiByteReader( const std::uint8_t * _data, std::size_t _size )
    {
        kiwi_reader_init( &m_reader, _data, _size );
    }
    //////////////////////////////////////////////////////////////////////////
    KiwiByteReader::~KiwiByteReader()
    {
    }
    //////////////////////////////////////////////////////////////////////////
    bool KiwiByteReader::eof() const
    {
        return kiwi_reader_eof( &m_reader ) != 0;
    }
    //////////////////////////////////////////////////////////////////////////
    std::uint8_t KiwiByteReader::readByte()
    {
        return kiwi_reader_read_u8( &m_reader );
    }
    //////////////////////////////////////////////////////////////////////////
    std::uint32_t KiwiByteReader::readVarUint()
    {
        return kiwi_reader_read_var_uint32( &m_reader );
    }
    //////////////////////////////////////////////////////////////////////////
    std::int32_t KiwiByteReader::readVarInt()
    {
        return kiwi_reader_read_var_int32( &m_reader );
    }
    //////////////////////////////////////////////////////////////////////////
    std::uint64_t KiwiByteReader::readVarUint64()
    {
        return kiwi_reader_read_var_uint64( &m_reader );
    }
    //////////////////////////////////////////////////////////////////////////
    std::int64_t KiwiByteReader::readVarInt64()
    {
        return kiwi_reader_read_var_int64( &m_reader );
    }
    //////////////////////////////////////////////////////////////////////////
    float KiwiByteReader::readVarFloat()
    {
        return kiwi_reader_read_var_float( &m_reader );
    }
    //////////////////////////////////////////////////////////////////////////
    FigmaString KiwiByteReader::readString( FigmaMemoryResource * _memory )
    {
        FigmaString result( _memory );

        for( ;; )
        {
            const std::uint32_t codepoint = this->readUtf8Codepoint();
            if( codepoint == 0 )
            {
                break;
            }

            this->appendUtf8( codepoint, &result );
        }

        return result;
    }
    //////////////////////////////////////////////////////////////////////////
    FigmaByteBuffer KiwiByteReader::readByteArray( FigmaMemoryResource * _memory )
    {
        std::uint32_t size = 0;
        const std::uint8_t * data = kiwi_reader_read_byte_array( &m_reader, &size );

        FigmaByteBuffer bytes( _memory );
        bytes.assign( data, data + size );
        return bytes;
    }
    //////////////////////////////////////////////////////////////////////////
    std::uint32_t KiwiByteReader::readUtf8Codepoint()
    {
        return kiwi_reader_read_utf8_codepoint( &m_reader );
    }
    //////////////////////////////////////////////////////////////////////////
    void KiwiByteReader::appendUtf8( std::uint32_t _codepoint, FigmaString * const _result )
    {
        if( _codepoint < 0x80 )
        {
            _result->push_back( static_cast<char>( _codepoint ) );
        }
        else if( _codepoint < 0x800 )
        {
            _result->push_back( static_cast<char>( 0xc0 | ( _codepoint >> 6 ) ) );
            _result->push_back( static_cast<char>( 0x80 | ( _codepoint & 0x3f ) ) );
        }
        else if( _codepoint < 0x10000 )
        {
            _result->push_back( static_cast<char>( 0xe0 | ( _codepoint >> 12 ) ) );
            _result->push_back( static_cast<char>( 0x80 | ( ( _codepoint >> 6 ) & 0x3f ) ) );
            _result->push_back( static_cast<char>( 0x80 | ( _codepoint & 0x3f ) ) );
        }
        else
        {
            _result->push_back( static_cast<char>( 0xf0 | ( _codepoint >> 18 ) ) );
            _result->push_back( static_cast<char>( 0x80 | ( ( _codepoint >> 12 ) & 0x3f ) ) );
            _result->push_back( static_cast<char>( 0x80 | ( ( _codepoint >> 6 ) & 0x3f ) ) );
            _result->push_back( static_cast<char>( 0x80 | ( _codepoint & 0x3f ) ) );
        }
    }
    //////////////////////////////////////////////////////////////////////////
}

#pragma once

#include <array>
#include <limits>
#include <span>
#include <string>
#include <vector>

// Conversions between binary-encoded and string-encoded data.

namespace Synthfi
{

const char HEX_MAP[ 16 ] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };

template< size_t EncodeBytes >
constexpr std::string enc_binary_hex( std::span< const uint8_t, EncodeBytes > data )
{
    std::string s( data.size( ) * 2, ' ' );
    for ( std::size_t i = 0; i < data.size( ); ++i )
    {
        s[ 2 * i ] = HEX_MAP[ ( data[ i ] & 0xF0 ) >> 4 ];
        s[ 2 * i + 1 ] = HEX_MAP[ data[ i ] & 0x0F ];
    }
    return s;
}

const char * const ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
const char ALPHABET_MAP[ 256 ] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1,  0,  1,  2,  3,  4,  5,  6,  7,  8, -1, -1, -1, -1, -1, -1,
    -1,  9, 10, 11, 12, 13, 14, 15, 16, -1, 17, 18, 19, 20, 21, -1,
    22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, -1, -1, -1, -1, -1,
    -1, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, -1, 44, 45, 46,
    47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

const double iFactor = 1.36565823730976103695740418120764243208481439700722980119458355862779176747360903943915516885072037696111192757109;

template< size_t SourceSize >
constexpr std::string enc_base58( std::span< const std::byte, SourceSize > source )
{
    static_assert( SourceSize > 0 );

    // Overallocate.
    std::array< char, SourceSize * 2 > result;

    int zeros = 0, length = 0, pbegin = 0, pend;
    if ( !( pend = ( int )source.size( ) ) ) return 0;
    while ( pbegin != pend && ( source[ pbegin ] == std::byte{ 0 } ) ) pbegin = ++zeros;
    int size = 1 + iFactor * ( double )( pend - pbegin );
    assert( size > 0 );
    uint8_t b58[ static_cast< unsigned >( size ) ];
    for ( int i = 0; i < size; i++ ) b58[ i ] = 0;
    while ( pbegin != pend )
    {
        uint32_t carry = static_cast< uint32_t >( source[pbegin] );
        int i = 0;
        for ( int it1 = size - 1; (carry || i < length) && (it1 != -1); it1--,i++ )
        {
            carry += 256U * b58[it1];
            b58[it1] = carry % 58;
            carry /= 58;
        }
        if ( carry )
        {
            return { };
        }
        length = i;
        pbegin++;
    }
    int it2 = size - length;
    while ((it2-size) && !b58[it2]) it2++;
    if ( ( zeros + size - it2 + 1) > ( int )result.size( ) )
    {
        return { };
    }

    int ri = 0;
    while(ri < zeros) result[ri++] = '1';
    for (; it2 < size; ++it2) result[ri++] = ALPHABET[b58[it2]];
    result[ri] = 0;

    return std::string( result.begin( ), result.begin( ) + ri );
}

// result must be declared (for the worst case): char result[len * 2];
template< size_t SourceSize >
static std::vector< std::byte > dec_base58( std::span< const char, SourceSize > text )
{
    assert( !text.empty( ) );

    // Overallocate.
    std::vector< std::byte > result( text.size( ) );

    result[ 0 ] = std::byte{ 0 };
    size_t resultSize = 1;
    for ( size_t i = 0; i < text.size( ); i++ )
    {
        uint32_t carry = static_cast< uint32_t >( ALPHABET_MAP[ static_cast< uint8_t >( text[ i ] ) ] );
        if ( carry == std::numeric_limits< uint32_t >::max( ) )
        {
            break;
        }
        for ( size_t j = 0; j < resultSize; j++)
        {
            carry += static_cast< uint32_t >( result[ j ] ) * 58;
            result[ j ] = static_cast< std::byte >( carry & 0xff );
            carry >>= 8;
        }
        while ( carry > 0 )
        {
            result[ resultSize++ ] = static_cast< std::byte >( carry & 0xff );
            carry >>= 8;
        }
    }

    for ( size_t i = 0; i < text.size( ) && text[ i ] == '1'; i++)
    {
        result[ resultSize++ ] = std::byte{ 0 };
    }

    // Poorly coded, but guaranteed to work.
    for ( int i = resultSize - 1, z = ( resultSize >> 1 ) + ( resultSize & 1 ); i >= z; i--)
    {
        int k = static_cast< int >( result[ i ] );
        result[ i ] = result[ resultSize - i - 1 ];
        result[ resultSize - i - 1 ] = static_cast< std::byte >( k );
    }

    result.resize( resultSize );
    return result;
}

//////////////////////////////////////////////////////////////////
// base64 encode/decode (with some formatting changes) courtesy of
// Adam Rudd per licence: github.com/adamvr/arduino-base64

static const char b64_alphabet[ ] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
  "abcdefghijklmnopqrstuvwxyz"
  "0123456789+/";

static void a3_to_a4( uint8_t * a4, uint8_t * a3 )
{
  a4[ 0 ] = ( a3[  0 ] & 0xfc ) >> 2;
  a4[ 1 ] = ( ( a3[ 0 ] & 0x03 ) << 4 ) + ( ( a3[ 1 ] & 0xf0 ) >> 4 );
  a4[ 2 ] = ( ( a3[ 1 ] & 0x0f ) << 2 ) + ( ( a3[ 2 ] & 0xc0 ) >> 6 );
  a4[ 3 ] = ( a3[ 2 ] & 0x3f );
}

static void a4_to_a3( uint8_t * a3, const uint8_t * a4 )
{
  a3[ 0 ] = ( a4[ 0 ] << 2 ) + ((a4[1] & 0x30 ) >> 4 );
  a3[ 1 ] = ( ( a4[ 1 ] & 0xf ) << 4) + ( ( a4[ 2 ] & 0x3c ) >> 2 );
  a3[ 2 ] = ( ( a4[ 2 ] & 0x3 ) << 6 ) + a4[ 3 ];
}

static char b64_lookup( char c )
{
  if ( c >='A' && c <='Z' ) return c - 'A';
  if ( c >='a' && c <='z' ) return c - 71;
  if ( c >='0' && c <='9' ) return c + 4;
  if ( c == '+' ) return 62;
  if ( c == '/' ) return 63;
  return -1;
}

inline constexpr size_t enc_base64_len( size_t size )
{
  return ( size + 2 - ( ( size + 2 ) % 3 ) ) / 3 * 4;
}

template< size_t Size >
inline std::string enc_base64( std::span< const std::byte, Size > source )
{
    int i = 0;
    int j = 0;
    size_t encSize = 0;
    uint8_t a3[ 3 ];
    uint8_t a4[ 4 ];

    // Overallocate.
    std::vector< char > result( source.size( ) * 2 );

    for ( std::byte c : source )
    {
        a3[ i++ ] = static_cast< uint8_t >( c );
        if ( i == 3 )
        {
            a3_to_a4( a4, a3 );
            for ( i = 0; i < 4; i++ )
            {
                result[ encSize++ ] = b64_alphabet[ a4[ i ] ];
            }
            i = 0;
        }
    }

    if ( i )
    {
        for ( j = i; j < 3; j++ )
        {
            a3[ j ] = '\0';
        }
        a3_to_a4( a4, a3 );
        for ( j = 0; j < i + 1; j++ )
        {
            result[ encSize++ ] = b64_alphabet[ a4[ j ] ];
        }

        while ( ( i++ < 3 ) )
        {
            result[ encSize++ ] = '=';
        }
    }

    return std::string( result.begin( ), result.begin( ) + encSize );
}

template< size_t SourceSize >
inline std::vector< std::byte > dec_base64( std::span< const char, SourceSize > text )
{
    int i = 0, j = 0;
    size_t decLen = 0;
    uint8_t a3[ 3 ] = { 0, 0, 0 };
    char a4[ 4 ];

    // Overallocate.
    std::vector< std::byte > result( text.size( ) );

    auto it = text.begin( );
    for ( size_t iterations = 0; iterations < text.size( ); ++iterations )
    {
        if ( *it == '=' )
        {
            break;
        }
        a4[ i++]  = *( it++ );
        if ( i == 4 )
        {
            for ( i = 0; i < 4; i++ )
            {
                a4[ i ] = b64_lookup( a4[ i ] );
            }
            a4_to_a3( a3, reinterpret_cast< uint8_t * >( a4 ) );
            for ( i = 0; i < 3; i++ )
            {
                result[ decLen++ ] = static_cast< std::byte >( a3[ i ] );
            }
            i = 0;
        }
    }

    if ( i )
    {
        for ( j = i; j < 4; j++ )
        {
            a4[ j ] = '\0';
        }
        for ( j = 0; j <4; j++ )
        {
            a4[ j ] = b64_lookup( a4[ j ] );
        }
        a4_to_a3( a3, reinterpret_cast< uint8_t * >( a4 ) );
        for ( j = 0; j < i - 1; j++ )
        {
            result[ decLen++ ] = static_cast< std::byte >( a3[ j ] );
        }
    }

    result.resize( decLen );
    return result;
}

} // namespace Synthfi

#include "synthfi/Core/KeyPair.hpp"

#include "synthfi/Core/SignatureEncode.hpp"
#include "synthfi/Util/StringEncode.hpp"
#include "synthfi/Util/Utils.hpp"

#include <boost/json/serialize.hpp>

namespace Synthfi
{
namespace Core
{


// Hash

void Hash::zero( )
{
    _hash.fill( std::byte{ 0 } );
}

bool Hash::is_zero( ) const
{
    return std::find_if( _hash.begin( ), _hash.end( ), [ ]( std::byte value ){ return value != std::byte{ 0x00 }; } ) == _hash.end( );
}

boost::json::array Hash::enc_binary_json( ) const
{
    boost::json::array jsonEncodedKeyPair;
    for ( std::byte datum : _hash )
    {
        jsonEncodedKeyPair.push_back( static_cast< uint8_t >( datum ) ) ;
    }
    return jsonEncodedKeyPair;
}

bool Hash::init_from_binary_json( const boost::json::array & json )
{
    if ( json.size( ) != Hash::size )
    {
        return false;
    }

    boost::system::error_code errorCode;
    for ( size_t i = 0; i < Hash::size; ++i )
    {
        uint8_t binaryDatum = json[ i ].to_number< uint8_t >( errorCode );
        if ( errorCode )
        {
            return false;
        }
        _hash[ i ] = static_cast< std::byte >( binaryDatum );
    }

    return true;
}

bool Hash::init_from_base58( std::string_view text )
{
    auto result = dec_base58( std::span( text ) );
    if ( result.size ( ) != Hash::size )
    {
        return false;
    }

    std::copy( result.begin( ), result.end( ), _hash.begin( ) );
    return true;
}

std::string Hash::enc_base58_text( ) const
{
    return enc_base58< Hash::size >( _hash );
}

bool Hash::init_from_base64( std::string_view text )
{
    auto result = dec_base64( std::span( text ) );
    if ( result.size ( ) != Hash::size )
    {
        return false;
    }

    std::copy( result.begin( ), result.end( ), _hash.begin( ) );
    return true;
}

std::string Hash::enc_base64_text( ) const
{
    return enc_base64( std::span( _hash ) );
}

std::ostream & operator <<( std::ostream & o, const Hash & binaryHash )
{
    return o << binaryHash.enc_base58_text( );
};

// PublicKey

bool PublicKey::create_program_address
(
    const PublicKey & baseKey,
    const PublicKey & programAddress,
    uint64_t nonce
)
{
    return create_program_address_hash( baseKey.data( ), programAddress.data( ), nonce, _hash );
}

// KeyPair

void KeyPair::gen( )
{
    generate_key_ed25519< PublicKey::size >( _hash, _privateKey );
}

void KeyPair::zero( )
{
    Hash::zero( );
    _privateKey.fill( std::byte{ 0 } );
}

boost::json::array KeyPair::enc_binary_json( ) const
{
    // Serialize private key, followed by public key.
    boost::json::array binaryEncodedJson;
    for ( auto datum : _privateKey )
    {
        binaryEncodedJson.push_back( static_cast< uint8_t >( datum ) ) ;
    }

    auto binaryEncodedPublicKey = PublicKey::enc_binary_json( );
    binaryEncodedJson.insert( binaryEncodedJson.end( ), binaryEncodedPublicKey.begin( ), binaryEncodedPublicKey.end( ) );
    return binaryEncodedJson;
}

bool KeyPair::init_from_binary_json( const boost::json::array & json )
{
    if ( json.size( ) != KeyPair::size )
    {
        return false;
    }

    // Deserialize private key, followed by public key.
    for ( size_t i = 0; i < Hash::size; ++i )
    {
        boost::system::error_code errorCode;
        uint8_t binaryDatum = json[ i ].to_number< uint8_t >( errorCode );
        if ( errorCode )
        {
            return false;
        }
        _privateKey[ i ] = static_cast< std::byte >( binaryDatum );
    }

    auto publicKeyArray = boost::json::array( json.begin( ) + Hash::size, json.end( ) );
    return PublicKey::init_from_binary_json( publicKeyArray );
}

std::ostream & operator <<( std::ostream & o, const KeyPair & keyPair )
{
    return o << PublicKey( keyPair );
};

// Signature

bool Signature::init_from_base58( std::string_view text )
{
    auto result = dec_base58( std::span( text ) );
    if ( result.size ( ) != Signature::size )
    {
        return false;
    }

    std::copy( result.begin( ), result.end( ), _signature.begin( ) );
    return true;
}

std::string Signature::enc_base58_text( ) const
{
    return enc_base58< Signature::size >( _signature );
}

std::ostream & operator <<( std::ostream & o, Signature signature )
{
    return o << signature.enc_base58_text( );
};

} // namespace Core
} // namespace Synthfi

#pragma once

#include <boost/assert.hpp>

#include <openssl/evp.h>
#include <openssl/hmac.h>

#include <array>
#include <functional>
#include <memory>
#include <span>
#include <string>

namespace Synthfi
{

namespace Core
{

template< size_t KeySize >
bool generate_key_ed25519
(
    std::array< std::byte, KeySize > & publicKeyData,
    std::array< std::byte, KeySize > & privateKeyData
)
{
    EVP_PKEY * pKeyRef = NULL;
    {
        std::unique_ptr< EVP_PKEY_CTX, std::function< void( EVP_PKEY_CTX * ) > > pctx
        (
            EVP_PKEY_CTX_new_id( EVP_PKEY_ED25519, NULL ),
            [ ]( EVP_PKEY_CTX * context ) { EVP_PKEY_CTX_free( context ); }
        );

        EVP_PKEY_keygen_init( pctx.get( ) );
        EVP_PKEY_keygen( pctx.get( ), &pKeyRef );
    }

    std::unique_ptr< EVP_PKEY, std::function< void( EVP_PKEY * ) > > pkey
    (
        pKeyRef,
        [ ]( EVP_PKEY * evpKey ) { EVP_PKEY_free( evpKey ); }
    );

    size_t keySize = KeySize;
    EVP_PKEY_get_raw_private_key( pkey.get( ), reinterpret_cast< unsigned char * >( privateKeyData.data( ) ), &keySize );
    EVP_PKEY_get_raw_public_key( pkey.get( ), reinterpret_cast< unsigned char * >( publicKeyData.data( ) ), &keySize );

    return true;
}

template< size_t MessageSize, size_t KeySize, size_t SignatureSize >
bool sign_message_ed25519
(
    std::span< const std::byte, MessageSize > message,
    const std::array< std::byte, KeySize > & key,
    std::array< std::byte, SignatureSize > & signature
)
{
    std::unique_ptr< EVP_PKEY, std::function< void( EVP_PKEY * ) > > pkey
    (
        EVP_PKEY_new_raw_private_key( EVP_PKEY_ED25519, NULL, reinterpret_cast< const unsigned char * >( key.data( ) ), key.size( ) ),
        [ ]( EVP_PKEY * evpKey ) { EVP_PKEY_free( evpKey ); }
    );

    if ( !pkey )
    {
        return false;
    }

    std::unique_ptr< EVP_MD_CTX, std::function< void( EVP_MD_CTX * ) > > mctx
    (
        EVP_MD_CTX_new( ),
        [ ]( EVP_MD_CTX * context ) { EVP_MD_CTX_free( context ); }
    );

    int rc = EVP_DigestSignInit( mctx.get( ), NULL, NULL, NULL, pkey.get( ) );
    if ( rc )
    {
        size_t signatureSize = signature.size( );
        rc = EVP_DigestSign
        (
            mctx.get( ),
            reinterpret_cast< unsigned char * >( signature.data( ) ),
            &signatureSize,
            reinterpret_cast< const unsigned char * >( message.data( ) ),
            message.size( )
        );
    }

    return rc != 0;
}

template< size_t SignatureSize >
bool sign_message_hmac
(
    std::string_view message,
    std::string_view key,
    std::array< std::byte, SignatureSize > & signature
)
{
    std::unique_ptr< HMAC_CTX, std::function< void( HMAC_CTX * ) > > hmacContext
    (
        HMAC_CTX_new( ), [ ]( HMAC_CTX * context ){ HMAC_CTX_free( context ); }
    );

    if ( !hmacContext )
    {
        return false;
    }

    HMAC_Init_ex
    (
        hmacContext.get( ),
        key.data( ),
        static_cast< int >( key.size( ) ),
        EVP_sha256( ),
        nullptr
    );
    int rc = HMAC_Update( hmacContext.get( ), reinterpret_cast< const unsigned char * >( message.data( ) ), message.size( ) );
    if ( rc )
    {
        HMAC_Final( hmacContext.get( ), reinterpret_cast< unsigned char * >( signature.data( ) ), nullptr );
    }

    return rc != 0;
}

template< size_t MessageSize, size_t KeySize, size_t SignatureSize >
bool verify_message
(
    std::span< const std::byte, MessageSize > message,
    const std::array< std::byte, KeySize > & publicKey,
    std::array< std::byte, SignatureSize > & signature
)
{
    std::unique_ptr< EVP_PKEY, std::function< void( EVP_PKEY * ) > > pkey
    (
        EVP_PKEY_new_raw_private_key( EVP_PKEY_ED25519, NULL, publicKey.data( ).data( ), publicKey.data( ).size( ) ),
        [ ]( EVP_PKEY * evpKey ) { EVP_PKEY_free( evpKey ); }
    );

    if ( !pkey )
    {
        return false;
    }

    std::unique_ptr< EVP_MD_CTX, std::function< void( EVP_MD_CTX * ) > > mctx
    (
        EVP_MD_CTX_new( ),
        [ ]( EVP_MD_CTX * context ) { EVP_MD_CTX_free( context ); }
    );

    int rc = EVP_DigestSignInit( mctx.get( ), NULL, NULL, NULL, pkey.get( ) );
    if ( rc )
    {
        size_t signatureSize = signature.size( );
        rc = EVP_DigestVerify( mctx.get( ), signature.data( ), signature.size( ), message.data( ), message.size( ) );
    }

    return rc != 0;
}

template< size_t KeySize >
bool create_program_address_hash
(
    const std::array< std::byte, KeySize > & baseKey,
    const std::array< std::byte, KeySize > & programAddress,
    uint64_t nonce,
    std::array< std::byte, KeySize > & result
)
{
    constexpr std::array< char, 21 > postfix = { 'P','r','o','g','r','a','m','D','e','r','i','v','e','d','A','d','d','r','e','s','s' };

    constexpr size_t bufferSize = sizeof( nonce ) + sizeof( postfix ) + KeySize * 2;
    std::array< std::byte, bufferSize > buffer;

    auto it = std::copy( baseKey.begin( ), baseKey.end( ), buffer.begin( ) );
    it = std::copy( reinterpret_cast< const std::byte * >( &nonce ), reinterpret_cast< const std::byte * >( &nonce ) + sizeof( uint64_t ), it );
    it = std::copy( programAddress.begin( ), programAddress.end( ), it );
    it = std::copy( reinterpret_cast< const std::byte * >( postfix.begin( ) ), reinterpret_cast< const std::byte * >( postfix.end( ) ), it );

    std::unique_ptr< EVP_MD_CTX, std::function< void( EVP_MD_CTX * ) > > mctx
    (
        EVP_MD_CTX_new( ),
        [ ]( EVP_MD_CTX * context ) { EVP_MD_CTX_free( context ); }
    );

    if ( !EVP_DigestInit_ex( mctx.get( ), EVP_sha256( ), NULL ) )
    {
        return false;
    }

    if ( !EVP_DigestUpdate( mctx.get( ), buffer.data( ), buffer.size( ) )) 
    {
        return false;
    }

    std::array< std::byte, bufferSize * 2 > resultBuffer;
    if ( !EVP_DigestFinal( mctx.get( ), reinterpret_cast< uint8_t * >( resultBuffer.data( ) ), NULL ) )
    {
        return false;
    }

    // Copy public key portion.
    std::copy( resultBuffer.begin( ), resultBuffer.begin( ) + KeySize, result.begin( ) );
    return true;
}

} // namespace Core
} // namespace Synthfi

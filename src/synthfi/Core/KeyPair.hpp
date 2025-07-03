#pragma once

#include "synthfi/Core/SignatureEncode.hpp"
#include "synthfi/Util/StringEncode.hpp"

#include <boost/functional/hash.hpp>
#include <boost/json/array.hpp>

#include <array>
#include <span>

#include <fmt/format.h>

namespace Synthfi
{
namespace Core
{

// 32-byte hash.
class Hash
{
public:
    static constexpr size_t size = 32;

    Hash( ) = default;
    Hash( const Hash & ) = default;

    bool operator==( const Hash & ) const = default;
    bool operator!=( const Hash & ) const = default;

    void zero( );
    bool is_zero( ) const;

    boost::json::array enc_binary_json( ) const;
    bool init_from_binary_json( const boost::json::array & json );

    std::string enc_base58_text( ) const;
    bool init_from_base58( std::string_view text );

    std::string enc_base64_text( ) const;
    bool init_from_base64( std::string_view text );

    std::array< std::byte, size > & data( ) & { return _hash; }
    const std::array< std::byte, size > & data( ) const & { return _hash; }

    friend std::ostream & operator <<( std::ostream & o, const Hash & binaryHash );

protected:
    std::array< std::byte, size > _hash;
};
static_assert( sizeof( Hash ) == 32, "Invalid Hash size" );

// Public key.
class PublicKey : public Hash
{
public:
    bool create_program_address
    (
        const PublicKey & baseKey,
        const PublicKey & programAddress,
        uint64_t nonce
    );
};
static_assert( sizeof( PublicKey ) == 32, "Invalid PublicKey size" );

using PublicKeyRef = const PublicKey *;

static PublicKey create_program_address
(
    const PublicKey & baseKey,
    const PublicKey & programAddress,
    uint64_t nonce
);

// Public/private key pair.
class KeyPair : public PublicKey
{
public:
    KeyPair( ) = default;
    KeyPair( const KeyPair & ) = default;

    static constexpr size_t size = Hash::size + PublicKey::size;

    void zero( );

    // Generate new keypair.
    void gen( );

    boost::json::array enc_binary_json( ) const;
    bool init_from_binary_json( const boost::json::array & json );

    const std::array< std::byte, Hash::size > & data( ) const { return _privateKey; }

    friend std::ostream & operator <<( std::ostream & o, const KeyPair & keyPair );

private:
    std::array< std::byte, Hash::size > _privateKey;
};
static_assert( sizeof( KeyPair ) == 64, "Invalid KeyPair size" );

using KeyPairRef = const KeyPair *;



// Digital signature.
class Signature
{
public:
    static constexpr size_t size = 64;

    bool init_from_base58( std::string_view text );
    std::string enc_base58_text( ) const;

    template< size_t EncodeBytes > requires ( EncodeBytes <= size )
    std::string enc_hex( ) const
    {
        return enc_binary_hex
        (
            std::span< const uint8_t, EncodeBytes >
            (
                reinterpret_cast< const uint8_t * >( _signature.data( ) ), EncodeBytes
            )
        );
    }

    // sign message given key_pair.
    template< size_t MessageSize >
    bool sign_ed25519( std::span< const std::byte, MessageSize > message, KeyPairRef keyPair )
    {
        return sign_message_ed25519< MessageSize, Hash::size, Signature::size >( message, keyPair->data( ), _signature );
    }

    bool sign_hmac( std::string_view message, std::string_view key )
    {
        return Synthfi::Core::sign_message_hmac< Signature::size >( message, key, _signature );
    }

    // Verify message given public key.
    template< size_t MessageSize >
    bool verify( std::span< const std::byte, MessageSize > message, PublicKeyRef publicKey )
    {
        return verify_message< MessageSize, Hash::size, Signature::size >( message, publicKey->data( ), _signature );
    }

    bool verify( const std::vector< std::byte > & message, const PublicKey & );

    const std::array< std::byte, size > & data( ) const { return _signature;; }

    friend std::ostream & operator <<( std::ostream & o, Signature signature );

private:
    std::array< std::byte, size > _signature;
};

inline std::size_t hash_value( Synthfi::Core::PublicKey const & key )
{
    return boost::hash_value( key.data( ) );
}

} // namespace Core
} // namespace Synthfi

template< >
struct std::hash< Synthfi::Core::Hash >
{
    std::size_t operator( )( const Synthfi::Core::Hash & hash ) const noexcept
    {
        return boost::hash_value( hash.data( ) ) ; // or use boost::hash_combine
    }
};

template< >
struct std::hash< Synthfi::Core::PublicKey >
{
    std::size_t operator( )( const Synthfi::Core::PublicKey & key ) const noexcept
    {
        return boost::hash_value( key.data( ) ) ; // or use boost::hash_combine
    }
};

namespace fmt
{

template < >
struct formatter< Synthfi::Core::Hash > : fmt::formatter< std::string >
{
    // parse is inherited from formatter<string_view>.
    template < class FormatContext >
    auto format( Synthfi::Core::Hash hash, FormatContext & ctx)
    {
        return fmt::formatter< std::string >::format( hash.enc_base58_text( ), ctx );
    }
};

template < >
struct formatter< Synthfi::Core::PublicKey > : fmt::formatter< std::string >
{
    // parse is inherited from formatter<string_view>.
    template < class FormatContext >
    auto format( Synthfi::Core::PublicKey hash, FormatContext & ctx)
    {
        return fmt::formatter< std::string >::format( hash.enc_base58_text( ), ctx );
    }
};

} // namespace fmt

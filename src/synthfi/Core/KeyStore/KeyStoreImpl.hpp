#pragma once

#include "synthfi/Core/KeyPair.hpp"

#include "synthfi/Util/Logger.hpp"
#include "synthfi/Util/StringHash.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include <filesystem>
#include <unordered_map>

namespace Synthfi
{
namespace Core
{

class KeyStoreImpl
{
public:
    KeyStoreImpl( boost::asio::io_context & ioContext, const std::filesystem::path & keyStorePath );

    // Create/chmod key_store directory.
    template< class CompletionHandlerType >
    void create_key_store_directory( CompletionHandlerType && completionHandler )
    {
        boost::asio::co_spawn( _strand, do_create_key_store_directory( ), completionHandler );
    }

    // Verfiy key store directory exists and permissions are correct.
    template< class CompletionHandlerType >
    void verify_key_store_directory( CompletionHandlerType && completionHandler )
    {
        boost::asio::co_spawn( _strand, do_verify_key_store_directory( ), completionHandler );
    }

    // Create and write new key pair to file, returns the tag of the new key.
    template< class CompletionHandlerType >
    void create_key_pair( CompletionHandlerType && completionHandler )
    {
        boost::asio::co_spawn( _strand, do_create_key_pair( ), completionHandler );
    }

    // Create and write new key pair to file.
    template< class CompletionHandlerType >
    void create_key_pair( std::string tag, CompletionHandlerType && completionHandler )
    {
        boost::asio::co_spawn( _strand, do_create_key_pair_tag( std::move( tag ) ), completionHandler );
    }

    // Load key pair from file.
    template< class CompletionHandlerType >
    void load_key_pair( std::string tag, CompletionHandlerType && completionHandler )
    {
        boost::asio::co_spawn( _strand, do_load_key_pair( std::move( tag ) ), completionHandler );
    }

    // Get public key from key store.
    template< class CompletionHandlerType >
    void get_public_key( std::string tag, CompletionHandlerType && completionHandler )
    {
        boost::asio::co_spawn( _strand, do_get_public_key( std::move( tag ) ), completionHandler );
    }

    // Get key pair from key store.
    template< class CompletionHandlerType >
    void get_key_pair( std::string tag, CompletionHandlerType && completionHandler )
    {
        boost::asio::co_spawn( _strand, do_get_key_pair( std::move( tag ) ), completionHandler );
    }

private:
    std::filesystem::path get_key_pair_path( const std::string_view tag ) const;

    boost::asio::awaitable< void > do_create_key_store_directory( );
    boost::asio::awaitable< void > do_verify_key_store_directory( );

    boost::asio::awaitable< void > do_write_key_pair( const KeyPair & keyPair, std::filesystem::path keyFile );
    boost::asio::awaitable< void > do_load_key_pair( std::string tag );

    boost::asio::awaitable< void > do_create_key_pair_tag( std::string tag );
    boost::asio::awaitable< std::string > do_create_key_pair( );


    boost::asio::awaitable< PublicKeyRef > do_get_public_key( std::string tag );
    boost::asio::awaitable< KeyPairRef > do_get_key_pair( std::string tag );

    static PublicKey base58_to_public_key( std::string_view text );

    static std::string get_sysvar_rent( )
    {
        return "SysvarRent111111111111111111111111111111111";
    }

    static std::string get_spl_token_program( )
    {
        return "TokenkegQfeZyiNwAJbNbGKPFXCWuBvf9Ss623VQ5DA";
    }

    static std::string get_usdt_mainnet( )
    {
        return "Es9vMFrzaCERmJfrF4H2FYD4KCoNkY11McCe8BenwNYB";
    }

    static std::string get_pyth_usdt( )
    {
        return "C5wDxND9E61RZ1wZhaSTWkoA8udumaHnoQY6BBsiaVpn";
    }

    std::filesystem::path _keyStorePath;

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;

    TransparentStringMap< PublicKey > _publicKeys;
    TransparentStringMap< KeyPair > _keyPairs;

    std::unordered_map< std::string, PublicKey > _pythProductPublicKeys =
    {
        { "AAPL" , base58_to_public_key( "G89jkM5wFLpmnbvRbeePUumxsJyzoXaRfgBVjyx2CPzQ" ) },
        { "AMZN" , base58_to_public_key( "J6zuHzycf8XLd85QHDUwMtVPxJGPJptPSC9dyioKXCnb" ) },
        { "AMC" , base58_to_public_key(  "6U4PrvMwfMcBkG7Zrc4oxYqJwrfTMWmgA9hS6fjDJkmo" ) },
        { "GME" , base58_to_public_key(  "7MudLeJnT2GCPZ66oeAqd6jenF9fGARrB1pLo5nBT3KM" ) },
        { "GOOG" , base58_to_public_key( "CpPmHbFqkfejPcF8cvxyDogm32Sqo3YGMFBgv3kR1UtG" ) },
        { "NFLX" , base58_to_public_key( "4nyATHv6KnZY5fVTqQLq9DkcstfqGYd834Jmbch2bf3i" ) },
        { "TSLA" , base58_to_public_key( "GaBJpKtnyUbyKe34XuyegR7W98a9PT5cg985G974NY8R" ) },
        { "SPY" , base58_to_public_key(  "CpPmHbFqkfejPcF8cvxyDogm32Sqo3YGMFBgv3kR1UtG" ) }
    };

    SynthfiLogger _logger;
};

} // namespace Core
} // namespace Synthfi

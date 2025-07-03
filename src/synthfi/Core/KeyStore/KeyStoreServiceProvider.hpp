#pragma once

#include "synthfi/Core/KeyPair.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <filesystem>

namespace Synthfi
{
namespace Core
{

template< typename Service >
class KeyStoreServiceProvider
{
public:
    KeyStoreServiceProvider( Service & service )
        : _service( &service )
    { }

    KeyStoreServiceProvider( boost::asio::io_context & ioContext, const std::filesystem::path & keyStorePath )
        : _service( &boost::asio::make_service< Service >( ioContext, keyStorePath ) )
    { }

    // Create/chmod key_store directory, fails if directory exists.
    template< boost::asio::completion_token_for< void( std::exception_ptr ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto create_key_store_directory( CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr ) >
        (
            [ this ]( auto completionHandler )
            {
                _service->key_store_container( ).create_key_store_directory( completionHandler );
            },
            token
        );
    }

    // Verify key_store directory exists and permissions are correct.
    template< boost::asio::completion_token_for< void( std::exception_ptr ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto verify_key_store_directory( CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr ) >
        (
            [ this ]( auto completionHandler )
            {
                _service->key_store_container( ).verify_key_store_directory( completionHandler );
            },
            token
        );
    }

    // Create and write new key pair to file, returns the tag of the new key.
    template< boost::asio::completion_token_for< void( std::exception_ptr, std::string ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto create_key_pair( CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, std::string ) >
        (
            [ this ]( auto completionHandler )
            {
                _service->key_store_container( ).create_key_pair( completionHandler );
            },
            token
        );
    }

    // Create and write new key pair to file.
    template< boost::asio::completion_token_for< void( std::exception_ptr ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto create_key_pair( std::string tag, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr ) >
        (
            [ this, tag = std::move( tag ) ]( auto completionHandler )
            {
                _service->key_store_container( ).create_key_pair( std::move( tag ), completionHandler );
            },
            token
        );
    }

    // Load key pair from file.
    template< boost::asio::completion_token_for< void( std::exception_ptr ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto load_key_pair( std::string tag, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr ) >
        (
            [ this, tag = std::move( tag ) ]< class Handler >( Handler && self )
            {
                _service->key_store_container( ).load_key_pair
                (
                    std::move( tag ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex )
                    {
                        ( *self )( ex );
                    }
                );
            },
            token
        );
    }

    // Get public key from key store.
    template< boost::asio::completion_token_for< void( std::exception_ptr, PublicKeyRef ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto get_public_key( std::string tag, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, PublicKeyRef ) >
        (
            [ this, tag = std::move( tag ) ]< class Handler >( Handler && self )
            {
                _service->key_store_container( ).get_public_key
                (
                    std::move( tag ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex, PublicKeyRef publicKey )
                    {
                        ( *self )( ex, publicKey );
                    }
                );
            },
            token
        );
    }

    // Get key pair from key store.
    template< boost::asio::completion_token_for< void( std::exception_ptr, KeyPairRef ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto get_key_pair( std::string tag, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, KeyPairRef ) >
        (
            [ this, tag = std::move( tag ) ]< class Handler >( Handler && self )
            {
                _service->key_store_container( ).get_key_pair
                (
                     std::move( tag ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex, KeyPairRef keyPair )
                    {
                        ( *self )( ex, keyPair );
                    }
                );
            },
            token
        );
    }

private:
    Service * _service;
};

} // namespace Core
} // namespace Synthfi

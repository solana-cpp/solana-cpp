#pragma once

#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClient.hpp"
#include "synthfi/Solana/SolanaHttpMessage.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

namespace Synthfi
{
namespace Solana
{

template< class Service >
class AccountBatcherServiceProvider
{
public:
    explicit AccountBatcherServiceProvider( Service & service )
        : _service( &service )
    { }

    AccountBatcherServiceProvider
    (
        boost::asio::io_context & ioContext,
        SolanaHttpClientService & solanaHttpClientService,
        size_t multipleAccountsBatchSize
    )
        : _service( &boost::asio::make_service< Service >( ioContext, solanaHttpClientService, multipleAccountsBatchSize ) )
    { }

    boost::asio::strand< boost::asio::io_context::executor_type > & strand( ) const &
    {
        return _service->strand( );
    }

    // Manages GetMultipleAccountsRequest by batching request into smaller GetMultipleAccountsRequests.
    template< boost::asio::completion_token_for< void( std::exception_ptr, GetAccountInfoResponse ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto get_account( GetAccountInfoRequest request, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, GetAccountInfoResponse ) >
        (
            [ this, request = std::move( request ) ]< class Handler >( Handler && self )
            {
                _service->subscriber( ).get_account
                (
                    std::move( request ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex, GetAccountInfoResponse response )
                    {
                        ( *self )( ex, std::move( response ) );
                    }
                );
            },
            token
        );
    }

    // Manages GetMultipleAccountsRequest by batching request into smaller GetMultipleAccountsRequests.
    template< boost::asio::completion_token_for< void( std::exception_ptr, GetMultipleAccountsResponse ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto get_multiple_accounts( GetMultipleAccountsRequest request, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, GetMultipleAccountsResponse ) >
        (
            [ this, request = std::move( request ) ]< class Handler >( Handler && self )
            {
                _service->subscriber( ).get_multiple_accounts
                (
                    std::move( request ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex, GetMultipleAccountsResponse response )
                    {
                        ( *self )( ex, std::move( response ) );
                    }
                );
            },
            token
        );
    }

private:
    Service * _service;
};

} // namespace Solana
} // namespace Synthfi

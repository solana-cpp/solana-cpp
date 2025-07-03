#pragma once

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Solana/SolanaWsClient/SolanaWsClient.hpp"
#include "synthfi/Solana/SolanaWsMessage.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

namespace Synthfi
{
namespace Solana
{

template< class Service >
class AccountSubscriberServiceProvider
{
public:
    explicit AccountSubscriberServiceProvider( Service & service )
        : _service( &service )
    { }

    AccountSubscriberServiceProvider
    (
        boost::asio::io_context & ioContext,
        SolanaWsClientService & solanaWsClientService
    )
        : _service( &boost::asio::make_service< Service >( ioContext, solanaWsClientService ) )
    { }

    template< boost::asio::completion_token_for< void( std::exception_ptr, SubscribeResult ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto account_subscribe
    (
        AccountSubscribe request,
        std::function< void( uint64_t, AccountNotification ) > onAccountNotification,
        CompletionToken && token = { }
    )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, SubscribeResult ) >
        (
            [ this, request = std::move( request ), onAccountNotification = std::move( onAccountNotification ) ]< class Handler >( Handler && self )
            {
                _service->subscriber( ).account_subscribe
                (
                    std::move( request ),
                    std::move( onAccountNotification ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex, SubscribeResult result )
                    {
                        ( *self )( ex, result );
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

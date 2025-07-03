#pragma once

#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherService.hpp"
#include "synthfi/Solana/SolanaTypes.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace Synthfi
{
namespace Solana
{

template< class Service >
class SolanaWsClientServiceProvider
{
public:
    explicit SolanaWsClientServiceProvider( Service & service )
        : _service( &service )
    { }

    SolanaWsClientServiceProvider
    (
        boost::asio::io_context & ioContext,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        const SolanaEndpointConfig & config
   )
        : _service
        (
            &boost::asio::make_service< Service >
            (
                ioContext,
                statisticsPublisherService,
                config
            )
        )
    { }

    template
    <
        class SubscribeType,
        class SubscribeResultType,
        class NotificationType,
        boost::asio::completion_token_for< void( std::exception_ptr, SubscribeResultType ) > CompletionToken = boost::asio::use_awaitable_t< >
    >
    auto send_subscribe( SubscribeType request, std::function< void( uint64_t, NotificationType ) > onNotificationFn, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, SubscribeResultType ) >
        (
            [ this, request = std::move( request ), onNotificationFn = std::move( onNotificationFn ) ]< class Handler >( Handler && self )
            {
                _service->impl( ).template send_subscribe< SubscribeType, SubscribeResultType, NotificationType >
                (
                    std::move( request ),
                    std::move( onNotificationFn ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex, SubscribeResultType response )
                    {
                        (* self )( ex, std::move( response ) );
                    }
                );
            },
            token
        );
    }

    template
    <
        class UnsubscribeType,
        class UnsubscribeResultType,
        boost::asio::completion_token_for< void( std::exception_ptr, UnsubscribeType ) > CompletionToken = boost::asio::use_awaitable_t< >
    >
    auto send_unsubscribe( UnsubscribeType request, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, UnsubscribeResultType ) >
        (
            [ this, request = std::move( request )  ]< class Handler >( Handler && self )
            {
                _service->impl( ).template send_unsubscribe< UnsubscribeType, UnsubscribeResultType >
                (
                    std::move( request ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex, UnsubscribeResultType response )
                    {
                        (* self )( ex, std::move( response ) );
                    }
                );
            },
            token
        );
    }

    template< boost::asio::completion_token_for< void( std::exception_ptr, bool ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto cancel_subscription( uint64_t subscriptionId, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, bool ) >
        (
            [ this, subscriptionId ]< class Handler >( Handler && self )
            {
                _service->impl( ).cancel_subscription
                (
                    subscriptionId,
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex, bool response )
                    {
                        ( *self )( ex, response );
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

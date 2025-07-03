#pragma once


#include "synthfi/Serum/SerumReferenceDataClient/SerumReferenceDataClientService.hpp"
#include "synthfi/Solana/AccountBatcher/AccountBatcherService.hpp"
#include "synthfi/Solana/AccountSubscriber/AccountSubscriberService.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherService.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

#include <boost/json/value.hpp>

namespace Synthfi
{
namespace Serum
{

template< class Service >
class SerumMarketDataClientServiceProvider
{
public:
    explicit SerumMarketDataClientServiceProvider( Service & service )
        : _service( &service )
    { }

    SerumMarketDataClientServiceProvider
    (
        boost::asio::io_context & ioContext,
        Solana::AccountBatcherService & solanaAccountBatcher,
        Solana::AccountSubscriberService & solanaAccountSubscriber,
        Serum::SerumReferenceDataClientService & serumReferenceDataClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService
    )
        : _service
        (
            &boost::asio::make_service< Service >
            (
                ioContext,
                solanaAccountBatcher,
                solanaAccountSubscriber,
                serumReferenceDataClientService,
                statisticsPublisherService
            )
        )
    { }

    template< boost::asio::completion_token_for< void( std::exception_ptr ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto orderbook_subscribe
    (
        std::function< void( uint64_t, Trading::Book ) > onBook,
        CompletionToken && token = { }
    )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr ) >
        (
            [ this, onBook = std::move( onBook ) ]< class Handler >( Handler && self )
            {
                _service->impl( ).orderbook_subscribe
                (
                    std::move( onBook ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex )
                    {
                        ( *self )( ex );
                    }
                );
            },
            token
        );
    }

    template< boost::asio::completion_token_for< void( std::exception_ptr ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto order_subscribe
    (
        std::function< void( uint64_t, Trading::Order ) > onOrder,
        CompletionToken && token = { }
    )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr ) >
        (
            [ this, onOrder = std::move( onOrder ) ]< class Handler >( Handler && self )
            {
                _service->impl( ).order_subscribe
                (
                    std::move( onOrder ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]
                    ( std::exception_ptr ex )
                    {
                        ( *self )( ex );
                    }
                );
            },
            token
        );
    }

private:
    Service * _service;
};

} // namespace Serum
} // namespace Synthfi

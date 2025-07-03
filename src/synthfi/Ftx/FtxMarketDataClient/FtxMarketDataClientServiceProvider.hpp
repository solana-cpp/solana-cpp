#pragma once

#include "synthfi/Trading/Book.hpp"

#include "synthfi/Ftx/FtxReferenceDataClient/FtxReferenceDataClientService.hpp"
#include "synthfi/Ftx/FtxWsClient/FtxWsClientService.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherService.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

namespace Synthfi
{
namespace Ftx
{

template< class Service >
class FtxMarketDataClientServiceProvider
{
public:
    explicit FtxMarketDataClientServiceProvider( Service & service )
        : _service( &service )
    { }

    FtxMarketDataClientServiceProvider
    (
        boost::asio::io_context & ioContext,
        FtxReferenceDataClientService & ftxReferenceDataClientService,
        FtxWsClientService & ftxWsClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService
    )
        : _service( &boost::asio::make_service< Service >( ioContext, ftxReferenceDataClientService, ftxWsClientService ) )
    { }

    template< boost::asio::completion_token_for< void( std::exception_ptr ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto orderbook_subscribe( std::function< void( uint64_t, Trading::Book ) > onBook, CompletionToken && token = { } )
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

private:
    Service * _service;
};

} // namespace Ftx
} // namespace Synthfi

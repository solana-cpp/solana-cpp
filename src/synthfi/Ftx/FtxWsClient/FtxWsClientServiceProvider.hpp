#pragma once

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

namespace Synthfi
{
namespace Ftx
{

template< class Service >
class FtxWsClientServiceProvider
{
public:
    explicit FtxWsClientServiceProvider( Service & service )
        : _service( &service )
    { }

    FtxWsClientServiceProvider
    (
        boost::asio::io_context & ioContext,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        std::string_view host
    )
        : _service( &boost::asio::make_service< Service >( ioContext, statisticsPublisherService, host ) )
    { }

    template< boost::asio::completion_token_for< void( std::exception_ptr ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto subscribe_orderbook
    (
        std::string marketName,
        Service::OnOrderbookMessageFn onOrderbookSnapshot,
        Service::OnOrderbookMessageFn onOrderbookUpdate,
        CompletionToken && token = { }
    )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr ) >
        (
            [
                this,
                marketName = std::move( marketName ),
                onOrderbookSnapshot = std::move( onOrderbookSnapshot ),
                onOrderbookUpdate = std::move( onOrderbookUpdate )
            ]< class Handler >( Handler && self )
            {
                _service->impl( ).subscribe_orderbook
                (
                    std::move( marketName ),
                    std::move( onOrderbookSnapshot ),
                    std::move( onOrderbookUpdate ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex )
                    {
                        (* self )( ex );
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

#pragma once

#include "synthfi/Ftx/FtxRestClient/FtxRestClientService.hpp"
#include "synthfi/Ftx/FtxReferenceDataClient/FtxReferenceDataClientService.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherService.hpp"

#include "synthfi/Ftx/FtxTypes.hpp"

#include "synthfi/Trading/Side.hpp"
#include "synthfi/Trading/Order.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

namespace Synthfi
{
namespace Ftx
{

template< class Service >
class FtxOrderClientServiceProvider
{
public:
    explicit FtxOrderClientServiceProvider( Service & service )
        : _service( &service )
    { }

    FtxOrderClientServiceProvider
    (
        boost::asio::io_context & ioContext,
        FtxRestClientService & ftxRestClientService,
        FtxReferenceDataClientService & ftxReferenceDataClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        const FtxAuthenticationConfig & config
    )
    : _service
    (
        &boost::asio::make_service< Service >
        (
            ioContext,
            ftxRestClientService,
            ftxReferenceDataClientService,
            statisticsPublisherService,
            config
       )
    )
    { }

    template< boost::asio::completion_token_for< void( std::exception_ptr ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto login( CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr ) >
        (
            [ this ]< class Handler >( Handler && self )
            {
                _service->impl( ).login
                (
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex )
                    {
                        ( *self )( ex );
                    }
                );
            },
            token
        );
    }

    template< boost::asio::completion_token_for< void( std::exception_ptr, Trading::Order ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto send_order( Trading::Order order, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, Trading::Order ) >
        (
            [ this, order = std::move( order ) ]< class Handler >( Handler && self )
            {
                _service->impl( ).send_order
                (
                    std::move( order ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]
                    ( std::exception_ptr ex, Trading::Order result )
                    {
                        ( *self )( ex, std::move( result ) );
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

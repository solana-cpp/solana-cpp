#pragma once

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

namespace Synthfi
{

namespace Ftx
{

template< class Service >
class FtxRestClientServiceProvider
{
public:
    explicit FtxRestClientServiceProvider( Service & service )
        : _service( &service )
    { }

    FtxRestClientServiceProvider
    (
        boost::asio::io_context & ioContext,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        const FtxAuthenticationConfig & config
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
        class RequestType,
        class ResponseType,
        boost::asio::completion_token_for< void( std::exception_ptr, ResponseType ) > CompletionToken = boost::asio::use_awaitable_t< >
    >
    auto send_request( RequestType request, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, ResponseType ) >
        (
            [ this, request = std::move( request ) ]< class Handler >( Handler && self )
            {
                _service->impl( ).template send_request< RequestType, ResponseType  >
                (
                    std::move( request ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex, ResponseType response )
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

} // namespace Ftx

} // namespace Synthfi

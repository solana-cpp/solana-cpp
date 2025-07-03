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
class SolanaHttpClientServiceProvider
{
public:
    explicit SolanaHttpClientServiceProvider( Service & service )
        : _service( &service )
    { }

    SolanaHttpClientServiceProvider
    (
        boost::asio::io_context & ioContext,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        const SolanaEndpointConfig & config
   )
        : _service( &boost::asio::make_service< Service >( ioContext, statisticsPublisherService, config ) )
    { }

    template
    <
        class RequestType,
        class ResponseType,
        boost::asio::completion_token_for< void( std::exception_ptr, ResponseType ) > CompletionToken = boost::asio::use_awaitable_t < >
    >
    auto send_request( RequestType request, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, ResponseType ) >
        (
            [ this, request = std::move( request ) ]< class Handler >( Handler && self )
            {
                _service->impl( ).template send_request< RequestType, ResponseType >
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

} // namespace Solana
} // namespace Synthfi

#pragma once

#include "synthfi/Core/KeyPair.hpp"

#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClientService.hpp"
#include "synthfi/Solana/SolanaWsClient/SolanaWsClientService.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherService.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

namespace Synthfi
{
namespace Solana
{

template< class Service >
class SlotSubscriberServiceProvider
{
public:
    explicit SlotSubscriberServiceProvider( Service & service )
        : _service( &service )
    { }

    SlotSubscriberServiceProvider
    (
        boost::asio::io_context & ioContext,
        SolanaHttpClientService & solanaHttpClientService,
        SolanaWsClientService & solanaWsClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService
    )
        : _service
        (
            &boost::asio::make_service< Service >
            (
                ioContext,
                solanaHttpClientService,
                solanaWsClientService,
                statisticsPublisherService
            )
        )
    { }

    template< boost::asio::completion_token_for< void( std::exception_ptr ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto subscribe_recent_blockhash( std::function< void( Core::Hash ) > onBlockhash, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr ) >
        (
            [ onBlockhash = std::move( onBlockhash ), this ]< class Handler >( Handler && self )
            {
                _service->subscriber( ).subscribe_recent_blockhash
                (
                    std::move( onBlockhash ),
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

} // namespace Solana
} // namespace Synthfi

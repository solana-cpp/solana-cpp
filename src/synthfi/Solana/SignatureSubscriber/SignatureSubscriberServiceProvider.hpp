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
class SignatureSubscriberServiceProvider
{
public:
    explicit SignatureSubscriberServiceProvider( Service & service )
        : _service( &service )
    { }

    SignatureSubscriberServiceProvider( boost::asio::io_context & ioContext, SolanaWsClientService & solanaWsClientService )
        : _service( &boost::asio::make_service< Service >( ioContext, solanaWsClientService ) )
    { }

    template< boost::asio::completion_token_for< void( std::exception_ptr, SignatureNotification ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto signature_subscribe( Core::Signature signature, Commitment commitment, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, SignatureNotification ) >
        (
            [ this, signature = std::move( signature ), commitment = commitment ]< class Handler >( Handler && self )
            {
                _service->subscriber( ).signature_subscribe
                (
                    std::move( signature ),
                    commitment,
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex, SignatureNotification notification )
                    {
                        ( *self )( ex, std::move( notification ) );
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

#pragma once

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Solana/SolanaWsMessage.hpp"
#include "synthfi/Solana/SolanaWsClient/SolanaWsClient.hpp"

#include <boost/asio/awaitable.hpp>

namespace Synthfi
{
namespace Solana
{

class SignatureSubscriberImpl
{
public:
    SignatureSubscriberImpl( boost::asio::io_context & ioContext, SolanaWsClientService & solanaWsClientService );

    template< class CompletionHandlerType >
    void signature_subscribe( Core::Signature signature, Commitment commitment, CompletionHandlerType completionHandler )
    {
        co_spawn( _strand, do_signature_subscribe( std::move( signature ), commitment ), completionHandler );
    }

private:
    void on_signature_notification( uint64_t subscriptionId, SignatureNotification notification );

    boost::asio::awaitable< SignatureNotification > do_signature_subscribe( Core::Signature signature, Commitment commitment );

    boost::asio::awaitable< void > do_handle_signature_notification( uint64_t subscriptionId, SignatureNotification notification );

    struct SignatureSubscription
    {
        SignatureSubscription( boost::asio::strand< boost::asio::io_context::executor_type > & strand, std::chrono::seconds timeout )
            : expiryTimer( strand, timeout )
        { }

        boost::asio::high_resolution_timer expiryTimer;
        SignatureNotification notification;
    };

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;
    SolanaWsClient _solanaWsClient;

    std::unordered_map< uint64_t, SignatureSubscription > _subscriptions;

    SynthfiLogger _logger;
};

} // namespace Solana
} // namespace Synthfi

#include "synthfi/Solana/SignatureSubscriber/SignatureSubscriberImpl.hpp"

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

namespace asio = boost::asio;
using namespace std::chrono_literals;

namespace Synthfi
{
namespace Solana
{

SignatureSubscriberImpl::SignatureSubscriberImpl( asio::io_context & ioContext, SolanaWsClientService & solanaWsClientService )
    : _strand( ioContext.get_executor( ) )
    , _solanaWsClient( solanaWsClientService )
{ }

void SignatureSubscriberImpl::on_signature_notification( uint64_t subscriptionId, SignatureNotification notification )
{
    co_spawn( _strand, do_handle_signature_notification( subscriptionId, std::move( notification ) ), boost::asio::detached );
}

asio::awaitable< SignatureNotification > SignatureSubscriberImpl::do_signature_subscribe( Core::Signature signature, Commitment commitment )
{
    SubscribeResult subscribeResult = co_await _solanaWsClient.send_subscribe< SignatureSubscribe, SubscribeResult, SignatureNotification >
    (
        SignatureSubscribe{ signature, commitment },
        std::bind( &SignatureSubscriberImpl::on_signature_notification, this, std::placeholders::_1, std::placeholders::_2 ),
        boost::asio::use_awaitable
    );

    auto & subscription = _subscriptions.emplace( subscribeResult.subscriptionId, SignatureSubscription( _strand, 30s) )
        .first->second;

    boost::system::error_code errorCode;
    co_await subscription.expiryTimer.async_wait( boost::asio::redirect_error( boost::asio::use_awaitable, errorCode ) );

    if ( errorCode == boost::asio::error::operation_aborted )
    {
        co_return subscription.notification;
    }
    else if ( errorCode )
    {
        throw SynthfiError( errorCode.what( ) );
    }
    throw SynthfiError( "Timed out ");
}

asio::awaitable< void > SignatureSubscriberImpl::do_handle_signature_notification( uint64_t subscriptionId, SignatureNotification notification )
{
    const auto & findSubscription = _subscriptions.find( subscriptionId );
    if ( findSubscription == _subscriptions.end( ) )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Missing subscription id: " << subscriptionId;
        co_return;
    }
    findSubscription->second.notification = std::move( notification );
    findSubscription->second.expiryTimer.cancel( );

    // On signatureNotification, the subscription is automatically cancelled.
    co_await _solanaWsClient.cancel_subscription( subscriptionId );
    co_return;
}

} // namespace Solana
} // namespace Synthfi

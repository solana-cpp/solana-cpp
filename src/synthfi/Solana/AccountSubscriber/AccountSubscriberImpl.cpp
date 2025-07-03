#include "synthfi/Solana/AccountSubscriber/AccountSubscriberImpl.hpp"

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>

using namespace std::chrono_literals;

namespace asio = boost::asio;

namespace Synthfi
{
namespace Solana
{

AccountSubscriberImpl::AccountSubscriberImpl
(
    asio::io_context & ioContext,
    SolanaWsClientService & solanaWsClientService
)
    : _strand( ioContext.get_executor( ) )
    , _solanaWsClient( solanaWsClientService )
{ }

void AccountSubscriberImpl::on_account_notification( uint64_t subscriptionId, AccountNotification notification )
{
    asio::co_spawn( _strand, do_handle_account_notification( subscriptionId, std::move( notification ) ), asio::detached );
}

boost::asio::awaitable< void > AccountSubscriberImpl::do_handle_account_notification( uint64_t subscriptionId, AccountNotification notification )
{
    auto findSubscription = _subscriptions.find( subscriptionId );
    if ( findSubscription == _subscriptions.end( ) )
    {
        SYNTHFI_LOG_ERROR( _logger) << fmt::format( "[{}] Invalid subscription id", name( ) );
        co_return;
    }

    for ( const auto & callback : findSubscription->second )
    {
        callback( subscriptionId, notification );
    }

    co_return;
}

asio::awaitable< SubscribeResult > AccountSubscriberImpl::account_subscribe
(
    AccountSubscribe request,
    std::function< void( uint64_t, AccountNotification ) > onAccountNotification
)
{
    auto subscriptionKey = std::make_pair( request.accountPublicKey, request.commitment );

    auto findPendingSubscription = _pendingAccountSubscriptions.find( subscriptionKey );
    if ( findPendingSubscription == _pendingAccountSubscriptions.end( ) )
    {
        auto findSubscriptionToAccount = _accountToCallback.find( subscriptionKey );
        // First request with this account/commitment.
        if ( findSubscriptionToAccount == _accountToCallback.end( ) )
        {
            // Other subscribers can wait on this timer while they wait for the subscription to complete.
            auto & pendingSubscription = _pendingAccountSubscriptions.emplace
            (
                std::piecewise_construct,
                std::forward_as_tuple( subscriptionKey ),
                std::forward_as_tuple( _strand, 30s, onAccountNotification )
            ).first->second;

            // Make subscription request.
            auto subscribeResult = co_await _solanaWsClient.send_subscribe< Solana::AccountSubscribe, Solana::SubscribeResult, Solana::AccountNotification >
            (
                std::move( request ),
                std::bind( &AccountSubscriberImpl::on_account_notification, this, std::placeholders::_1, std::placeholders::_2 )
            );

            // Construct subscription object.
            auto * subscription = &_subscriptions.emplace( subscribeResult.subscriptionId, std::move( pendingSubscription.callbacks ) ).first->second;
            _accountToCallback.emplace( subscriptionKey, subscription );

            // Cancel pending subscription to wake other subscribers.
            pendingSubscription.expiryTimer.cancel( );

            co_return SubscribeResult{ _nextSubscriptionId++ };
        }

        findSubscriptionToAccount->second->emplace_back( std::move( onAccountNotification ) );
        co_return SubscribeResult{ _nextSubscriptionId++ };
    }

    // Previous subscription request was made with account/commitment, wait until subscription completes.
    boost::system::error_code errorCode;
    co_await findPendingSubscription->second.expiryTimer.async_wait( boost::asio::redirect_error( boost::asio::use_awaitable, errorCode ) );

    if ( errorCode == asio::error::operation_aborted )
    {
        co_return SubscribeResult{ _nextSubscriptionId++ };
    }
    else if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( _logger ) << fmt::format( "[{}] Expiry timer error: {}", name( ), errorCode.what( ) );
        throw SynthfiError( "Account subscriber expiry timer error" );
    }
    else
    {
        throw SynthfiError( "AccountSubscriber expiry timer timed out" );
    }

    co_return SubscribeResult{ _nextSubscriptionId++ };
}

} // namespace Solana
} // namespace Synthfi

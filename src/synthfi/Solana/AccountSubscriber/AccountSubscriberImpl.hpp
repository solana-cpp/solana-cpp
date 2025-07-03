#pragma once

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Solana/SolanaWsClient/SolanaWsClient.hpp"
#include "synthfi/Solana/SolanaWsMessage.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/functional/hash.hpp>

namespace Synthfi
{
namespace Solana
{

class AccountSubscriberImpl
{
public:
    using AccountNotificationFn = std::function< void( uint64_t, AccountNotification ) >;

    AccountSubscriberImpl
    (
        boost::asio::io_context & ioContext,
        SolanaWsClientService & solanaWsClientService
    );

    template< class CompletionHandlerType >
    void account_subscribe
    (
        AccountSubscribe request,
        AccountNotificationFn onAccountNotification,
        CompletionHandlerType completionHandler
    )
    {
        co_spawn( _strand, account_subscribe( std::move( request ), std::move( onAccountNotification ) ), completionHandler );
    }

    constexpr std::string name( ) const & { return "AccountSubscriber"; };

private:
    boost::asio::awaitable< SubscribeResult > account_subscribe
    (
        AccountSubscribe request,
        AccountNotificationFn onAccountNotification
    );

    void on_account_notification( uint64_t subscriptionId, AccountNotification notification );
    boost::asio::awaitable< void > do_handle_account_notification( uint64_t subscriptionId, AccountNotification notification );

    struct PendingAccountSubscription
    {
        PendingAccountSubscription
        (
            boost::asio::strand< boost::asio::io_context::executor_type > & strand,
            std::chrono::seconds timeout,
            AccountNotificationFn accountNotification
        )
            : expiryTimer( strand, timeout )
            , callbacks{ accountNotification }
        { }

        boost::asio::high_resolution_timer expiryTimer;
        std::vector< AccountNotificationFn > callbacks;
    };

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;
    SolanaWsClient _solanaWsClient;

    std::unordered_map
    <
        std::pair< Core::PublicKey, Commitment >,
        PendingAccountSubscription,
        boost::hash< std::pair< Core::PublicKey, Commitment > >
    > _pendingAccountSubscriptions;

    std::unordered_map
    <
        std::pair< Core::PublicKey, Commitment >,
        std::vector< AccountNotificationFn > *,
        boost::hash< std::pair< Core::PublicKey, Commitment > >
    > _accountToCallback;

    uint64_t _nextSubscriptionId = 0;
    std::unordered_map< uint64_t, std::vector< AccountNotificationFn > > _subscriptions;

    SynthfiLogger _logger;
};

} // namespace Solana
} // namespace Synthfi

#pragma once

#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"
#include "synthfi/Solana/SolanaTypes.hpp"

#include "synthfi/Io/RpcSocket/RpcSocket.hpp"
#include "synthfi/Io/WebSocket/JsonWebSocket.hpp"
#include "synthfi/Io/WebSocket/SSLWebSocket.hpp"

#include "synthfi/Util/Logger.hpp"
#include "synthfi/Util/Utils.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/strand.hpp>

#include <fmt/core.h>

#include <unordered_map>


namespace Synthfi
{
namespace Solana
{

static constexpr std::chrono::seconds notification_wait_interval( ) { return std::chrono::seconds( 30 ); }

// Solana WebSocket JSON RPC API Client.
class SolanaWsClientImpl
{
public:
    SolanaWsClientImpl
    (
        boost::asio::io_context & ioContext,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        const SolanaEndpointConfig & config
   );

    template< class SubscribeType, class SubscribeResultType, class NotificationType, class CompletionHandlerType >
    void send_subscribe
    (
        SubscribeType request,
        std::function< void( uint64_t, NotificationType ) > onNotificationFn,
        CompletionHandlerType && completionHandler
    )
    {
        boost::asio::co_spawn
        (
            _strand,
            do_send_subscribe< SubscribeType, SubscribeResultType, NotificationType >( std::move( request ), std::move( onNotificationFn ) ),
            completionHandler
        );
    }

    template< class UnsubscribeType, class UnsubscribeResultType, class CompletionHandlerType >
    void send_unsubscribe( UnsubscribeType request, CompletionHandlerType && completionHandler )
    {
        boost::asio::co_spawn
        (
            _strand,
            do_send_unsubscribe< UnsubscribeType, UnsubscribeResultType >( std::move( request ) ),
            completionHandler
        );
    }

    template< class CompletionHandlerType >
    void cancel_subscription( uint64_t subscriptionId, CompletionHandlerType && completionHandler )
    {
        boost::asio::co_spawn( _strand, do_cancel( subscriptionId ), std::forward< CompletionHandlerType >( completionHandler ) );
    }

    // Blocking call to shutdown connection.
    void shutdown( )
    {
        boost::asio::co_spawn( _strand, _webSocket.do_close( ), boost::asio::use_future ).get( );
    }

    constexpr std::string name( ) const & { return "SolanaWsClient"; }

private:
    // Handle Json WebSocket messages.
    void on_notification( simdjson::ondemand::document message );

    boost::asio::awaitable< void > do_publish_statistics( );

    template< class SubscribeType, class SubscribeResultType, class NotificationType >
    boost::asio::awaitable< SubscribeResultType > do_send_subscribe( SubscribeType request, std::function< void( uint64_t, NotificationType ) > onNotificationFn )
    {
        auto response = co_await _webSocket.do_send_request< SubscribeType, SubscribeResultType >( std::move( request ) );
        auto subscriptionId = response.subscription_id( );
        SYNTHFI_LOG_INFO( _logger )
            << fmt::format( "[{}] Successfully subscribed, id: {}", name( ), subscriptionId );

        // RPC server subscription id's are not guaranteed unique
        // for example slotSubscribe subscription id is always 0.
        auto findActiveSubscription = _activeSubscriptions.find( subscriptionId );
        if ( findActiveSubscription == _activeSubscriptions.end( ) )
        {
            // Spawn subscription coro to handle subscription notifications.
            _activeSubscriptions.emplace
            (
                subscriptionId,
                std::make_unique< ActiveSubscription< NotificationType > >( _strand, notification_wait_interval( ) )
            );
            boost::asio::co_spawn( _strand, do_notify< NotificationType >( subscriptionId, onNotificationFn ), boost::asio::detached );
            boost::asio::co_spawn( _strand, do_publish_statistics( ), boost::asio::detached );

            co_return response;
        }

        throw SynthfiError( "Duplicate subscription ids are not supported" );
    }

    template< class UnsubscribeType, class UnsubscribeResultType >
    boost::asio::awaitable< UnsubscribeResultType > do_send_unsubscribe( UnsubscribeType request )
    {
        auto subscriptionId = request.subscriptionId;
        auto response = co_await _webSocket.do_send_request< UnsubscribeType, UnsubscribeResultType >( std::move( request ) );

        SYNTHFI_LOG_INFO( _logger )
            << fmt::format( "[{}] Successfully unsubscribed, subscription id: {}", name( ), subscriptionId );

        co_await do_cancel( subscriptionId );
        boost::asio::co_spawn( _strand, do_publish_statistics( ), boost::asio::detached );

        co_return response;
    }

    boost::asio::awaitable< bool > do_cancel( uint64_t subscriptionId )
    {
        const auto & findSubscription = _activeSubscriptions.find( subscriptionId ); // TODO: probably not necessary.
        if ( findSubscription == _activeSubscriptions.end( ) )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << fmt::format( "[{}] Cannot cancel subscription, missing subscription id: {}", name( ), subscriptionId );
            co_return false;
        }
        SYNTHFI_LOG_DEBUG( _logger )
            << fmt::format( "[{}] Cancelling subscription id: {}", name( ), subscriptionId );

        findSubscription->second->isCancelled = true;
        findSubscription->second->expiryTimer.cancel( );

        boost::asio::co_spawn( _strand, do_publish_statistics( ), boost::asio::detached );

        co_return true;
    }

    template< class NotificationType >
    boost::asio::awaitable< void > do_notify( uint64_t subscriptionId, std::function< void( uint64_t, NotificationType ) > onNotificationFn )
    {
        // This function shouldn't throw since it is detached from the subscription request completion handler.
        auto findActiveSubscription = _activeSubscriptions.find( subscriptionId );
        BOOST_ASSERT_MSG( findActiveSubscription != _activeSubscriptions.end( ), "Invalid subscription id" );

        auto * subscription = reinterpret_cast< ActiveSubscription< NotificationType > * >( findActiveSubscription->second.get( ) );

        auto updateWaitStartTime = _clock.now( );

        boost::system::error_code errorCode;
        while ( true )
        {
            co_await subscription->expiryTimer.async_wait( boost::asio::redirect_error( boost::asio::use_awaitable, errorCode ) );
            if ( errorCode == boost::asio::error::operation_aborted )
            {
                if ( subscription->error )
                {
                    try
                    {
                        // Reader had error processing subscription - rethrow and log.
                        std::rethrow_exception( subscription->error );
                    }
                    catch ( const std::exception & ex )
                    {
                        auto updateWaitEndTime = _clock.now( );
                        auto updateWaitDuration = updateWaitEndTime - updateWaitStartTime;

                        SYNTHFI_LOG_ERROR( _logger )
                            << fmt::format
                            (
                                "[{}] Subscription error: {}, update wait duration: {}",
                                name( ),
                                ex.what( ),
                                std::chrono::duration_cast< std::chrono::milliseconds >( updateWaitDuration )
                            );
                    }
                }
                try
                {
                    // Received subscription update - notify subscriber.
                    onNotificationFn( subscriptionId, std::move( subscription->message ) );

                    auto updateWaitEndTime = _clock.now( );
                    auto updateWaitDuration = updateWaitEndTime - updateWaitStartTime;

                    SYNTHFI_LOG_TRACE( _logger )
                        << fmt::format
                        (
                            "[{}] Received notification for type: {}, update wait duration: {}",
                            name( ),
                            typeid( NotificationType ).name( ),
                            std::chrono::duration_cast< std::chrono::milliseconds>( updateWaitDuration )
                        );
                }
                catch ( const std::exception & ex )
                {
                    // Exception in callback - log.
                    SYNTHFI_LOG_ERROR( _logger )
                        << fmt::format
                        (
                            "[{}] Error handling notification for type: {}, error: {}",
                            name( ),
                            typeid( NotificationType ).name( ),
                            ex.what( )
                        );
                }

                if ( subscription->isCancelled )
                {
                    SYNTHFI_LOG_INFO( _logger )
                        << fmt::format( "[{}] Removing subscription: {}", name( ), subscriptionId );
                    _activeSubscriptions.erase( subscriptionId );
                    co_return;
                }
            }
            else if ( errorCode )
            {
                // Expiry timer error.
                SYNTHFI_LOG_ERROR( _logger ) << fmt::format( "[{}] Subscription error: {}", name( ), errorCode.what( ) );

                // TODO: should unsubscribe as well.
                _activeSubscriptions.erase( subscriptionId );
                throw SynthfiError( errorCode.what( ) );
            }
            else
            {
                // Expiry timer timed out.
                auto updateWaitTime = _clock.now( );
                auto updateWaitDuration = updateWaitTime - updateWaitStartTime;

                SYNTHFI_LOG_DEBUG( _logger )
                    << fmt::format
                    (
                        "[{}] Subscription waiting for: {}",
                        name( ),
                        std::chrono::duration_cast< std::chrono::seconds >( updateWaitDuration )
                    );
            }

            subscription->expiryTimer.expires_from_now( notification_wait_interval( ) );
        }
    }

    struct ActiveSubscriptionBase
    {
        ActiveSubscriptionBase( boost::asio::strand< boost::asio::io_context::executor_type > & strand, std::chrono::seconds timeout )
            : expiryTimer( strand, timeout )
        { }

        virtual ~ActiveSubscriptionBase( ) = default;

        bool isCancelled = false;
        boost::asio::high_resolution_timer expiryTimer;
        std::exception_ptr error;

        virtual void set_result( simdjson::ondemand::value jsonValue ) = 0;
    };

    template< class NotificationType >
    struct ActiveSubscription : ActiveSubscriptionBase
    {
        ActiveSubscription( boost::asio::strand< boost::asio::io_context::executor_type > & strand, std::chrono::seconds timeout )
            : ActiveSubscriptionBase( strand, timeout )
        { }


        void set_result( simdjson::ondemand::value jsonValue ) override
        {
            message = json_to< NotificationType >( std::move( jsonValue ) );
        };

        NotificationType message;
    };

    std::chrono::high_resolution_clock _clock;

    // Asynchronous operations on a web socket must be performed within the same strand.
    boost::asio::strand< boost::asio::io_context::executor_type > _strand;

    Statistics::StatisticsPublisher _statisticsPublisher;

    std::unordered_map< uint64_t, std::unique_ptr< ActiveSubscriptionBase > > _activeSubscriptions;
    Statistics::InfluxMeasurement _activeSubscriptionsMeasurement;

    Io::RpcSocket< Io::JsonWebSocket< Io::SSLWebSocket > > _webSocket;

    SynthfiLogger _logger;
};

} // namespace Solana
} // namespace Synthfi

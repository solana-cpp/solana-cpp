#include "synthfi/Solana/SolanaWsClient/SolanaWsClient.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{
namespace Solana
{

SolanaWsClientImpl::SolanaWsClientImpl
(
    asio::io_context & ioContext,
    Statistics::StatisticsPublisherService & statisticsPublisherService,
    const SolanaEndpointConfig & config
)
    : _strand( ioContext.get_executor( ) )
    , _statisticsPublisher( statisticsPublisherService, "solana_ws_api" )
    , _activeSubscriptionsMeasurement
    (
        "active_subscriptions",
        { { "source", "solana" } },
        { { "count", Statistics::InfluxDataType( 0UL ) } }
    )
    , _webSocket
    (
        &_strand,
        &_statisticsPublisher,
        config.host,
        config.service,
        config.target,
        std::bind( &SolanaWsClientImpl::on_notification, this, std::placeholders::_1 )
    )
{
    boost::asio::co_spawn( _strand, do_publish_statistics( ), asio::detached );
}

asio::awaitable< void > SolanaWsClientImpl::do_publish_statistics( )
{
    _activeSubscriptionsMeasurement.fields[ 0 ].second = Statistics::InfluxDataType( _activeSubscriptions.size( ) );

    co_await _statisticsPublisher.publish_statistic( _activeSubscriptionsMeasurement );

    co_return;
}

void SolanaWsClientImpl::on_notification( simdjson::ondemand::document message )
{
    try
    {
        auto messageObject = message.get_object( ).value( );

        const auto method = messageObject[ "method" ].value( ).get_string( ).value( );
        auto params = messageObject[ "params" ].value( ).get_object( ).value( );
        const auto subscriptionId = params[ "subscription" ].value( ).get_uint64( ).value( );

        // Notify writer of request that response is successful.
        const auto & findActiveSubscription = _activeSubscriptions.find( subscriptionId );
        if ( findActiveSubscription == _activeSubscriptions.end( ) )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << fmt::format( "[{}] Invalid JSON RPC notification: invalid subscription id: {} ", name( ), subscriptionId );
        }
        else
        {
            findActiveSubscription->second->set_result( params[ "result" ].value( ) );
            findActiveSubscription->second->expiryTimer.cancel( );
        }
    }
    catch ( const std::exception & ex )
    {
        SYNTHFI_LOG_ERROR( _logger )
            << fmt::format( "[{}] Error reading RPC notification: {}", name( ), ex.what( ) );
    }
}

} // namespace Solana
} // namespace Synhfi

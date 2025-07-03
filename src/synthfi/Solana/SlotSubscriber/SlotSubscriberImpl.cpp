#include "synthfi/Solana/SlotSubscriber/SlotSubscriberImpl.hpp"

#include "synthfi/Solana/SolanaHttpMessage.hpp"

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/redirect_error.hpp>

#include <fmt/format.h>

namespace asio = boost::asio;

namespace Synthfi
{
namespace Solana
{

// When the block height to lastValidBlockHeight is less than this value, refresh the recent blockhash.
static constexpr uint64_t recent_blockhash_refresh_slot_interval( )
{
    return 30;
}

SlotSubscriberImpl::SlotSubscriberImpl
(
    asio::io_context & ioContext,
    SolanaHttpClientService & solanaHttpClientService,
    SolanaWsClientService & solanaWsClientService,
    Statistics::StatisticsPublisherService & statisticsPublisherService
)
    : _strand( ioContext.get_executor( ) )
    , _solanaHttpClient( solanaHttpClientService )
    , _solanaWsClient( solanaWsClientService )
    , _statisticsPublisher( statisticsPublisherService, "solana_slot_subscriber" )
{
    // Initialize measurements.
    _slotMeasurement.measurementName = "slot";
    _slotMeasurement.tags = { { "source", "solana" } };
    _slotMeasurement.fields =
    {
        { "slot", Statistics::InfluxDataType( 0UL ) },
        { "root", Statistics::InfluxDataType( 0UL ) },
        { "parent", Statistics::InfluxDataType( 0UL ) }
    };

    _recentBlockhashMeasurement.measurementName = "recent_blockhash";
    _recentBlockhashMeasurement.tags = { { "source", "solana" } };
    _recentBlockhashMeasurement.fields =
    {
        { "blockhash", Statistics::InfluxDataType( _recentBlockhash.enc_base58_text( ) ) },
        { "last_valid_block_height", Statistics::InfluxDataType( 0UL ) }
    };

    asio::co_spawn( _strand, do_load_subscription( ), asio::detached );
}

void SlotSubscriberImpl::on_slot_notification( uint64_t subscriptionId, Solana::SlotNotification slotNotification )
{
    asio::co_spawn( _strand, do_on_slot_notification( subscriptionId, std::move( slotNotification ) ), asio::detached );
}

boost::asio::awaitable< void > SlotSubscriberImpl::do_on_slot_notification
(
    uint64_t subscriptionId,
    Solana::SlotNotification slotNotification
)
{
    // Update measurement.
    _slotMeasurement.fields[ 0 ].second = slotNotification.slot;
    _slotMeasurement.fields[ 1 ].second = slotNotification.root;
    _slotMeasurement.fields[ 2 ].second = slotNotification.parent;

    // Publish updated measurement.
    co_await _statisticsPublisher.publish_statistic( _slotMeasurement );

    if ( slotNotification.slot < _recentBlockhashSlot + recent_blockhash_refresh_slot_interval( ) )
    {
        // Recent blockhash valid for greater block height than refresh block height.
        co_return;
    }

    SYNTHFI_LOG_DEBUG( _logger )
        << fmt::format
        (
            "[{}] Refreshing recent blockhash, slot: {}, recent blockhash slot: {} ",
            name( ),
            slotNotification.slot,
            _recentBlockhashSlot
        );

    auto blockhashResponse = co_await _solanaHttpClient.send_request< Solana::GetLatestBlockhashRequest, Solana::GetLatestBlockhashResponse >
    (
        { Commitment::finalized }
    );

    if ( blockhashResponse.lastValidBlockHeight <= _lastValidBlockHeight )
    {
        SYNTHFI_LOG_INFO( _logger )
            << fmt::format
            (
                "[{}] Latest blockhash: {}, last valid block height: {} <= recent blockhash last valid block height: {}",
                name( ),
                blockhashResponse.blockhash,
                blockhashResponse.lastValidBlockHeight,
                _lastValidBlockHeight
            );
            co_return;
    }

    _recentBlockhash = blockhashResponse.blockhash;
    _lastValidBlockHeight = blockhashResponse.lastValidBlockHeight;

    // Notify recent blockhash subscribers.
    for ( const auto & callback : _onBlockhashCallbacks )
    {
        callback( _recentBlockhash );
    }

    // Update measurement.
    _recentBlockhashMeasurement.fields[ 0 ].second = blockhashResponse.blockhash.enc_base58_text( );
    _recentBlockhashMeasurement.fields[ 1 ].second = blockhashResponse.lastValidBlockHeight;
    co_await _statisticsPublisher.publish_statistic( _recentBlockhashMeasurement );

    SYNTHFI_LOG_INFO( _logger )
        << fmt::format
        (
            "[{}] Updated recent blockhash: {}, slot: {}, last valid block height: {}",
            name( ),
            blockhashResponse.blockhash,
            slotNotification.slot,
            blockhashResponse.lastValidBlockHeight
        );

    co_return;
}

boost::asio::awaitable< void > SlotSubscriberImpl::do_subscribe_recent_blockhash( std::function< void( Core::Hash ) > onBlockhash )
{
    // Notify subscriber if blockhash is available.
    if ( !_recentBlockhash.is_zero( ) )
    {
        onBlockhash( _recentBlockhash );
    }

    _onBlockhashCallbacks.push_back( std::move( onBlockhash ) );

    co_return;
}

boost::asio::awaitable< void > SlotSubscriberImpl::do_load_subscription( )
{
    SYNTHFI_LOG_DEBUG( _logger ) << fmt::format( "[{}] Subscribing to slot", name( ) );

    co_await _solanaWsClient.send_subscribe< Solana::SlotSubscribe, Solana::SubscribeResult, Solana::SlotNotification >
    (
        Solana::SlotSubscribe{ },
        std::bind( &SlotSubscriberImpl::on_slot_notification, this, std::placeholders::_1, std::placeholders::_2 )
    );
}

} // namespace Solana
} // namespace Synthfi

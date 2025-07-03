#pragma once

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClient.hpp"
#include "synthfi/Solana/SolanaWsClient/SolanaWsClient.hpp"
#include "synthfi/Solana/SolanaWsMessage.hpp"

#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

#include <boost/asio/awaitable.hpp>


namespace Synthfi
{
namespace Solana
{

class SlotSubscriberImpl
{
public:
    SlotSubscriberImpl
    (
        boost::asio::io_context & ioContext,
        SolanaHttpClientService & solanaHttpClientService,
        SolanaWsClientService & solanaWsClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService
    );

    template< class CompletionHandlerType >
    void subscribe_recent_blockhash
    (
        std::function< void( Core::Hash ) > onBlockhash,
        CompletionHandlerType completionHandler
    )
    {
        boost::asio::co_spawn( _strand, do_subscribe_recent_blockhash( std::move( onBlockhash ) ), completionHandler );
    }

    constexpr std::string name( ) const & { return "SlotSubscriber"; }

private:
    boost::asio::awaitable< void > do_subscribe_recent_blockhash( std::function< void( Core::Hash ) > onBlockhash );

    // On slot update, check for new blockhash.
    void on_slot_notification( uint64_t subscriptionId, Solana::SlotNotification slotNotification );
    boost::asio::awaitable< void > do_on_slot_notification( uint64_t subscriptionId, Solana::SlotNotification slotNotification );

    boost::asio::awaitable< void > do_load_subscription( );

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;

    SolanaHttpClient _solanaHttpClient;
    SolanaWsClient _solanaWsClient;

    Statistics::StatisticsPublisher _statisticsPublisher;

    Statistics::InfluxMeasurement _slotMeasurement;
    Statistics::InfluxMeasurement _recentBlockhashMeasurement;

    uint64_t _recentBlockhashSlot = 0;
    uint64_t _lastValidBlockHeight = 0;
    Core::Hash _recentBlockhash;

    std::vector< std::function< void( Core::Hash ) > > _onBlockhashCallbacks;

    SynthfiLogger _logger;
};

} // namespace Solana
} // namespace Synthfi

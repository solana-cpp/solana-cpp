#pragma once

#include "synthfi/Trading/Book.hpp"
#include "synthfi/Trading/Order.hpp"

#include "synthfi/Serum/SerumTypes.hpp"

#include "synthfi/Serum/SerumReferenceDataClient/SerumReferenceDataClient.hpp"
#include "synthfi/Solana/AccountBatcher/AccountBatcher.hpp"
#include "synthfi/Solana/AccountSubscriber/AccountSubscriber.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

#include "synthfi/Solana/SolanaWsMessage.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>
#include <functional>
#include <string>

namespace Synthfi
{
namespace Serum
{

class SerumMarketDataClientImpl
{
public:
    SerumMarketDataClientImpl
    (
        boost::asio::io_context & ioContext,
        Solana::AccountBatcherService & solanaAccountBatcher,
        Solana::AccountSubscriberService & solanaAccountSubscriber,
        Serum::SerumReferenceDataClientService & serumReferenceDataClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService
    );

    template< class CompletionHandlerType >
    void orderbook_subscribe
    (
        std::function< void( uint64_t, Trading::Book ) > onBook,
        CompletionHandlerType completionHandler )
    {
        co_spawn( _strand, do_orderbook_subscribe( std::move( onBook ) ), completionHandler );
    }

    template< class CompletionHandlerType >
    void orders_subscribe
    (
        std::function< void( uint64_t, Trading::Order ) > onOrder,
        CompletionHandlerType completionHandler
    )
    {
        co_spawn
        (
            _strand,
            do_orders_subscribe( std::move( onOrder ) ),
            completionHandler
        );
    }

    constexpr std::string name( ) const & { return "SerumMarketDataClient"; }

private:
    boost::asio::awaitable< void > do_load_market_data( );

    void on_orderbook_account( uint64_t tradingPairIndex, uint64_t subscriptionId, Solana::AccountNotification notification );
    boost::asio::awaitable< void > do_on_orderbook_account( uint64_t tradingPairIndex, Solana::AccountNotification notification );

    void on_event_queue_account( uint64_t tradingPairIndex, uint64_t subscriptionId, Solana::AccountNotification notification );
    boost::asio::awaitable< void > do_on_event_queue_account( uint64_t tradingPairIndex, Solana::AccountNotification notification );

    boost::asio::awaitable< void > do_on_event( uint64_t tradingPairIndex, Serum::SerumEvent event );

    boost::asio::awaitable< void > do_orderbook_subscribe
    (
        std::function< void( uint64_t, Trading::Book ) > onBook
    );

    boost::asio::awaitable< void > do_orders_subscribe
    (
        std::function< void( uint64_t, Trading::Order ) > onOrder
    );

    boost::asio::awaitable< void > do_update_book
    (
        uint64_t tradingPairIndex,
        SerumOrderbookAccount serumOrderbookAccount,
        bool doPublish
    );

    boost::asio::awaitable< void > do_publish_statistics( uint64_t tradingPairIndex, Trading::Side side );

    std::chrono::high_resolution_clock _clock;

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;
    boost::asio::high_resolution_timer _expiryTimer;

    Solana::AccountBatcher _accountBatcher;
    Solana::AccountSubscriber _accountSubscriber;
    Serum::SerumReferenceDataClient _referenceDataClient;
    Statistics::StatisticsPublisher _statisticsPublisher;

    bool _isInitialized = false;
    SerumReferenceData _referenceData;

    std::vector< std::function< void( uint64_t, Trading::Book ) > > _onBooks;
    std::vector< Trading::Book > _books;

    std::vector< uint64_t > _eventQueueSequenceNumbers;
    std::unordered_map< uint64_t, SerumLeafNode > _orders; // maps client order id, to serum order state.
    std::vector< std::function< void( uint64_t, Trading::Order ) > > _onOrders;

    std::vector< Trading::SidePair< std::vector< Statistics::InfluxMeasurement > > > _bookMeasurements;

    mutable SynthfiLogger _logger;
};

} // namespace Serum
} // namespace Synthfi

#pragma once

#include "synthfi/Ftx/FtxReferenceDataClient/FtxReferenceDataClient.hpp"
#include "synthfi/Ftx/FtxWsClient/FtxWsClient.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

#include "synthfi/Ftx/FtxTypes.hpp"
#include "synthfi/Ftx/FtxWsMessage.hpp"
#include "synthfi/Trading/Book.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/awaitable.hpp>

#include <chrono>
#include <string>

namespace Synthfi
{
namespace Ftx
{

class FtxMarketDataClientImpl
{
public:
    FtxMarketDataClientImpl
    (
        boost::asio::io_context & ioContext,
        FtxReferenceDataClientService & ftxReferenceDataClientService,
        FtxWsClientService & ftxWsClientService,
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

    constexpr std::string name( ) const & { return "FtxMarketDataClient"; }

private:
    boost::asio::awaitable< void > do_orderbook_subscribe( std::function< void( uint64_t, Trading::Book ) > onBook );

    void on_orderbook_snapshot( uint64_t tradingPairIndex, FtxOrderbookMessage orderbookSnapshot );
    void on_orderbook_update( uint64_t tradingPairIndex, FtxOrderbookMessage orderbookUpdate );

    boost::asio::awaitable< void > do_on_orderbook_snapshot( uint64_t tradingPairIndex, FtxOrderbookMessage orderbookSnapshot );
    boost::asio::awaitable< void > do_on_orderbook_update( uint64_t tradingPairIndex, FtxOrderbookMessage orderbookUpdate );

    boost::asio::awaitable< void > do_publish_statistics( uint64_t tradingPairIndex );

    std::chrono::high_resolution_clock _clock;

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;

    FtxWsClient _wsClient;
    FtxReferenceDataClient _referenceDataClient;
    Statistics::StatisticsPublisher _statisticsPublisher;

    FtxReferenceData _referenceData;
    std::function< void( uint64_t, Trading::Book ) > _onBook;

    std::vector< Trading::Book > _books;

    std::vector< std::vector< Statistics::InfluxMeasurement > > _bookMeasurements;

    mutable SynthfiLogger _logger;
};

} // namespace Ftx
} // namespace Synthfi

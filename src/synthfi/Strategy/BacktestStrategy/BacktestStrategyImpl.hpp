#pragma once

#include "synthfi/Serum/SerumTypes.hpp"
#include "synthfi/Strategy/BacktestStrategy/BacktestStrategyTypes.hpp"
#include "synthfi/Trading/Book.hpp"

// Services
#include "synthfi/Core/KeyStore/KeyStore.hpp"
#include "synthfi/Ftx/FtxReferenceDataClient/FtxReferenceDataClient.hpp"
#include "synthfi/Ftx/FtxMarketDataClient/FtxMarketDataClient.hpp"
#include "synthfi/Serum/SerumReferenceDataClient/SerumReferenceDataClient.hpp"
#include "synthfi/Serum/SerumMarketDataClient/SerumMarketDataClient.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

#include <boost/asio/awaitable.hpp>

#include <chrono>

namespace Synthfi
{
namespace Strategy
{

namespace asio = boost::asio;

class BacktestStrategyImpl
{
public:
    BacktestStrategyImpl
    (
        boost::asio::io_context & ioContext,
        Core::KeyStoreService & keyStoreService,
        Serum::SerumReferenceDataClientService & serumReferenceDataClientService,
        Serum::SerumMarketDataClientService & serumMarketDataClientService,
        Ftx::FtxReferenceDataClientService & ftxReferenceDataClientService,
        Ftx::FtxMarketDataClientService & ftxMarketDataClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        BacktestStrategyConfig config
    );

    boost::asio::awaitable< void > run( );

    constexpr std::string name( ) const & { return "BacktestStrategy"; }

private:
    void on_serum_book( uint64_t tradingPairIndex, Trading::Book book );
    boost::asio::awaitable< void > do_on_serum_book( uint64_t tradingPairIndex, Trading::Book book );

    void on_ftx_book( uint64_t tradingPairIndex, Trading::Book book );
    boost::asio::awaitable< void > do_on_ftx_book( uint64_t tradingPairIndex, Trading::Book book );

    template< Trading::Side SideValue >
    boost::asio::awaitable< void > do_book_update( uint64_t tradingPairIndex );

    std::chrono::high_resolution_clock _clock;

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;

    Core::KeyStore _keyStore;

    Serum::SerumReferenceDataClient _serumReferenceDataClient;
    Serum::SerumMarketDataClient _serumMarketDataClient;

    Ftx::FtxReferenceDataClient _ftxReferenceDataClient;
    Ftx::FtxMarketDataClient _ftxMarketDataClient;

    Serum::SerumReferenceData _serumReferenceData;
    Ftx::FtxReferenceData _ftxReferenceData;

    Statistics::StatisticsPublisher _statisticsPublisher;

    std::vector< Trading::Book > _serumBooks;
    std::vector< Trading::Book > _ftxBooks;

    mutable SynthfiLogger _logger;
};

} // namespace Strategy
} // namespace Synthfi

#pragma once

#include "synthfi/Mango/MangoTypes.hpp"
#include "synthfi/Serum/SerumTypes.hpp"
#include "synthfi/Strategy/TakeStrategy/TakeStrategyTypes.hpp"
#include "synthfi/Trading/Book.hpp"
#include "synthfi/Trading/Wallet.hpp"

// Services
#include "synthfi/Core/KeyStore/KeyStore.hpp"
#include "synthfi/Ftx/FtxMarketDataClient/FtxMarketDataClient.hpp"
#include "synthfi/Ftx/FtxOrderClient/FtxOrderClient.hpp"
#include "synthfi/Ftx/FtxReferenceDataClient/FtxReferenceDataClient.hpp"
#include "synthfi/Ftx/FtxWalletClient/FtxWalletClient.hpp"
#include "synthfi/Mango/MangoOrderClient/MangoOrderClient.hpp"
#include "synthfi/Mango/MangoWalletClient/MangoWalletClient.hpp"
#include "synthfi/Serum/SerumMarketDataClient/SerumMarketDataClient.hpp"
#include "synthfi/Serum/SerumReferenceDataClient/SerumReferenceDataClient.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

#include <boost/asio/awaitable.hpp>

namespace Synthfi
{
namespace Strategy
{

class TakeStrategyImpl
{
public:
    TakeStrategyImpl
    (
        boost::asio::io_context & ioContext,
        Ftx::FtxReferenceDataClientService & ftxReferenceDataClientService,
        Serum::SerumReferenceDataClientService & serumReferenceDataClientService,
        Ftx::FtxMarketDataClientService & ftxMarketDataClientService,
        Serum::SerumMarketDataClientService & serumMarketDataClientService,
        Ftx::FtxWalletClientService & ftxWalletClientService,
        Mango::MangoWalletClientService & mangoWalletClientService,
        Ftx::FtxOrderClientService & ftxOrderClientService,
        Mango::MangoOrderClientService & mangoOrderClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        const TakeStrategyOptionsConfig & config
    );

    boost::asio::awaitable< void > run( );

    constexpr std::string name( ) const & { return "TakeStrategy"; }

private:
    struct MangoIndexPair
    {
        uint8_t baseIndex;
        uint8_t quoteIndex;
    };

    boost::asio::awaitable< void > do_update_ftx_balance( );

    // On mango account update, refresh balances.
    void on_mango_wallet( Trading::Wallet wallet );
    boost::asio::awaitable< void > do_on_mango_wallet( Trading::Wallet wallet );

    void on_ftx_wallet( Trading::Wallet wallet );
    boost::asio::awaitable< void > do_on_ftx_wallet( Trading::Wallet wallet );

    // On serum book update, check for arbitrage.
    void on_serum_book( uint64_t tradingPairIndex, Trading::Book book );
    boost::asio::awaitable< void > do_on_serum_book( uint64_t tradingPairIndex, Trading::Book book );

    // On ftx book update, check for arbitrage.
    void on_ftx_book( uint64_t tradingPairIndex, Trading::Book book );
    boost::asio::awaitable< void > do_on_ftx_book( uint64_t tradingPairIndex, Trading::Book book );

    // On book update, check for arbitrage.
    boost::asio::awaitable< void > do_book_update( size_t tradingPairIndex, Trading::Side side );

    bool _isRunning = false;
    bool _isTrading = false;

    Trading::Quantity _maxUsdTradeSize; // Largest trade size in USD.
    Trading::Price _minUsdTradeProfit; // Minimum profit for a trade.

    std::chrono::high_resolution_clock _clock;
    boost::asio::strand< boost::asio::io_context::executor_type > _strand;

    Ftx::FtxReferenceDataClient _ftxReferenceDataClient;
    Serum::SerumReferenceDataClient _serumReferenceDataClient;

    Ftx::FtxMarketDataClient _ftxMarketDataClient;
    Serum::SerumMarketDataClient _serumMarketDataClient;

    Ftx::FtxWalletClient _ftxWalletClient;
    Mango::MangoWalletClient _mangoWalletClient;

    Ftx::FtxOrderClient _ftxOrderClient;
    Mango::MangoOrderClient _mangoOrderClient;

    Statistics::StatisticsPublisher _statisticsPublisher;

    Ftx::FtxReferenceData _ftxReferenceData;
    Serum::SerumReferenceData _serumReferenceData;

    // Strategy maintains up-to-date FTX and Serum books.
    // Book updates are the primary event which drives trading activity.
    std::vector< Trading::Book > _ftxBooks;
    std::vector< Trading::Book > _serumBooks;

    // Strategy maintains up-to-date FTX and Mango wallets.
    // Wallets are indexed by currency index.
    Trading::Wallet _ftxWallet;
    Trading::Wallet _mangoWallet;

    mutable SynthfiLogger _logger;
};

} // namespace Strategy
} // namespace Synthfi

#include "synthfi/Strategy/TakeStrategy/TakeStrategyImpl.hpp"

#include "synthfi/Mango/MangoInstruction.hpp"
#include "synthfi/Solana/SolanaHttpMessage.hpp"

#include "synthfi/Ftx/FtxTypes.hpp"

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_future.hpp>
#include <boost/asio/experimental/promise.hpp>

#include <fmt/core.h>
#include <fmt/ranges.h>

#include <boost/asio/experimental/awaitable_operators.hpp>
using namespace boost::asio::experimental::awaitable_operators;
using namespace std::chrono_literals;

namespace asio = boost::asio;

namespace Synthfi
{
namespace Strategy
{

static constexpr std::chrono::milliseconds ftx_wallet_refresh_interval( ) { return 1000ms; }

TakeStrategyImpl::TakeStrategyImpl
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
)
    : _maxUsdTradeSize( config.maxUsdTradeSize )
    , _minUsdTradeProfit( config.minUsdTradeProfit )
    , _strand( ioContext.get_executor( ) )
    , _ftxReferenceDataClient( ftxReferenceDataClientService )
    , _serumReferenceDataClient( serumReferenceDataClientService )
    , _ftxMarketDataClient( ftxMarketDataClientService )
    , _serumMarketDataClient( serumMarketDataClientService )
    , _ftxWalletClient( ftxWalletClientService )
    , _mangoWalletClient( mangoWalletClientService )
    , _ftxOrderClient( ftxOrderClientService )
    , _mangoOrderClient( mangoOrderClientService )
    , _statisticsPublisher( statisticsPublisherService, "take_strategy" )
{ }

void TakeStrategyImpl::on_serum_book( uint64_t tradingPairIndex, Trading::Book book )
{
    co_spawn( _strand, do_on_serum_book( tradingPairIndex, std::move( book ) ), boost::asio::detached );
}

void TakeStrategyImpl::on_ftx_book( uint64_t tradingPairIndex, Trading::Book book )
{
    co_spawn( _strand, do_on_ftx_book( tradingPairIndex, std::move( book ) ), boost::asio::detached );
}

void TakeStrategyImpl::on_mango_wallet( Trading::Wallet wallet )
{
    co_spawn( _strand, do_on_mango_wallet( std::move( wallet ) ), boost::asio::detached );
}

void TakeStrategyImpl::on_ftx_wallet( Trading::Wallet wallet )
{
    co_spawn( _strand, do_on_ftx_wallet( std::move( wallet ) ), boost::asio::detached );
}

asio::awaitable< void > TakeStrategyImpl::do_on_ftx_wallet( Trading::Wallet ftxWallet )
{
    _ftxWallet = std::move( ftxWallet );

    co_return;
}

asio::awaitable< void > TakeStrategyImpl::do_on_serum_book( uint64_t tradingPairIndex, Synthfi::Trading::Book book )
{
    _serumBooks[ tradingPairIndex ] = std::move( book );
    co_await
    (
        do_book_update( tradingPairIndex, Trading::Side::bid )
        && do_book_update( tradingPairIndex, Trading::Side::ask )
    );

    SYNTHFI_LOG_TRACE( _logger ) << fmt::format( "[{}] On book update, trading pair index: {}", name( ), tradingPairIndex );

    co_return;
}

asio::awaitable< void > TakeStrategyImpl::do_on_ftx_book( uint64_t tradingPairIndex, Synthfi::Trading::Book book )
{
    _ftxBooks[ tradingPairIndex ] = std::move( book );
    co_await
    (
        do_book_update( tradingPairIndex, Trading::Side::bid )
        && do_book_update( tradingPairIndex, Trading::Side::ask )
    );

    SYNTHFI_LOG_TRACE( _logger ) << fmt::format( "[{}] On book update, trading pair index: {}", name( ), tradingPairIndex );

   co_return;
}

asio::awaitable< void > TakeStrategyImpl::do_on_mango_wallet( Trading::Wallet mangoWallet )
{
    _mangoWallet = std::move( mangoWallet );

    SYNTHFI_LOG_DEBUG( _logger ) << fmt::format( "[{}] Hedging mango positions: {}", name( ), _mangoWallet.positions );

    for ( size_t currencyIndex = 0; currencyIndex < _ftxReferenceData.currencies.size( ); ++currencyIndex )
    {
        const auto & mangoPosition = _mangoWallet.positions[ currencyIndex ];
        const auto & ftxPosition = _ftxWallet.positions[ currencyIndex ];

        auto netPosition = mangoPosition + ftxPosition;

        if ( netPosition < 0 )
        {
            const auto & currencyName = _ftxReferenceData.currencies[ currencyIndex ].currencyName;

            // Don't hedge USDC quantity.
            if ( currencyName == "USD" ) // actually USDC.
            {
                co_return;
            }

            auto findTradingPair = std::find_if
            (
                _ftxReferenceData.tradingPairs.begin( ),
                _ftxReferenceData.tradingPairs.end( ),
                [ &currencyName ]( const auto & tradingPair )
                {
                    return tradingPair.marketInformation.baseCurrency == currencyName;
                }
            );
            if ( findTradingPair == _ftxReferenceData.tradingPairs.end( ) )
            {
                SYNTHFI_LOG_ERROR( _logger )
                    << fmt::format
                    (
                        "[{}] FTX is not configured with trading pair for currency: {}",
                        name( ),
                        currencyName
                    );
                co_return;
            }

            auto tradingPairIndex = std::distance( _ftxReferenceData.tradingPairs.begin( ), findTradingPair );

            const auto & ftxTradingPair = _ftxReferenceData.tradingPairs[ tradingPairIndex ];

            if ( -netPosition < ftxTradingPair.marketInformation.quantityIncrement )
            {
                SYNTHFI_LOG_DEBUG( _logger )
                    << fmt::format
                    (
                        "[{}] Can't hedge mango position for currency: {}, position: {} < minimum price increment: {}",
                        name( ),
                        currencyName,
                        mangoPosition,
                        ftxTradingPair.marketInformation.quantityIncrement
                    );
                co_return;
            }

            if ( !_ftxBooks[ tradingPairIndex ].is_valid( ) )
            {
                SYNTHFI_LOG_ERROR( _logger )
                    << fmt::format
                    (
                        "[{}] Can't hedge, invalid ftx book: {}",
                        name( ),
                        ftxTradingPair.marketInformation.name
                    );
                co_return;
            }

            auto ftxAskIt = _ftxBooks[ tradingPairIndex ].get( Trading::Side::ask ).begin( );
            auto ftxAskEnd = _ftxBooks[ tradingPairIndex ].get( Trading::Side::ask ).end( );

            Trading::Price tradePrice;
            Trading::Quantity totalLevelQuantity;

            while ( totalLevelQuantity < -netPosition && ftxAskIt != ftxAskEnd )
            {
                tradePrice = ftxAskIt->price( );
                totalLevelQuantity += ftxAskIt->quantity( );
                ++ftxAskIt;
            }

            Trading::Order ftxOrder
            (
                tradePrice,
                -netPosition,
                Trading::Side::bid,
                Trading::OrderType::immediateOrCancel,
                tradingPairIndex,
                "ftx"
            );
            auto pendingOrder = _ftxOrderClient.send_order( std::move( ftxOrder ) );

            SYNTHFI_LOG_DEBUG( _logger )
                << fmt::format
                (
                    "[{}] Hedging ftx/mango/net positions: {}/{}/{} trade price: {}",
                    name( ),
                    ftxPosition,
                    mangoPosition,
                    netPosition,
                    tradePrice
                );
        }
    }

    SYNTHFI_LOG_DEBUG( _logger ) << fmt::format( "[{}] Done hedging mango positions: {}", name( ), _mangoWallet.positions );

    co_return;
}


asio::awaitable< void > TakeStrategyImpl::do_book_update( size_t tradingPairIndex, Trading::Side side )
{
    const auto & marketName = _ftxReferenceData.tradingPairs[ tradingPairIndex ].marketInformation.name;

    const auto & serumSideBook = _serumBooks[ tradingPairIndex ].get( side );
    const auto & ftxSideBook = _ftxBooks[ tradingPairIndex ].get( Trading::side_flip( side ) );

    if ( serumSideBook.empty( ) )
    {
        SYNTHFI_LOG_DEBUG( _logger ) << fmt::format( "[{}] Serum side book invalid", marketName );
        co_return;
    }

    if ( ftxSideBook.empty( ) )
    {
        SYNTHFI_LOG_DEBUG( _logger ) << fmt::format( "[{}] Ftx side book invalid", marketName );
        co_return;
    }

    // Setup iterators
    auto serumSideBookIt = serumSideBook.begin( );
    const auto & serumSideBookItEnd = serumSideBook.cend( );
    auto ftxSideBookIt = ftxSideBook.begin( );
    const auto & ftxSideBookItEnd = ftxSideBook.cend( );

    // Arbitrage information.
    Trading::Quantity remainingSerumBidsQuantity = serumSideBookIt->quantity( );
    Trading::Quantity remainingFtxAsksQuantity = ftxSideBookIt->quantity( );
    Trading::Quantity arbitrageQuantity = Trading::Quantity{ 0 };

    Trading::Price serumTradePrice = Trading::Price{ 0 };
    Trading::Price tradeCost = Trading::Price{ 0 };
    Trading::Price expectedProfit = Trading::Price{ 0 };

    Trading::Price totalFtxFee = Trading::Quantity{ 0 };
    Trading::Price totalSerumFee = Trading::Quantity{ 0 };

    // Iterate over serum bids and ftx asks, accumulating trade quantity until the arbitrage is no longer yields profitable.
    while ( serumSideBookIt != serumSideBookItEnd && ftxSideBookIt != ftxSideBookItEnd )
    {
        Trading::Price serumFee = serumSideBookIt->price( ) * Serum::serum_taker_fee( );
        Trading::Price ftxFee = ftxSideBookIt->price( ) * Ftx::ftx_taker_fee( );

        if ( ( side == Trading::Side::bid  )
            ? ( ( serumSideBookIt->price( ) - serumFee ) <= ( ftxSideBookIt->price( ) + ftxFee ) )
            : ( ( serumSideBookIt->price( ) + serumFee ) >= ( ftxSideBookIt->price( ) - ftxFee ) ) )
        {
            SYNTHFI_LOG_DEBUG( _logger ) << fmt::format
            (
                "[{}] Break, bid: {}, ask: {}, bid fee: {}, ask fee: {}",
                marketName,
                serumSideBookIt->price( ),
                ftxSideBookIt->price( ),
                serumFee,
                ftxFee
            );
            break;
        }

        serumTradePrice = serumSideBookIt->price( );

        auto levelSize = std::min( serumSideBookIt->quantity( ), ftxSideBookIt->quantity( ) );

        arbitrageQuantity += levelSize;
        tradeCost += levelSize * serumSideBookIt->price( );
        expectedProfit += ( side == Trading::Side::bid )
            ? ( serumSideBookIt->price( ) - ( ftxSideBookIt->price( ) + ftxFee + serumFee ) ) * levelSize
            : ( ftxSideBookIt->price( ) - ( serumSideBookIt->price( ) + ftxFee + serumFee ) ) * levelSize;

        totalFtxFee += ftxFee * arbitrageQuantity;
        totalSerumFee += serumFee * arbitrageQuantity;

        if ( remainingSerumBidsQuantity < remainingFtxAsksQuantity )
        {
            remainingFtxAsksQuantity -= remainingSerumBidsQuantity;
            ++serumSideBookIt;
            remainingSerumBidsQuantity = serumSideBookIt->quantity( );
        }
        else if ( remainingSerumBidsQuantity > remainingFtxAsksQuantity )
        {
            remainingSerumBidsQuantity -= remainingFtxAsksQuantity;
            ++ftxSideBookIt;
            remainingFtxAsksQuantity = ftxSideBookIt->quantity( );
        }
        else
        {
            ++serumSideBookIt;
            remainingSerumBidsQuantity = serumSideBookIt->quantity( );
            ++ftxSideBookIt;
            remainingFtxAsksQuantity = ftxSideBookIt->quantity( );
        }
    }

    auto publishStatistic =
    [
        this,
        side,
        expectedProfit,
        arbitrageQuantity,
        serumSideBook,
        ftxSideBook,
        tradingPairIndex
    ]( )
    {
        auto spread = ( side == Trading::Side::bid )
            ? ( ftxSideBook.begin( )->price( ) - serumSideBook.begin( )->price( ) )
            : ( serumSideBook.begin( )->price( ) - ftxSideBook.begin( )->price( ) );

        auto spreadWithFees = ( side == Trading::Side::bid )
            ? ( ( ftxSideBook.begin( )->price( ) * ( 1 + Ftx::ftx_taker_fee( ) ) )
                - ( serumSideBook.begin( )->price( ) * ( 1 - Serum::serum_taker_fee( ) ) ) )
            : ( ( serumSideBook.begin( )->price( ) * ( 1 + Serum::serum_taker_fee( ) ) )
                - ( ftxSideBook.begin( )->price( ) * ( 1 - Ftx::ftx_taker_fee( ) ) ) );

        Statistics::InfluxMeasurement arbitrageMeasurement;
        arbitrageMeasurement.measurementName = "arbitrage";
        arbitrageMeasurement.tags =
        {
            { "source", "strategy" },
            { "side", std::string( magic_enum::enum_name( side ) ) },
            { "trading_pair_index", std::to_string( tradingPairIndex ) }
        };
        arbitrageMeasurement.fields =
        {
            { "arbitrage_quantity", Statistics::InfluxDataType( arbitrageQuantity ) },
            { "expected_profit", Statistics::InfluxDataType( expectedProfit ) },
            { "spread", Statistics::InfluxDataType( spread ) },
            { "spread_with_fees", Statistics::InfluxDataType( spreadWithFees ) },
            { "min_usd_trade_profit", Statistics::InfluxDataType( _minUsdTradeProfit ) },
            { "max_usd_trade_size", Statistics::InfluxDataType( _maxUsdTradeSize ) },
            { "is_trading", Statistics::InfluxDataType( _isTrading ) }
        };
        co_spawn( _strand, _statisticsPublisher.publish_statistic( arbitrageMeasurement ), asio::detached );
    };

    if ( _isTrading  )
    {
        SYNTHFI_LOG_DEBUG( _logger ) << fmt::format( "[{}] Unable to arbitrage: strategy is trading", marketName );
        publishStatistic( );
        co_return;
    }

    if ( expectedProfit < _minUsdTradeProfit )
    {
        SYNTHFI_LOG_DEBUG( _logger ) << fmt::format
        (
            "[{}] Unable to arbitrage: expected profit: {} < min USD trade profit: {}",
            marketName,
            expectedProfit,
            _minUsdTradeProfit,
            ftxSideBook.begin( )->price( ) - serumSideBook.begin( )->price( )
        );
        publishStatistic( );
        co_return;
    }

    _isTrading = true;

    Trading::Quantity maxTradeQuantity = _maxUsdTradeSize / _maxUsdTradeSize;
    Trading::Quantity tradeQuantity = boost::multiprecision::min( arbitrageQuantity, maxTradeQuantity );

    Trading::Order takeLiquidityOrder
    (
        serumTradePrice,
        tradeQuantity,
        side,
        Trading::OrderType::immediateOrCancel,
        tradingPairIndex,
        "serum"
    );
    auto pendingOrder = _mangoOrderClient.send_order
    (
        std::move( takeLiquidityOrder )
    );

    SYNTHFI_LOG_INFO( _logger ) << fmt::format
    (
        "[{}] Sent mango take order: serum price: {}, quantity: {}",
        marketName,
        serumTradePrice,
        tradeQuantity
    );

    auto order = co_await std::move( pendingOrder );

    publishStatistic( );
}


boost::asio::awaitable< void > TakeStrategyImpl::run( )
{
    BOOST_ASSERT_MSG( !_isRunning, "TakeStrategy is already running" );

    SYNTHFI_LOG_INFO( _logger ) << fmt::format( "[{}] Loading strategy", name( ) );

    auto [ serumReferenceData, ftxReferenceData ] = co_await
    (
        _serumReferenceDataClient.get_reference_data( )
        && _ftxReferenceDataClient.get_reference_data( )
    );

    // Load reference data.
    _serumReferenceData = std::move( serumReferenceData ); 
    _ftxReferenceData = std::move( ftxReferenceData );

    auto currencyCount = _serumReferenceData.currencies.size( );
    auto tradingPairCount = _serumReferenceData.tradingPairs.size( );

    _serumBooks.resize( tradingPairCount );
    _ftxBooks.resize( tradingPairCount );

    _ftxWallet.positions.resize( currencyCount, Trading::Quantity( 0 ) );
    _ftxWallet.marginAvailable.resize( currencyCount, Trading::Quantity( 0 ) );

    // Subscribe to wallet updates.
    co_await
    (
        _mangoWalletClient.subscribe_wallet
        (
            std::bind( &TakeStrategyImpl::on_mango_wallet, this, std::placeholders::_1 )
        )
        && _ftxWalletClient.subscribe_wallet
        (
            std::bind( &TakeStrategyImpl::on_ftx_wallet, this, std::placeholders::_1 )
        )
    );

    // Subscribe to orderbook updates.
    co_await
    (
        _serumMarketDataClient.orderbook_subscribe
        (
            std::bind( &TakeStrategyImpl::on_serum_book, this, std::placeholders::_1, std::placeholders::_2 )
        )
        && _ftxMarketDataClient.orderbook_subscribe
        (
            std::bind( &TakeStrategyImpl::on_ftx_book, this, std::placeholders::_1, std::placeholders::_2 )
        )
    );

    // Load order clients.
    co_await ( _ftxOrderClient.login( ) && _mangoOrderClient.load_mango_account( ) );

    _isRunning = true; // Strategy I/O loop is running.

    SYNTHFI_LOG_INFO( _logger ) << fmt::format( "[{}] Done loading strategy", name( ) );

    co_return;
}

} // namespace Strategy
} // namespace Synthfi

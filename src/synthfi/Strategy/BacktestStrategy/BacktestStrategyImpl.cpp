#include "synthfi/Strategy/BacktestStrategy/BacktestStrategyImpl.hpp"

#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <fmt/core.h>

#include <boost/asio/experimental/awaitable_operators.hpp>
using namespace boost::asio::experimental::awaitable_operators;

namespace asio = boost::asio;

namespace Synthfi
{
namespace Strategy
{

BacktestStrategyImpl::BacktestStrategyImpl
(
    asio::io_context & ioContext,
    Core::KeyStoreService & keyStoreService,
    Serum::SerumReferenceDataClientService & serumReferenceDataClientService,
    Serum::SerumMarketDataClientService & serumMarketDataClientService,
    Ftx::FtxReferenceDataClientService & ftxReferenceDataClientService,
    Ftx::FtxMarketDataClientService & ftxMarketDataClientService,
    Statistics::StatisticsPublisherService & statisticsPublisherService,
    BacktestStrategyConfig config
)
    : _strand( ioContext.get_executor( ) )
    , _keyStore( keyStoreService )
    , _serumReferenceDataClient( serumReferenceDataClientService )
    , _serumMarketDataClient( serumMarketDataClientService )
    , _ftxReferenceDataClient( ftxReferenceDataClientService )
    , _ftxMarketDataClient( ftxMarketDataClientService )
    , _statisticsPublisher( statisticsPublisherService, "backtest_strategy" )
{ }

void BacktestStrategyImpl::on_serum_book( uint64_t tradingPairIndex, Trading::Book book )
{
    co_spawn( _strand, do_on_serum_book( tradingPairIndex, std::move( book ) ), boost::asio::detached );
}

void BacktestStrategyImpl::on_ftx_book( uint64_t tradingPairIndex, Trading::Book book )
{
    co_spawn( _strand, do_on_ftx_book( tradingPairIndex, std::move( book ) ), boost::asio::detached );
}

asio::awaitable< void > BacktestStrategyImpl::do_on_serum_book( uint64_t tradingPairIndex, Synthfi::Trading::Book book )
{
    _serumBooks[ tradingPairIndex ] = std::move( book );

    co_await
    (
        do_book_update< Trading::Side::bid >( tradingPairIndex )
        && do_book_update< Trading::Side::ask >( tradingPairIndex )
    );

    co_return;
}

asio::awaitable< void > BacktestStrategyImpl::do_on_ftx_book( uint64_t tradingPairIndex, Synthfi::Trading::Book book )
{
    _ftxBooks[ tradingPairIndex ] = std::move( book );

    co_await
    (
        do_book_update< Trading::Side::bid >( tradingPairIndex )
        && do_book_update< Trading::Side::ask >( tradingPairIndex )
    );

    co_return;
}

template< Trading::Side SideValue >
asio::awaitable< void > BacktestStrategyImpl::do_book_update( uint64_t tradingPairIndex )
{
    const auto & marketName = _ftxReferenceData.tradingPairs[ tradingPairIndex ].marketInformation.name;

    const auto & serumSideBook = _serumBooks[ tradingPairIndex ].get( SideValue );
    const auto & ftxSideBook = _ftxBooks[ tradingPairIndex ].get( Trading::side_flip( SideValue ) );

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

        if ( ( SideValue == Trading::Side::bid  )
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
        expectedProfit += ( SideValue == Trading::Side::bid )
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

    auto spread = ( SideValue == Trading::Side::bid )
        ? ( ftxSideBook.begin( )->price( ) - serumSideBook.begin( )->price( ) )
        : ( serumSideBook.begin( )->price( ) - ftxSideBook.begin( )->price( ) );

    auto spreadWithFees = ( SideValue == Trading::Side::bid )
        ? ( ( ftxSideBook.begin( )->price( ) * ( 1 + Ftx::ftx_taker_fee( ) ) )
            - ( serumSideBook.begin( )->price( ) * ( 1 - Serum::serum_taker_fee( ) ) ) )
        : ( ( serumSideBook.begin( )->price( ) * ( 1 + Serum::serum_taker_fee( ) ) )
            - ( ftxSideBook.begin( )->price( ) * ( 1 - Ftx::ftx_taker_fee( ) ) ) );

    Statistics::InfluxMeasurement arbitrageMeasurement;
    arbitrageMeasurement.measurementName = "arbitrage";
    arbitrageMeasurement.tags =
    {
        { "source", "strategy" },
        { "side", std::string( magic_enum::enum_name( SideValue ) ) },
        { "trading_pair_index", std::to_string( tradingPairIndex ) }
    };
    arbitrageMeasurement.fields =
    {
        { "arbitrage_quantity", Statistics::InfluxDataType( arbitrageQuantity ) },
        { "expected_profit", Statistics::InfluxDataType( expectedProfit ) },
        { "spread", Statistics::InfluxDataType( spread ) },
        { "spread_with_fees", Statistics::InfluxDataType( spreadWithFees ) }
    };
    co_spawn( _strand, _statisticsPublisher.publish_statistic( arbitrageMeasurement ), asio::detached );

    co_return;
}

boost::asio::awaitable< void > BacktestStrategyImpl::run( )
{
    SYNTHFI_LOG_INFO( _logger ) << fmt::format( "[{}] Loading strategy", name( ) );

    // Load reference data.
    auto [ serumReferenceData, ftxReferenceData ] = co_await
    (
        _serumReferenceDataClient.get_reference_data( )
        && _ftxReferenceDataClient.get_reference_data( )
    );

    _serumReferenceData = std::move( serumReferenceData );
    _ftxReferenceData = std::move( ftxReferenceData );

    auto tradingPairCount = _serumReferenceData.tradingPairs.size( );

    _serumBooks.resize( tradingPairCount );
    _ftxBooks.resize( tradingPairCount );

    // Subscribe to orderbooks.
    co_await
    (
        _ftxMarketDataClient.orderbook_subscribe
        (
            std::bind( &BacktestStrategyImpl::on_ftx_book, this, std::placeholders::_1, std::placeholders::_2 )
        )
        && _serumMarketDataClient.orderbook_subscribe
        (
            std::bind( &BacktestStrategyImpl::on_serum_book, this, std::placeholders::_1, std::placeholders::_2 )
        )
    );

    SYNTHFI_LOG_INFO( _logger ) << fmt::format( "[{}] Done loading strategy", name( ) );

    co_return;
}

} // namespace Strategy
} // namespace Synthfi

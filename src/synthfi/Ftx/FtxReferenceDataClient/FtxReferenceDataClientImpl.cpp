#include "synthfi/Ftx/FtxReferenceDataClient/FtxReferenceDataClientImpl.hpp"

#include "synthfi/Ftx/FtxRestMessage.hpp"

#include <boost/asio/detached.hpp>

#include <boost/asio/experimental/awaitable_operators.hpp>

using namespace boost::asio::experimental::awaitable_operators;
using namespace std::chrono_literals;

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{
namespace Ftx
{

FtxReferenceDataClientImpl::FtxReferenceDataClientImpl
(
    boost::asio::io_context & ioContext,
    FtxRestClientService & ftxRestClientService,
    Statistics::StatisticsPublisherService & statisticsPublisherService,
    std::vector< std::string > ftxMarketNames,
    std::vector< std::string > ftxCurrencyNames
)
    : _strand( ioContext.get_executor( ) )
    , _expiryTimer( _strand, 30s )
    , _ftxRestClient( ftxRestClientService )
    , _statisticsPublisher( statisticsPublisherService, "ftx_reference_data_client" )
{
    BOOST_ASSERT_MSG( !ftxMarketNames.empty( ), "FtxReferenceDataClient expected non-empty market names" );
    BOOST_ASSERT_MSG( !ftxCurrencyNames.empty( ), "FtxReferenceDataClient expected non-empty currency names" );

    asio::co_spawn
    (
        _strand,
        do_load_reference_data( std::move( ftxMarketNames ), std::move( ftxCurrencyNames ) ),
        boost::asio::detached
    );
}

asio::awaitable< FtxReferenceData > FtxReferenceDataClientImpl::do_get_reference_data( )
{
    if ( _isInitialized )
    {
        // Reference data request already completed.
        co_return _referenceData;
    }

    // Wait for reference data request to complete.
    boost::system::error_code errorCode;

    co_await _expiryTimer.async_wait( boost::asio::redirect_error( boost::asio::use_awaitable, errorCode ) );
    if ( errorCode == asio::error::operation_aborted )
    {
        if ( _isInitialized )
        {
            // Reference data was successfully loaded.
            co_return _referenceData;
        }
        // Failed to load reference data.
        throw SynthfiError( "FtxReferenceDataClient unable to load reference data" );
    }
    else if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( _logger ) << fmt::format( "[{}] Timer error: {}", name( ), errorCode.what( ) );
        throw SynthfiError( "Timer error" );
    }
    throw SynthfiError( "Reference data request timed out" );
}

asio::awaitable< void > FtxReferenceDataClientImpl::do_load_reference_data
(
    std::vector< std::string > ftxMarketNames,
    std::vector< std::string > ftxCurrencyNames
)
{
    BOOST_ASSERT_MSG( !_isInitialized, "FtxReferenceDataClient already loaded reference data" );

    SYNTHFI_LOG_DEBUG( _logger ) << fmt::format
    (
        "[{}] Loading reference data, ftx market names: {}, currency names: {}",
        name( ),
        fmt::join( ftxMarketNames, "," ),
        fmt::join( ftxCurrencyNames, "," )
    );

    auto getMarketsResponse = co_await _ftxRestClient.send_request< GetMarketsRequest, GetMarketsResponse >( { } );

    auto tradingPairCount = ftxMarketNames.size( );
    auto currencyCount = ftxCurrencyNames.size( );

    _referenceData.tradingPairs.resize( tradingPairCount );
    _tradingPairMeasurements.resize( tradingPairCount );

    _referenceData.currencies.resize( currencyCount );
    _currencyMeasurements.resize( currencyCount );

    for ( size_t tradingPairIndex = 0; tradingPairIndex < tradingPairCount; ++tradingPairIndex )
    {
        const auto & ftxMarketName = ftxMarketNames[ tradingPairIndex ];
        auto & tradingPair = _referenceData.tradingPairs[ tradingPairIndex ];

        auto findMarket = std::find_if
        (
            getMarketsResponse.markets.begin( ),
            getMarketsResponse.markets.end( ),
            [ &ftxMarketName ]( const auto & ftxMarket )
            {
                return ftxMarket.name == ftxMarketName;
            }
        );
        if ( findMarket == getMarketsResponse.markets.end( ) )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << fmt::format( "[{}] Reference data does not exist for FTX market: {}", name( ), ftxMarketName );
            throw SynthfiError( "Invalid FTX market" );
        }
        tradingPair.marketInformation = std::move( *findMarket );

        auto findBaseCurrency = std::find_if
        (
            ftxCurrencyNames.begin( ),
            ftxCurrencyNames.end( ),
            [ &tradingPair ]( const auto & currencyName )
            {
                return tradingPair.marketInformation.baseCurrency == currencyName;
            }
        );
        if ( findBaseCurrency == ftxCurrencyNames.end( ) )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << fmt::format
                (
                    "[{}] Missing configured base currency: {}",
                    name( ),
                    tradingPair.marketInformation.baseCurrency
                );
            throw SynthfiError( "Ftx markets missing configured currency for trading pair" );
        }

        auto findQuoteCurrency = std::find_if
        (
            ftxCurrencyNames.begin( ),
            ftxCurrencyNames.end( ),
            [ &tradingPair ]( const auto & currencyName )
            {
                return tradingPair.marketInformation.quoteCurrency == currencyName;
            }
        );
        if ( findQuoteCurrency == ftxCurrencyNames.end( ) )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << fmt::format( "[{}] Missing configured quote currency: {}", name( ), tradingPair.marketInformation.quoteCurrency );
            throw SynthfiError( "Ftx markets missing configured currency for trading pair" );
        }

        // Compute currency indices.
        tradingPair.baseCurrencyIndex = std::distance( ftxCurrencyNames.begin( ), findBaseCurrency );
        tradingPair.quoteCurrencyIndex = std::distance( ftxCurrencyNames.begin( ), findQuoteCurrency );

        // Initialize trading pair measurements.
        auto & tradingPairMeasurement = _tradingPairMeasurements[ tradingPairIndex ];

        tradingPairMeasurement.measurementName = "trading_pair";
        tradingPairMeasurement.tags =
        {
            { "source", "ftx" },
            { "trading_pair_index", std::to_string( tradingPairIndex ) },
            { "base_currency_index", std::to_string( tradingPair.baseCurrencyIndex ) },
            { "quote_currency_index", std::to_string( tradingPair.quoteCurrencyIndex ) }
        };
        tradingPairMeasurement.fields =
        {
            { "market_name", tradingPair.marketInformation.name },
            { "price_increment", Statistics::InfluxDataType( tradingPair.marketInformation.priceIncrement ) },
            { "quantity_increment", Statistics::InfluxDataType( tradingPair.marketInformation.quantityIncrement ) }
        };
    }

    // Initialize currency measurements.
    for ( size_t currencyIndex = 0; currencyIndex < currencyCount; ++currencyIndex )
    {
        const auto & currencyName = ftxCurrencyNames[ currencyIndex ];
        auto & currency = _referenceData.currencies[ currencyIndex ];
        currency.currencyName = std::move( ftxCurrencyNames[ currencyIndex ] );

        auto & currencyMeasurement = _currencyMeasurements[ currencyIndex ];

        currencyMeasurement.measurementName = "currency";
        currencyMeasurement.tags =
        {
            { "source", "ftx" },
            { "currency_index", std::to_string( currencyIndex ) }
        };
        currencyMeasurement.fields =
        {
            { "currency_name", currency.currencyName }
        };
    }

    co_spawn( _strand, do_publish_statistics( ), asio::detached );

    // Wake clients waiting on reference data.
    _isInitialized = true;
    _expiryTimer.cancel( );

    co_return;
}

boost::asio::awaitable< void > FtxReferenceDataClientImpl::do_publish_statistics( )
{
    // Publish currency and trading pair statistics.
    co_await
    (
        _statisticsPublisher.publish_statistics( _tradingPairMeasurements )
        && _statisticsPublisher.publish_statistics( _currencyMeasurements )
    );

    co_return;
}

} // namespace Ftx
} // namespace Synthfi

#include "synthfi/Serum/SerumReferenceDataClient/SerumReferenceDataClient.hpp"

#include "synthfi/Solana/SolanaHttpMessage.hpp"

#include <boost/asio/redirect_error.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>

#include <boost/asio/experimental/awaitable_operators.hpp>
using namespace boost::asio::experimental::awaitable_operators;
using namespace std::chrono_literals;

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{
namespace Serum
{

SerumReferenceDataClientImpl::SerumReferenceDataClientImpl
(
    asio::io_context & ioContext,
    Solana::AccountBatcherService & solanaAccountBatcherService,
    Statistics::StatisticsPublisherService & statisticsPublisherService,
    std::vector< Core::PublicKey > serumAccountAddresses,
    std::vector< Core::PublicKey > mintAddresses
)
    : _strand( ioContext.get_executor( ) )
    , _expiryTimer( _strand, 30s )
    , _accountBatcher( solanaAccountBatcherService )
    , _statisticsPublisher( statisticsPublisherService, "serum_reference_data" )
{
    BOOST_ASSERT_MSG( !serumAccountAddresses.empty( ), "SerumReferenceDataClient expected non-empty account addresses" );
    BOOST_ASSERT_MSG( !mintAddresses.empty( ), "SerumReferenceDataClient expected non-empty mint addresses" );

    asio::co_spawn
    (
        _strand,
        do_load_reference_data
        (
            std::move( serumAccountAddresses ),
            std::move( mintAddresses )
        ),
        boost::asio::detached
    );
}

asio::awaitable< SerumReferenceData > SerumReferenceDataClientImpl::do_get_reference_data( )
{
    if ( _isInitialized )
    {
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
        throw SynthfiError( "SerumReferenceDataClient unable to load reference data" );
    }
    else if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( _logger ) << fmt::format( "[{}] Timer error: {}", name( ), errorCode.what( ) );
        throw SynthfiError( "Timer error" );
    }
    throw SynthfiError( "Reference data request timed out" );
}

asio::awaitable< void > SerumReferenceDataClientImpl::do_load_reference_data
(
    std::vector< Core::PublicKey > serumMarketAddresses,
    std::vector< Core::PublicKey > mintAddresses
)
{
    BOOST_ASSERT_MSG( !_isInitialized, "SerumReferenceDataClient already loaded reference data" );

    SYNTHFI_LOG_INFO( _logger ) << fmt::format
    (
        "[{}] Loading reference data, serum market addresses: {}, mint addresses: {}",
        name( ),
        fmt::join( serumMarketAddresses, "," ),
        fmt::join( mintAddresses, "," )
    );

    auto tradingPairCount = serumMarketAddresses.size( );
    auto currencyCount = mintAddresses.size( );

    _referenceData.tradingPairs.resize( tradingPairCount );
    _tradingPairMeasurements.resize( tradingPairCount );

    _referenceData.currencies.resize( currencyCount );
    _currencyMeasurements.resize( currencyCount );

    // Get serum market accounts.
    Solana::GetMultipleAccountsRequest serumAccountsRequest{ serumMarketAddresses };
    auto serumAccountsResponse = co_await _accountBatcher.get_multiple_accounts( std::move( serumAccountsRequest ) );

    BOOST_ASSERT_MSG
    (
        std::adjacent_find
        (
            serumAccountsResponse.accountInfos.begin( ),
            serumAccountsResponse.accountInfos.end( ),
            []( const auto & lhs, const auto & rhs )
            {
                return lhs.owner != rhs.owner;
            }
        ) == serumAccountsResponse.accountInfos.end( ),
        "Serum program ids don't match"
    );

    _referenceData.serumProgramId = serumAccountsResponse.accountInfos.front( ).owner;

    // Deserialize serum market accounts.
    for ( size_t tradingPairIndex = 0; tradingPairIndex < tradingPairCount; ++tradingPairIndex )
    {
        _referenceData.tradingPairs[ tradingPairIndex ].serumMarketAddress = serumMarketAddresses[ tradingPairIndex ];
        _referenceData.tradingPairs[ tradingPairIndex ].serumMarketAccount = Solana::account_to< SerumMarketAccount >
        (
            std::move( serumAccountsResponse.accountInfos[ tradingPairIndex ] )
        );
    }

    // Validate trading pairs are configured with tradable currencies.
    for ( size_t tradingPairIndex = 0; tradingPairIndex < tradingPairCount; ++tradingPairIndex )
    {
        auto & tradingPair = _referenceData.tradingPairs[ tradingPairIndex ];

        auto findBaseCurrency = std::find_if
        (
            mintAddresses.begin( ),
            mintAddresses.end( ),
            [ &tradingPair ]( const auto & mintAddress )
            {
                return tradingPair.serumMarketAccount.baseMint == mintAddress;
            }
        );
        if ( findBaseCurrency == mintAddresses.end( ) )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << "SerumMarket missing configured base currency: " << serumMarketAddresses[ tradingPairIndex ];
            throw SynthfiError( "Serum markets missing configured trading pair" );
        }

        auto findQuoteCurrency = std::find_if
        (
            mintAddresses.begin( ),
            mintAddresses.end( ),
            [ &tradingPair ]( const auto & mintAddress )
            {
                return tradingPair.serumMarketAccount.quoteMint == mintAddress;
            }
        );
        if ( findQuoteCurrency == mintAddresses.end( ) )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << "SerumMarket missing configured quote currency: " << serumMarketAddresses[ tradingPairIndex ];
            throw SynthfiError( "Serum markets missing configured trading pair" );
        }

        // Compute currency indices.
        tradingPair.baseCurrencyIndex = std::distance( mintAddresses.begin( ), findBaseCurrency );
        tradingPair.quoteCurrencyIndex = std::distance( mintAddresses.begin( ), findQuoteCurrency );
    }

    // Get token mint accounts.
    Solana::GetMultipleAccountsRequest getMintAccounts{ mintAddresses };
    auto mintAccountsResponse = co_await _accountBatcher.get_multiple_accounts( std::move( getMintAccounts ) );

    for ( size_t currencyIndex = 0; currencyIndex < currencyCount; ++currencyIndex )
    {
        auto & currency = _referenceData.currencies[ currencyIndex ];
        currency.mintAddress = mintAddresses[ currencyIndex ];
        currency.mintAccount = Solana::account_to< Solana::TokenMintAccount >
        (
            std::move( mintAccountsResponse.accountInfos[ currencyIndex ] )
        );

        // Initialize currency measurements.
        auto & currencyMeasurement = _currencyMeasurements[ currencyIndex ];

        currencyMeasurement.measurementName = "currency";
        currencyMeasurement.tags =
        {
            { "source", "serum" },
            { "currency_index", std::to_string( currencyIndex ) }
        };
        currencyMeasurement.fields =
        {
            { "mint_address", currency.mintAddress.enc_base58_text( ) },
            { "decimals", Statistics::InfluxDataType( static_cast< uint64_t >( currency.mintAccount.decimals ) ) }
        };
    }

    // Set price and quantity increment.
    for ( size_t tradingPairIndex = 0; tradingPairIndex < _referenceData.tradingPairs.size( ); ++tradingPairIndex )
    {
        auto & tradingPair = _referenceData.tradingPairs[ tradingPairIndex ];
        const auto & baseCurrency = _referenceData.currencies[ tradingPair.baseCurrencyIndex ];
        const auto & quoteCurrency = _referenceData.currencies[ tradingPair.quoteCurrencyIndex ];
    
        tradingPair.priceIncrement = serum_to_synthfi_price
        (
            tradingPair.serumMarketAccount.quoteLotSize,
            quoteCurrency.mintAccount.decimals
        );
        tradingPair.quantityIncrement = serum_to_synthfi_quantity
        (
            tradingPair.serumMarketAccount.baseLotSize,
            baseCurrency.mintAccount.decimals
        );

        // Initialize trading pair measurements.
        auto & tradingPairMeasurement = _tradingPairMeasurements[ tradingPairIndex ];

        tradingPairMeasurement.measurementName = "trading_pair";
        tradingPairMeasurement.tags =
        {
            { "source", "serum" },
            { "trading_pair_index", std::to_string( tradingPairIndex ) },
            { "base_currency_index", std::to_string( tradingPair.baseCurrencyIndex ) },
            { "quote_currency_index", std::to_string( tradingPair.quoteCurrencyIndex ) }
        };
        tradingPairMeasurement.fields =
        {
            { "market_address", tradingPair.serumMarketAddress.enc_base58_text( ) },
            { "price_increment", Statistics::InfluxDataType( tradingPair.priceIncrement ) },
            { "quantity_increment", Statistics::InfluxDataType( tradingPair.quantityIncrement ) }
        };
    }

    SYNTHFI_LOG_DEBUG( _logger ) << fmt::format( "[{}] Loaded reference data", name( ) );

    co_spawn( _strand, do_publish_statistics( ), boost::asio::detached );

    // Wake clients waiting on reference data.
    _isInitialized = true;
    _expiryTimer.cancel( );

    co_return;
}

boost::asio::awaitable< void > SerumReferenceDataClientImpl::do_publish_statistics( )
{
    // Publish currency and trading pair statistics.
    co_await
    (
        _statisticsPublisher.publish_statistics( _tradingPairMeasurements )
        && _statisticsPublisher.publish_statistics( _currencyMeasurements )
    );

    co_return;
}

} // namespace Serum
} // namespace Synthfi

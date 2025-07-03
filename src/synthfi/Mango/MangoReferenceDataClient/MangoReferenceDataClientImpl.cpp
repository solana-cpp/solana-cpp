#include "synthfi/Mango/MangoReferenceDataClient/MangoReferenceDataClient.hpp"

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
namespace Mango
{

MangoReferenceDataClientImpl::MangoReferenceDataClientImpl
(
    asio::io_context & ioContext,
    Solana::AccountBatcherService & solanaAccountBatcherService,
    Serum::SerumReferenceDataClientService & serumReferenceDataClientService,
    Statistics::StatisticsPublisherService & statisticsPublisherService,
    Core::PublicKey mangoAccountAddress
)
    : _strand( ioContext.get_executor( ) )
    , _expiryTimer( _strand, 30s )
    , _accountBatcher( solanaAccountBatcherService )
    , _serumReferenceDataClient( serumReferenceDataClientService )
    , _statisticsPublisher( statisticsPublisherService, "mango_reference_data" )
{
    BOOST_ASSERT_MSG( !mangoAccountAddress.is_zero( ), "MangoReferenceDataClient expected non-zero mango account address" );

    asio::co_spawn( _strand, do_load_reference_data( mangoAccountAddress ), boost::asio::detached );
}

asio::awaitable< MangoReferenceData > MangoReferenceDataClientImpl::do_get_reference_data( )
{
    if ( _isInitialized )
    {
        // Reference data request already completed.
        co_return _mangoReferenceData;
    }

    boost::system::error_code errorCode;
    // Wait for reference data request to complete.
    co_await _expiryTimer.async_wait( boost::asio::redirect_error( boost::asio::use_awaitable, errorCode ) );

    if ( errorCode == asio::error::operation_aborted )
    {
        if ( _isInitialized )
        {
            // Reference data was successfully loaded.
            co_return _mangoReferenceData;
        }
        // Failed to load reference data.
        throw SynthfiError( "MangoReferenceDataClient unable to load reference data" );
    }
    else if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( _logger ) << fmt::format( "[{}] Timer error: {}", name( ), errorCode.what( ) );
        throw SynthfiError( "Timer error" );
    }

    throw SynthfiError( "Reference data request timed out" );
}

asio::awaitable< void > MangoReferenceDataClientImpl::do_load_reference_data( Core::PublicKey mangoAccountAddress )
{
    BOOST_ASSERT_MSG( _serumReferenceData.tradingPairs.empty( ), "MangoReferenceDataClient already loaded reference data" );

    SYNTHFI_LOG_DEBUG( _logger )
        << fmt::format
        (
            "[{}] Loading reference data, mango account address: {}",
            name( ),
            mangoAccountAddress
        );

    _serumReferenceData = co_await _serumReferenceDataClient.get_reference_data( );

    auto tradingPairCount = _serumReferenceData.tradingPairs.size( );
    auto currencyCount = _serumReferenceData.currencies.size( );

    _mangoReferenceData.tradingPairs.resize( tradingPairCount );
    _tradingPairMeasurements.resize( tradingPairCount );

    _mangoReferenceData.currencies.resize( currencyCount );
    _currencyMeasurements.resize( currencyCount );

    _mangoReferenceData.mangoAccountAddress = mangoAccountAddress;

    // Load mango account.
    Solana::GetAccountInfoRequest getMangoAccountRequest{ mangoAccountAddress };
    auto getMangoAccount = co_await _accountBatcher.get_account( std::move( getMangoAccountRequest ) );
    _mangoReferenceData.mangoAccount = Solana::account_to< MangoAccount >
    (
        std::move( getMangoAccount.accountInfo )
    );

    // Load mango group.
    Solana::GetAccountInfoRequest getMangoGroupRequest( { _mangoReferenceData.mangoAccount.mangoGroupAddress } );
    auto getMangoGroup = co_await _accountBatcher.get_account( std::move( getMangoGroupRequest ) );
    _mangoReferenceData.mangoGroup = Solana::account_to< MangoGroupAccount >
    (
        std::move( getMangoGroup.accountInfo )
    );

    // Load mango cache.
    Solana::GetAccountInfoRequest getMangoCacheRequest( { _mangoReferenceData.mangoGroup.mangoCache } );
    auto getMangoCache = co_await _accountBatcher.get_account( std::move( getMangoCacheRequest ) );
    _mangoReferenceData.mangoCache = Solana::account_to< MangoCacheAccount >
    (
        std::move( getMangoCache.accountInfo )
    );

    // Validate configured currencies are tradable on mango.
    std::vector< Core::PublicKey > rootBankAccounts( currencyCount );
    for ( size_t currencyIndex = 0; currencyIndex < currencyCount; ++currencyIndex )
    {
        const auto & currency = _serumReferenceData.currencies[ currencyIndex ];

        auto findTokenInfo = std::find_if
        (
            _mangoReferenceData.mangoGroup.tokens.begin( ),
            _mangoReferenceData.mangoGroup.tokens.end( ),
            [ &currency ]( const auto & tokenInfo )
            {
                return tokenInfo.mintAddress == currency.mintAddress;
            }
        );
        if ( findTokenInfo == _mangoReferenceData.mangoGroup.tokens.end( ) )
        {
            SYNTHFI_LOG_ERROR( _logger ) << "MangoGroup missing configured currency: " << currency.mintAddress;
            throw SynthfiError( "MangoGroup missing configured currency" );
        }

        _mangoReferenceData.currencies[ currencyIndex ].mangoCurrencyIndex = std::distance
        (
            _mangoReferenceData.mangoGroup.tokens.begin( ),
            findTokenInfo
        );

        rootBankAccounts[ currencyIndex ] = findTokenInfo->rootBankAddress;
    }

    // Load root bank accounts.
    Solana::GetMultipleAccountsRequest getRootBankAccounts( std::move( rootBankAccounts ) );
    auto rootBankAccountsResponse = co_await _accountBatcher.get_multiple_accounts( std::move( getRootBankAccounts ) );

    std::vector< Core::PublicKey > nodeBankAddresses( currencyCount );
    for ( size_t currencyIndex = 0; currencyIndex < currencyCount; ++currencyIndex )
    {
        _mangoReferenceData.currencies[ currencyIndex ].rootBank = Solana::account_to< Mango::RootBank >
        (
            std::move( rootBankAccountsResponse.accountInfos[ currencyIndex ] )
        );

        for ( const auto & nodeBank : _mangoReferenceData.currencies[ currencyIndex ].rootBank.nodeBanks )
        {
            if ( !nodeBank.is_zero( ) )
            {
                _mangoReferenceData.currencies[ currencyIndex ].nodeBankAddress = nodeBank;
                nodeBankAddresses[ currencyIndex ] =  nodeBank;
                break;
            }
        }
    }

    // Load open orders accounts;
    std::vector< Core::PublicKey > openOrdersAddresses( tradingPairCount );
    for ( size_t tradingPairIndex = 0; tradingPairIndex < tradingPairCount; ++tradingPairIndex )
    {
        auto mangoTradingPairIndex = _mangoReferenceData.tradingPairs[ tradingPairIndex ].mangoTradingPairIndex;

        const auto & openOrdersAddress = _mangoReferenceData.mangoAccount.openOrdersAddresses[ mangoTradingPairIndex ];
        if ( openOrdersAddress.is_zero( ) )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << fmt::format
                (
                    "[{}] MangoGroup missing open orders account, trading pair index: {}, mango trading pair index: {}",
                    name( ),
                    tradingPairIndex,
                    mangoTradingPairIndex
                );
            throw SynthfiError( "MangoGroup missing open orders address" );
        }
        openOrdersAddresses[ tradingPairIndex ] = openOrdersAddress;
    }
    Solana::GetMultipleAccountsRequest getOpenOrdersAddressesRequest{ std::move( openOrdersAddresses ) };
    auto getOpenOrdersAccountsResponse = co_await _accountBatcher.get_multiple_accounts( std::move( getOpenOrdersAddressesRequest ) );

    // Load node bank accounts.
    Solana::GetMultipleAccountsRequest getNodeBankAccounts{ nodeBankAddresses };
    auto nodeBankAccountsResponse = co_await _accountBatcher.get_multiple_accounts( std::move( getNodeBankAccounts ) );

    for ( size_t currencyIndex = 0; currencyIndex < currencyCount; ++currencyIndex )
    {
        auto & currency = _mangoReferenceData.currencies[ currencyIndex ];
        currency.nodeBank = Solana::account_to< Mango::NodeBank >
        (
            std::move( nodeBankAccountsResponse.accountInfos[ currencyIndex ] )
        );
        // Initialize currency measurements.
        auto & currencyMeasurement = _currencyMeasurements[ currencyIndex ];

        currencyMeasurement.measurementName = "currency";
        currencyMeasurement.tags =
        {
            { "source", "mango" },
            { "currency_index", std::to_string( currencyIndex ) }
        };
        currencyMeasurement.fields =
        {
            { "optimal_utilization", Statistics::InfluxDataType( currency.rootBank.optimal_utilization( ) ) },
            { "optimal_rate", Statistics::InfluxDataType( currency.rootBank.optimal_utilization( ) ) },
            { "maximum_rate", Statistics::InfluxDataType( currency.rootBank.maximum_rate( ) ) }
        };
    }

    // Validate configured trading pairs are tradable on mango.
    for ( size_t tradingPairIndex = 0; tradingPairIndex < tradingPairCount; ++tradingPairIndex )
    {
        auto & mangoTradingPair = _mangoReferenceData.tradingPairs[ tradingPairIndex ];

        const auto & serumMarketReferenceData = _serumReferenceData.tradingPairs[ tradingPairIndex ];
        auto findTradingPairInfo = std::find_if
        (
            _mangoReferenceData.mangoGroup.spotMarkets.begin( ),
            _mangoReferenceData.mangoGroup.spotMarkets.end( ),
            [ &serumMarketReferenceData ]( const auto & spotMarketInfo )
            {
                return serumMarketReferenceData.serumMarketAddress == spotMarketInfo.spotMarketAddress;
            }
        );
        if ( findTradingPairInfo == _mangoReferenceData.mangoGroup.spotMarkets.end( ) )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << fmt::format
                (
                    "[{}] MangoGroup missing configured currency: {}",
                    name( ),
                    serumMarketReferenceData.serumMarketAddress
                );
            throw SynthfiError( "MangoGroup missing configured trading pair" );
        }

        // Currency index to mango token index.
        mangoTradingPair.mangoTradingPairIndex = std::distance
        (
            _mangoReferenceData.mangoGroup.spotMarkets.begin( ),
            findTradingPairInfo
        );

        auto findBaseCurrencyInfo = std::find_if
        (
            _mangoReferenceData.mangoGroup.tokens.begin( ),
            _mangoReferenceData.mangoGroup.tokens.end( ),
            [ &serumMarketReferenceData ]( const auto & tokenInfo )
            {
                return serumMarketReferenceData.serumMarketAccount.baseMint == tokenInfo.mintAddress;
            }
        );
        if ( findBaseCurrencyInfo == _mangoReferenceData.mangoGroup.tokens.end( ) )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << fmt::format
                (
                    "[{}] MangoGroup missing token info for mint: {}",
                    name( ),
                    serumMarketReferenceData.serumMarketAccount.baseMint
                );
            throw SynthfiError( "MangoGroup missing base token" );
        }

        auto findQuoteCurrencyInfo = std::find_if
        (
            _mangoReferenceData.mangoGroup.tokens.begin( ),
            _mangoReferenceData.mangoGroup.tokens.end( ),
            [ &serumMarketReferenceData ]( const auto & tokenInfo )
            {
                return serumMarketReferenceData.serumMarketAccount.quoteMint == tokenInfo.mintAddress;
            }
        );
        if ( findQuoteCurrencyInfo == _mangoReferenceData.mangoGroup.tokens.end( ) )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << fmt::format
                (
                    "[{}] MangoGroup missing token info for mint: {}",
                    name( ),
                    serumMarketReferenceData.serumMarketAccount.quoteMint
                );
            throw SynthfiError( "MangoGroup missing quote token" );
        }

        // Currency index to mango market base/quote currency index.
        mangoTradingPair.mangoBaseCurrencyIndex = std::distance
        (
            _mangoReferenceData.mangoGroup.tokens.begin( ),
            findBaseCurrencyInfo
        );
        mangoTradingPair.mangoQuoteCurrencyIndex = std::distance
        (
            _mangoReferenceData.mangoGroup.tokens.begin( ),
            findQuoteCurrencyInfo
        );
        // Trading pair index to mango spot market index.
        mangoTradingPair.mangoTradingPairIndex = std::distance
        (
            _mangoReferenceData.mangoGroup.spotMarkets.begin( ),
            findTradingPairInfo
        );

        // Open orders account.
        mangoTradingPair.openOrdersAccount = Solana::account_to< Serum::SerumOpenOrdersAccount >
        (
            std::move( getOpenOrdersAccountsResponse.accountInfos[ tradingPairIndex ] )
        );
        // Initialize trading pair measurements.
        auto & tradingPairMeasurement = _tradingPairMeasurements[ tradingPairIndex ];

        tradingPairMeasurement.measurementName = "trading_pair";
        tradingPairMeasurement.tags =
        {
            { "source", "mango" },
            { "trading_pair_index", std::to_string( tradingPairIndex ) },
        };
        const auto & spotMarket = _mangoReferenceData.mangoGroup.spotMarkets[ mangoTradingPair.mangoTradingPairIndex ];
        tradingPairMeasurement.fields =
        {
            { "maintenance_asset_weight", Statistics::InfluxDataType( spotMarket.maintenance_asset_weight( ) ) },
            { "initial_asset_weight", Statistics::InfluxDataType( spotMarket.initial_asset_weight( ) ) },
            { "maintenance_liability_weight", Statistics::InfluxDataType( spotMarket.maintenance_liability_weight( ) ) },
            { "initial_liability_weight", Statistics::InfluxDataType( spotMarket.initial_liability_weight( ) ) },
            { "liquidation_fee", Statistics::InfluxDataType( spotMarket.liquidation_fee( ) ) }
       };
    }

    SYNTHFI_LOG_DEBUG( _logger ) << fmt::format( "[{}] Loaded reference data", name( ) );

    co_spawn( _strand, do_publish_statistics( ), boost::asio::detached );

    // Wake clients waiting on reference data.
    _isInitialized = true;
    _expiryTimer.cancel( );

    co_return;
}

boost::asio::awaitable< void > MangoReferenceDataClientImpl::do_publish_statistics( )
{
    // Publish currency and trading pair statistics.
    co_await
    (
        _statisticsPublisher.publish_statistics( _tradingPairMeasurements )
        && _statisticsPublisher.publish_statistics( _currencyMeasurements )
    );

    co_return;
}

} // namespace Mango
} // namespace Synthfi

#include "synthfi/Mango/MangoWalletClient/MangoWalletClientImpl.hpp"

#include "synthfi/Solana/SolanaHttpMessage.hpp"

#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/detached.hpp>

#include <boost/asio/experimental/awaitable_operators.hpp>

#include <fmt/core.h>
#include <fmt/format.h>

using namespace boost::asio::experimental::awaitable_operators;

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{
namespace Mango
{

MangoWalletClientImpl::MangoWalletClientImpl
(
    boost::asio::io_context & ioContext,
    Core::KeyStoreService & keyStoreService,
    Solana::SolanaHttpClientService & solanaHttpClientService,
    Solana::AccountSubscriberService & accountSubscriberService,
    Solana::SignatureSubscriberService & signatureSubscriberService,
    Serum::SerumReferenceDataClientService & serumReferenceDataClientService,
    MangoReferenceDataClientService & mangoReferenceDataClientService,
    Statistics::StatisticsPublisherService & statisticsPublisherService
)
    : _strand( ioContext.get_executor( ) )
    , _keyStore( keyStoreService )
    , _solanaHttpClient( solanaHttpClientService )
    , _accountSubscriber( accountSubscriberService )
    , _signatureSubscriber( signatureSubscriberService )
    , _serumReferenceDataClient( serumReferenceDataClientService )
    , _mangoReferenceDataClient( mangoReferenceDataClientService )
    , _statisticsPublisher( statisticsPublisherService, "mango_wallet" )
{ }

void MangoWalletClientImpl::on_mango_account_notification( uint64_t subscriptionId, Solana::AccountNotification accountNotification )
{
    co_spawn( _strand, do_on_mango_account_notification( subscriptionId, std::move( accountNotification ) ), boost::asio::detached );
}

boost::asio::awaitable< void > MangoWalletClientImpl::do_on_mango_account_notification( uint64_t subscriptionId, Solana::AccountNotification accountNotification )
{
    _mangoReferenceData.mangoAccount = Solana::account_to< MangoAccount >( std::move( accountNotification.accountInfo ) );

    co_return co_await do_update_positions( );
}

void MangoWalletClientImpl::on_mango_cache_account_notification( uint64_t subscriptionId, Solana::AccountNotification accountNotification )
{
    co_spawn( _strand, do_on_mango_cache_notification( subscriptionId, std::move( accountNotification ) ), boost::asio::detached );
}

boost::asio::awaitable< void > MangoWalletClientImpl::do_on_mango_cache_notification( uint64_t subscriptionId, Solana::AccountNotification accountNotification )
{
    _mangoReferenceData.mangoCache = Solana::account_to< MangoCacheAccount >
    (
        std::move( accountNotification.accountInfo )
    );

    co_return co_await do_update_positions( );
}

void MangoWalletClientImpl::on_open_orders_account_notification
(
    uint64_t subscriptionId,
    Solana::AccountNotification accountNotification,
    uint64_t tradingPairIndex
)
{
    co_spawn
    (
        _strand,
        do_on_open_orders_account_notification( subscriptionId, std::move( accountNotification ), tradingPairIndex ),
        boost::asio::detached
    );
}

boost::asio::awaitable< void > MangoWalletClientImpl::do_on_open_orders_account_notification
(
    uint64_t subscriptionId,
    Solana::AccountNotification accountNotification,
    uint64_t tradingPairIndex
)
{
    _mangoReferenceData.tradingPairs[ tradingPairIndex ].openOrdersAccount =
        Solana::account_to< Serum::SerumOpenOrdersAccount >( std::move( accountNotification.accountInfo ) );

    co_return co_await do_update_positions( );

    co_return;
}

boost::asio::awaitable< void > MangoWalletClientImpl::do_update_positions( )
{
    SYNTHFI_LOG_TRACE( _logger ) << fmt::format( "[{}] Updating mango wallet", name( ) );

    // Update positions.
    _mangoReferenceData.get_position_component( _wallet.positions );

    // Calculate health.
    Trading::Quantity quoteComponent;
    std::vector< Trading::Quantity > spotComponent( _wallet.marginAvailable.size( ) );
    std::vector< Trading::Price > priceComponent( _wallet.marginAvailable.size( ) );
    std::vector< Trading::Price > weightComponent( _wallet.marginAvailable.size( ) );
    _mangoReferenceData.get_health_components( quoteComponent, spotComponent, priceComponent, weightComponent, _wallet.positions );

    SYNTHFI_LOG_ERROR( _logger )
    << fmt::format
    (
        "quote: {}, spot: {}, price: {}, weight: {}",
        quoteComponent,
        spotComponent,
        priceComponent,
        weightComponent
    );

    // Update margin available.
    auto health = _mangoReferenceData.get_health_from_components( quoteComponent, spotComponent, priceComponent, weightComponent );

    if ( health <= Trading::Price( 0 ) )
    {
        std::fill
        (
            _wallet.marginAvailable.begin( ),
            _wallet.marginAvailable.end( ),
            Trading::Quantity( 0 )
        );
    }
    else
    {
        for ( size_t tradingPairIndex = 0; tradingPairIndex < _wallet.marginAvailable.size( ); ++tradingPairIndex )
        {
            BOOST_ASSERT_MSG( weightComponent[ tradingPairIndex ] != Trading::Price( 1 ), "Mango weight divide-by-zero" );

            _wallet.marginAvailable[ tradingPairIndex ] = health / ( Trading::Price( 1 ) - weightComponent[ tradingPairIndex ] );
        }
    }

    // Notify wallet subscribers.
    _onWallet( _wallet );

    // Update position statistic.
    for ( size_t currencyIndex = 0; currencyIndex < _wallet.positions.size( ); ++currencyIndex )
    {
        _positionMeasurements[ currencyIndex ].fields.front( ).second
            = Statistics::InfluxDataType( _wallet.positions[ currencyIndex ] );
    }

    // Update margin available statistic.
    for ( size_t tradingPairIndex = 0; tradingPairIndex < _wallet.marginAvailable.size( ); ++tradingPairIndex )
    {
        _marginAvailableMeasurements[ tradingPairIndex ].fields.front( ).second
            = Statistics::InfluxDataType( _wallet.marginAvailable[ tradingPairIndex ] );
    }

    // Publish wallet statistics.
    co_await
    (
        _statisticsPublisher.publish_statistics( _positionMeasurements )
        && _statisticsPublisher.publish_statistics( _marginAvailableMeasurements )
    );

    SYNTHFI_LOG_INFO( _logger )
        << fmt::format
        (
            "[{}] Updated positions: {}, margin available: {}",
            name( ),
            _wallet.positions,
            _wallet.marginAvailable
        );

    co_return;
}

asio::awaitable< void > MangoWalletClientImpl::do_subscribe_wallet
(
    std::function< void( Trading::Wallet ) > onWallet
)
{
    SYNTHFI_LOG_INFO( _logger ) << fmt::format( "[{}] Subscribing wallet", name( ) );

    // Load reference.
    auto [ serumReferenceData, mangoReferenceData ] = co_await
    (
        _serumReferenceDataClient.get_reference_data( )
        && _mangoReferenceDataClient.get_reference_data( )
    );
    _serumReferenceData = std::move( serumReferenceData );
    _mangoReferenceData = std::move( mangoReferenceData );

    // Initialize wallet state.
    const auto currencyCount = _serumReferenceData.currencies.size( );
    const auto tradingPairCount = _serumReferenceData.tradingPairs.size( );
    _wallet.positions.resize( currencyCount );
    _wallet.marginAvailable.resize( tradingPairCount );

    // Initialize statistics initial state.
    _positionMeasurements.resize( currencyCount );
    for ( size_t currencyIndex = 0; currencyIndex < currencyCount; ++currencyIndex )
    {
        _positionMeasurements[ currencyIndex ].measurementName = "wallet";
        _positionMeasurements[ currencyIndex ].tags.emplace_back( "source", "mango" );
        _positionMeasurements[ currencyIndex ].tags.emplace_back( "currency_index", std::to_string( currencyIndex ) );
        _positionMeasurements[ currencyIndex ].fields.emplace_back( "position", Trading::Quantity( 0 ) );
    }

    _marginAvailableMeasurements.resize( tradingPairCount );
    for ( size_t tradingPairIndex = 0; tradingPairIndex < tradingPairCount; ++tradingPairIndex )
    {
        _marginAvailableMeasurements[ tradingPairIndex ].measurementName = "wallet";
        _marginAvailableMeasurements[ tradingPairIndex ].tags.emplace_back( "source", "mango" );
        _marginAvailableMeasurements[ tradingPairIndex ].tags.emplace_back( "trading_pair_index", std::to_string( tradingPairIndex ) );
        _marginAvailableMeasurements[ tradingPairIndex ].fields.emplace_back( "margin_available", Trading::Quantity( 0 ) );
    }

    // Initialize wallet callback.
    _onWallet = std::move( onWallet );

    // Update wallet with initial reference data.
    co_await do_update_positions( );

    co_await
    (
        // Subscribe to mango account.
        _accountSubscriber.account_subscribe
        (
            Solana::AccountSubscribe{ _mangoReferenceData.mangoAccountAddress, Solana::Commitment::processed },
            std::bind
            (
                &MangoWalletClientImpl::on_mango_account_notification,
                this,
                std::placeholders::_1,
                std::placeholders::_2
            )
        )
        // Subscribe to mango cache.
        && _accountSubscriber.account_subscribe
        (
            Solana::AccountSubscribe{ _mangoReferenceData.mangoGroup.mangoCache, Solana::Commitment::processed },
            std::bind
            (
                &MangoWalletClientImpl::on_mango_cache_account_notification,
                this,
                std::placeholders::_1,
                std::placeholders::_2
            )
        )
    );

    // Subscribe to open orders accounts.
    for ( size_t tradingPairIndex = 0; tradingPairIndex < tradingPairCount; ++tradingPairIndex)
    {
        const auto & mangoTradingPairIndex = _mangoReferenceData.tradingPairs[ tradingPairIndex ].mangoTradingPairIndex;
        co_await _accountSubscriber.account_subscribe
        (
            Solana::AccountSubscribe
            {
                _mangoReferenceData.mangoAccount.openOrdersAddresses[ mangoTradingPairIndex ],
                Solana::Commitment::processed
            },
            std::bind
            (
                &MangoWalletClientImpl::on_open_orders_account_notification,
                this,
                std::placeholders::_1,
                std::placeholders::_2,
                tradingPairIndex
            )
        );
    }

    SYNTHFI_LOG_INFO( _logger ) << fmt::format( "[{}] Done subscribing wallet", name( ) );

    co_return;
}

} // namespace Mango
} // namespace Synthfi

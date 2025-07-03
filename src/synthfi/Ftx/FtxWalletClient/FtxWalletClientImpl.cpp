#include "synthfi/Ftx/FtxWalletClient/FtxWalletClientImpl.hpp"

#include "synthfi/Ftx/FtxRestMessage.hpp"

#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

using namespace boost::asio::experimental::awaitable_operators;
using namespace std::chrono_literals;

namespace asio = boost::asio;
namespace beast = boost::beast;

static std::chrono::milliseconds ftx_wallet_refresh_interval( ) { return 1000ms; }

namespace Synthfi
{
namespace Ftx
{

FtxWalletClientImpl::FtxWalletClientImpl
(
    boost::asio::io_context & ioContext,
    Ftx::FtxRestClientService & ftxRestClientService,
    Ftx::FtxReferenceDataClientService & ftxReferenceDataClientService,
    Statistics::StatisticsPublisherService & statisticsPublisherService
)
    : _strand( ioContext.get_executor( ) )
    , _ftxRestClient( ftxRestClientService )
    , _ftxReferenceDataClient( ftxReferenceDataClientService )
    , _statisticsPublisher( statisticsPublisherService, "ftx_wallet" )
    , _updatePositionsTimer( _strand )
{ }

asio::awaitable< void > FtxWalletClientImpl::do_update_positions( )
{
    boost::system::error_code errorCode;

    do
    {
        SYNTHFI_LOG_TRACE( _logger ) << fmt::format( "[{}] Updating ftx wallet", name( ) );

        auto balancesResponse = co_await _ftxRestClient.send_request< Ftx::GetBalancesRequest, Ftx::GetBalancesResponse >( { } );

        for ( size_t currencyIndex = 0; currencyIndex < _referenceData.currencies.size( ); ++currencyIndex )
        {
            const auto & currency = _referenceData.currencies[ currencyIndex ];
            const auto & findBalance = std::find_if
            (
                balancesResponse.balances.begin( ),
                balancesResponse.balances.end( ),
                [ &currency ]( const auto & balance )
                {
                    return currency.currencyName == balance.coin;
                }
            );

            findBalance == balancesResponse.balances.end( )
                ? _wallet.positions[ currencyIndex ] = Trading::Quantity( 0 )
                : _wallet.positions[ currencyIndex ] = findBalance->free;
        }

        // USD balance is the total available quote currency - FTX doesn't support margin :(
        std::fill
        (
            _wallet.marginAvailable.begin( ),
            _wallet.marginAvailable.end( ),
            _wallet.positions[ _quoteCurrencyIndex ]
        );

        // Notify wallet subscribers.
        _onWallet( _wallet );

        // Initialize position statistics.
        for ( size_t currencyIndex = 0; currencyIndex < _wallet.positions.size( ); ++currencyIndex )
        {
            _positionMeasurements[ currencyIndex ].fields.front( ).second = Statistics::InfluxDataType( _wallet.positions[ currencyIndex ] );
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

        SYNTHFI_LOG_TRACE( _logger ) << fmt::format( "[{}] Done updating ftx wallet, next update in: {}", name( ), ftx_wallet_refresh_interval( ) );

        _updatePositionsTimer.expires_from_now( ftx_wallet_refresh_interval( ) );
        co_await _updatePositionsTimer.async_wait( asio::redirect_error( asio::use_awaitable, errorCode ) );
    }
    while( !errorCode );

    SYNTHFI_LOG_TRACE( _logger ) << fmt::format( "[{}] Ftx wallet update timer error: {}", name( ), errorCode.what( ) );
    co_return;
}

asio::awaitable< void > FtxWalletClientImpl::do_subscribe_wallet
(
    std::function< void( Trading::Wallet ) > onWallet
)
{
    SYNTHFI_LOG_INFO( _logger ) << fmt::format( "[{}] Subscribing wallet", name( ) );

    // Load reference data.
    _referenceData = co_await _ftxReferenceDataClient.get_reference_data( );

    const auto currencyCount = _referenceData.currencies.size( );
    const auto tradingPairCount = _referenceData.tradingPairs.size( );

    // Initialize wallet state.
    _wallet.positions.resize( currencyCount );
    _wallet.marginAvailable.resize( tradingPairCount );

    // Initialize statistics state.
    _positionMeasurements.resize( currencyCount );
    for ( size_t currencyIndex = 0; currencyIndex < currencyCount; ++currencyIndex )
    {
        _positionMeasurements[ currencyIndex ].measurementName = "wallet";
        _positionMeasurements[ currencyIndex ].tags.emplace_back( "source", "ftx" );
        _positionMeasurements[ currencyIndex ].tags.emplace_back( "currency_index", std::to_string( currencyIndex ) );
        _positionMeasurements[ currencyIndex ].fields.emplace_back( "position", Trading::Quantity( 0 ) );

        // USD is FTX's quote currency.
        if ( _referenceData.currencies[ currencyIndex ].currencyName == "USD" )
        {
            _quoteCurrencyIndex = currencyIndex;
        }
    }

    if ( _quoteCurrencyIndex == std::numeric_limits< uint64_t >::max( ) )
    {
        SYNTHFI_LOG_ERROR( _logger ) << fmt::format( "[{}] Missing quote currency", name( ) );

        throw SynthfiError( "Missing quote currency" );
    }

    _marginAvailableMeasurements.resize( tradingPairCount );
    for ( size_t tradingPairIndex = 0; tradingPairIndex < tradingPairCount; ++tradingPairIndex )
    {
        _marginAvailableMeasurements[ tradingPairIndex ].measurementName = "wallet";
        _marginAvailableMeasurements[ tradingPairIndex ].tags.emplace_back( "source", "ftx" );
        _marginAvailableMeasurements[ tradingPairIndex ].tags.emplace_back( "trading_pair_index", std::to_string( tradingPairIndex ) );
        _marginAvailableMeasurements[ tradingPairIndex ].fields.emplace_back( "margin_available", Trading::Quantity( 0 ) );
    }

    _onWallet = std::move( onWallet );

    co_spawn( _strand, do_update_positions( ), asio::detached );

    SYNTHFI_LOG_INFO( _logger ) << fmt::format( "[{}] Done subscribing wallet", name( ) );

    co_return;
}

} // namespace Ftx
} // namespace Synthfi

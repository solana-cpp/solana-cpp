#include "synthfi/Serum/SerumMarketDataClient/SerumMarketDataClientImpl.hpp"

#include "synthfi/Solana/SolanaHttpMessage.hpp"

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

SerumMarketDataClientImpl::SerumMarketDataClientImpl
(
    boost::asio::io_context & ioContext,
    Solana::AccountBatcherService & solanaAccountBatcher,
    Solana::AccountSubscriberService & solanaAccountSubscriber,
    Serum::SerumReferenceDataClientService & serumReferenceDataClientService,
    Statistics::StatisticsPublisherService & statisticsPublisherService
)
    : _strand( ioContext.get_executor( ) )
    , _expiryTimer( _strand, 30s )
    , _accountBatcher( solanaAccountBatcher )
    , _accountSubscriber( solanaAccountSubscriber )
    , _referenceDataClient( serumReferenceDataClientService )
    , _statisticsPublisher( statisticsPublisherService, "serum_market_data" )
{
    asio::co_spawn
    (
        _strand,
        do_load_market_data( ),
        boost::asio::detached
    );
}

asio::awaitable< void > SerumMarketDataClientImpl::do_load_market_data( )
{
    BOOST_ASSERT_MSG( !_isInitialized, "SerumMarketDataClient already loaded markets" );

    SYNTHFI_LOG_ERROR( _logger ) << fmt::format( "[{}] Loading serum market data", name( ) );

    _referenceData = co_await _referenceDataClient.get_reference_data( );

    auto tradingPairCount = _referenceData.tradingPairs.size( );

    _books.resize( tradingPairCount );
    _eventQueueSequenceNumbers.resize( tradingPairCount, 0 );
    _bookMeasurements.resize( tradingPairCount );

    Solana::GetMultipleAccountsRequest getOrderbookAccounts{ std::vector< Core::PublicKey >( tradingPairCount * 2 ) };
    for ( size_t i = 0; i < tradingPairCount; ++i )
    {
        getOrderbookAccounts.accountPublicKeys[ i ] =
            _referenceData.tradingPairs[ i ].serumMarketAccount.bids;
        getOrderbookAccounts.accountPublicKeys[ i + tradingPairCount ] =
            _referenceData.tradingPairs[ i ].serumMarketAccount.asks;
    }
    auto orderbooksAccounts = co_await _accountBatcher.get_multiple_accounts( std::move( getOrderbookAccounts ) );

    // Get initial orderbook state.
    for ( size_t i = 0; i < tradingPairCount; ++i )
    {
        co_await 
        (
            // Update bids, asks book empty - don't publish.
            do_update_book
            (
                i,
                Solana::account_to< SerumOrderbookAccount >( std::move( orderbooksAccounts.accountInfos[ i ] ) ),
                false
            )
            // Update asks, bids book updated, do publish.
            && do_update_book
            (
                i,
                Solana::account_to< SerumOrderbookAccount >
                (
                    std::move( orderbooksAccounts.accountInfos[ i + tradingPairCount ] )
                ),
                true
            )
        );
    }

    // Subscribe to orderbook updates.
    for ( size_t i = 0; i < _referenceData.tradingPairs.size( ); ++i )
    {
        co_await
        (
            _accountSubscriber.account_subscribe
            (
                Solana::AccountSubscribe
                {
                    _referenceData.tradingPairs[ i ].serumMarketAccount.bids,
                    Solana::Commitment::processed
                },
                std::bind( &SerumMarketDataClientImpl::on_orderbook_account, this, i, std::placeholders::_1, std::placeholders::_2 )
            )
            && _accountSubscriber.account_subscribe
            (
                Solana::AccountSubscribe
                {
                    _referenceData.tradingPairs[ i ].serumMarketAccount.asks,
                    Solana::Commitment::processed
                },
                std::bind( &SerumMarketDataClientImpl::on_orderbook_account, this, i, std::placeholders::_1, std::placeholders::_2 )
            )
            && _accountSubscriber.account_subscribe
            (
                Solana::AccountSubscribe
                {
                    _referenceData.tradingPairs[ i ].serumMarketAccount.eventQueue,
                    Solana::Commitment::processed
                },
                std::bind( &SerumMarketDataClientImpl::on_event_queue_account, this, i, std::placeholders::_1, std::placeholders::_2 )
            )
        );
    }

    // Wake clients waiting on subscription.
    _isInitialized = true;
    _expiryTimer.cancel( );

    SYNTHFI_LOG_ERROR( _logger ) << fmt::format( "[{}] Done serum market data", name( ) );

    co_return;
}

void SerumMarketDataClientImpl::on_orderbook_account( uint64_t tradingPairIndex, uint64_t subscriptionId, Solana::AccountNotification notification )
{
    co_spawn( _strand, do_on_orderbook_account( tradingPairIndex, std::move( notification ) ), boost::asio::detached );
}

void SerumMarketDataClientImpl::on_event_queue_account( uint64_t tradingPairIndex, uint64_t subscriptionId, Solana::AccountNotification notification )
{
    co_spawn( _strand, do_on_event_queue_account( tradingPairIndex, std::move( notification ) ), boost::asio::detached );
}

asio::awaitable< void > SerumMarketDataClientImpl::do_on_orderbook_account( uint64_t tradingPairIndex, Solana::AccountNotification notification )
{
    try
    {
        auto orderbookAccount = Solana::account_to< SerumOrderbookAccount >( std::move( notification.accountInfo ) );
        co_await do_update_book( tradingPairIndex, std::move( orderbookAccount ), true );
    }
    catch ( const std::exception & ex )
    {
        SYNTHFI_LOG_ERROR( _logger )
            << fmt::format( "[{}] Error deserializing serum orderbook account: {}", name( ), ex.what( ) );
    }

    co_return;
}

asio::awaitable< void > SerumMarketDataClientImpl::do_on_event_queue_account( uint64_t tradingPairIndex, Solana::AccountNotification notification )
{
    try
    {
        auto eventQueueAccount = Solana::account_to< SerumEventQueueAccount >( std::move( notification.accountInfo ) );

        auto unprocessedEventCount = ( _eventQueueSequenceNumbers[ tradingPairIndex ] == 0 )
            ? eventQueueAccount.events.size( )
            : eventQueueAccount.sequenceNumber - _eventQueueSequenceNumbers[ tradingPairIndex ] == 0;

        if ( unprocessedEventCount > eventQueueAccount.events.size( ) )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << fmt::format
                (
                    "[{}] Missed events: {}, event count: {}",
                    name( ),
                    unprocessedEventCount - eventQueueAccount.events.size( ),
                    eventQueueAccount.events.size( )
                );
        }
        else
        {
            SYNTHFI_LOG_TRACE( _logger )
                << fmt::format
                (
                    "[{}] Processing events: {}, event count: {}",
                    name( ),
                    unprocessedEventCount - eventQueueAccount.events.size( ),
                    eventQueueAccount.events.size( )
                );
        }

        for ( size_t i = 0; i < std::min( eventQueueAccount.events.size( ), unprocessedEventCount ); ++i )
        {
            co_await do_on_event( tradingPairIndex, std::move( eventQueueAccount.events[ i ] ) );
        }

        _eventQueueSequenceNumbers[ tradingPairIndex ] = eventQueueAccount.sequenceNumber;
    }
    catch ( const std::exception & ex )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Error deserializing serum event queue account: " << ex.what( );
    }

    co_return;
}

boost::asio::awaitable< void > SerumMarketDataClientImpl::do_on_event( uint64_t tradingPairIndex, Serum::SerumEvent event )
{
    SYNTHFI_LOG_INFO( _logger ) << fmt::format( "[{}] Proccessing event: {}", name( ), event );

    co_return;
}

asio::awaitable< void > SerumMarketDataClientImpl::do_orderbook_subscribe
(
    std::function< void( uint64_t, Trading::Book ) > onBook
)
{
    SYNTHFI_LOG_DEBUG( _logger ) << fmt::format( "[{}] Subscribing to serum orderbook", name( ) );

    auto onSuccess = [ this, onBook = std::move( onBook ) ]
    {
        _onBooks.emplace_back( onBook );
    };

    if ( _isInitialized )
    {
        onSuccess( );
        co_return;
    }

    // Wait for orderbook subscription request to complete.
    boost::system::error_code errorCode;

    co_await _expiryTimer.async_wait( boost::asio::redirect_error( boost::asio::use_awaitable, errorCode ) );
    if ( errorCode == asio::error::operation_aborted )
    {
        if ( _isInitialized )
        {
            // Orderbooks are successfully loaded.
            onSuccess( );
            co_return;
        }
        // Failed to load markets.
        throw SynthfiError( "SerumMarketDataClient unable to subscribe to orderbook" );
    }
    else if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( _logger ) << fmt::format( "[{}] Timer error: {}", name( ), errorCode.what( ) );
        throw SynthfiError( "Timer error" );
    }
    throw SynthfiError( "Orderbook subscription request timed out" );
}

asio::awaitable< void > SerumMarketDataClientImpl::do_orders_subscribe
(
    std::function< void( uint64_t, Trading::Order ) > onOrder
)
{
    auto onSuccess = [ this, onOrder = std::move( onOrder ) ]
    {
        _onOrders.emplace_back( onOrder );
    };

    if ( _isInitialized )
    {
        onSuccess( );
        co_return;
    }

    // Wait for orderbook subscription request to complete.
    boost::system::error_code errorCode;

    co_await _expiryTimer.async_wait( boost::asio::redirect_error( boost::asio::use_awaitable, errorCode ) );
    if ( errorCode == asio::error::operation_aborted )
    {
        if ( _isInitialized )
        {
            // Orderbooks are successfully loaded.
            onSuccess( );
            co_return;
        }
        // Failed to load markets.
        throw SynthfiError( "SerumMarketDataClient unable to subscribe to orders" );
    }
    else if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( _logger ) << fmt::format( "[{}] Timer error: {}", name( ), errorCode.what( ) );
        throw SynthfiError( "Timer error" );
    }
    throw SynthfiError( "Orders subscription request timed out" );
}

asio::awaitable< void > SerumMarketDataClientImpl::do_update_book
(
    uint64_t tradingPairIndex,
    SerumOrderbookAccount serumOrderbookAccount,
    bool doPublish
)
{
    auto now = _clock.now( );

    std::vector< Trading::PriceQuantity > allOrders( serumOrderbookAccount.orders.size( ) );

    const auto & tradingPair = _referenceData.tradingPairs[ tradingPairIndex ];
    const auto & baseMint = _referenceData.currencies[ tradingPair.baseCurrencyIndex ].mintAccount;
    const auto & quoteMint = _referenceData.currencies[ tradingPair.quoteCurrencyIndex ].mintAccount;

    // Convert serum-order to synthfi-order.
    std::transform
    (
        serumOrderbookAccount.orders.begin( ),
        serumOrderbookAccount.orders.end( ),
        allOrders.begin( ),
        [ &tradingPair, &baseMint, &quoteMint ]( const auto & order ) -> Trading::PriceQuantity
        {
            Trading::Price price =
                ( ( static_cast< Trading::Price >( order.key.limitPrice ) * tradingPair.serumMarketAccount.quoteLotSize ) *
                boost::multiprecision::pow( Trading::Price( 10 ), baseMint.decimals ) ) /
                ( boost::multiprecision::pow( Trading::Price( 10 ), quoteMint.decimals ) * tradingPair.serumMarketAccount.baseLotSize );
            Trading::Quantity quantity =
                ( static_cast< Trading::Quantity >( order.quantity ) * tradingPair.serumMarketAccount.baseLotSize )
                    / boost::multiprecision::pow( Trading::Price( 10 ), baseMint.decimals );
            return { price, quantity };
        }
    );

    // Sort orders by price.
    serumOrderbookAccount.side == Trading::Side::bid
        ? std::sort
        (
            allOrders.begin( ),
            allOrders.end( ),
            [ ]( const auto & lhs, const auto & rhs ) { return lhs.price( ) > rhs.price( ); }
        )
        : std::sort
        (
            allOrders.begin( ),
            allOrders.end( ),
            [ ]( const auto & lhs, const auto & rhs ) { return lhs.price( ) < rhs.price( ); }
        );

    // Update book time.
    _books[ tradingPairIndex ].receive_timestamp( ) = now;
    _books[ tradingPairIndex ].exchange_timestamp( ) = now; // Until we come up with a better solution, exchange time is receive time.

    // Merge orders at the same price level.
    auto & sideBook = _books[ tradingPairIndex ].get( serumOrderbookAccount.side ); // Reuse vector.
    sideBook.clear( );
    for ( const auto & order : allOrders )
    {
        sideBook.empty( ) || sideBook.back( ).price( ) != order.price( )
            ? sideBook.emplace_back( order )
            : sideBook.back( ) += order;
    }

    SYNTHFI_LOG_DEBUG( _logger ) << fmt::format( "Book: {}", _books[ tradingPairIndex ] );

    // Notify subscriber of new book.
    if ( doPublish )
    {
        for ( const auto & onBook : _onBooks )
        {
            onBook( tradingPairIndex, _books[ tradingPairIndex ] );
        }
    }

    co_await do_publish_statistics( tradingPairIndex, serumOrderbookAccount.side );

    co_return;
}

boost::asio::awaitable< void > SerumMarketDataClientImpl::do_publish_statistics
(
    uint64_t tradingPairIndex,
    Trading::Side side
)
{
    const auto & sideBook = _books[ tradingPairIndex ].get( side );
    auto & bookMeasurement = _bookMeasurements[ tradingPairIndex ].get( side );
    bookMeasurement.resize( sideBook.size( ) );

    // Update bid measurements.
    for ( size_t levelIndex = 0; levelIndex < sideBook.size( ); ++levelIndex )
    {
        const auto & [ price, quantity ] = sideBook[ levelIndex ];
        auto & measurement = bookMeasurement[ levelIndex ];
        measurement.measurementName = "order_book";
        measurement.tags = 
            {
                { "source", "serum" },
                { "side", std::string( magic_enum::enum_name( side ) ) },
                { "level_index", std::to_string( levelIndex ) },
                { "trading_pair_index", std::to_string( tradingPairIndex ) }
            };
        measurement.fields =
            {
                { "price", Statistics::InfluxDataType( price ) },
                { "quantity", Statistics::InfluxDataType( quantity ) }
            };
    };

    // Publish bid and ask measurements at the same timestamp.
    co_await _statisticsPublisher.publish_statistics( bookMeasurement );

    co_return;
}

} // namespace Serum
} // namespace Synthfi

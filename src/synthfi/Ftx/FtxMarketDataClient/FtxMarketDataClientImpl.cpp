#include "synthfi/Ftx/FtxMarketDataClient/FtxMarketDataClientImpl.hpp"

#include "synthfi/Ftx/FtxUtils.hpp"

#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{
namespace Ftx
{

FtxMarketDataClientImpl::FtxMarketDataClientImpl
(
    boost::asio::io_context & ioContext,
    FtxReferenceDataClientService & ftxReferenceDataClientService,
    FtxWsClientService & ftxWsClientService,
    Statistics::StatisticsPublisherService & statisticsPublisherService
)
    : _strand( ioContext.get_executor( ) )
    , _wsClient( ftxWsClientService )
    , _referenceDataClient( ftxReferenceDataClientService )
    , _statisticsPublisher( statisticsPublisherService, "ftx_market_data" )
{ }

void FtxMarketDataClientImpl::on_orderbook_snapshot( uint64_t tradingPairIndex, FtxOrderbookMessage orderbookSnapshot )
{
    co_spawn( _strand, do_on_orderbook_snapshot( tradingPairIndex, std::move( orderbookSnapshot) ), boost::asio::detached );
}

void FtxMarketDataClientImpl::on_orderbook_update( uint64_t tradingPairIndex, FtxOrderbookMessage orderbookUpdate )
{
    co_spawn( _strand, do_on_orderbook_update( tradingPairIndex, std::move( orderbookUpdate ) ), boost::asio::detached );
}

asio::awaitable< void > FtxMarketDataClientImpl::do_orderbook_subscribe
(
    std::function< void( uint64_t, Trading::Book ) > onBook
)
{
    SYNTHFI_LOG_DEBUG( _logger ) << fmt::format( "[{}] Subscribing orderbooks", name( ) );

    _referenceData = co_await _referenceDataClient.get_reference_data( );

    auto tradingPairSize = _referenceData.tradingPairs.size( );
    _books.resize( tradingPairSize );
    _bookMeasurements.resize( tradingPairSize );

    // Callback to notify wallet subscribers.
    _onBook = std::move( onBook );

    for ( size_t i = 0; i < _referenceData.tradingPairs.size( ); ++i )
    {
        co_await _wsClient.subscribe_orderbook
        (
            _referenceData.tradingPairs[ i ].marketInformation.name,
            std::bind
            (
                &FtxMarketDataClientImpl::on_orderbook_snapshot,
                this,
                i,
                std::placeholders::_1
            ),
            std::bind
            (
                &FtxMarketDataClientImpl::on_orderbook_update,
                this,
                i,
                std::placeholders::_1
            )
        );
    }

    SYNTHFI_LOG_DEBUG( _logger ) << fmt::format( "[{}] Done subscribing orderbooks", name( ) );

    co_return;
}

boost::asio::awaitable< void > FtxMarketDataClientImpl::do_on_orderbook_snapshot
(
    uint64_t tradingPairIndex,
    FtxOrderbookMessage ftxOrderbookMessage
)
{
    auto now = _clock.now( );

    auto & book = _books[ tradingPairIndex ];

    book.receive_timestamp( ) = now;
    book.exchange_timestamp( ) = std::chrono::time_point( ftxOrderbookMessage.time );

    book.get( Trading::Side::bid ) = ftxOrderbookMessage.levels.get( Trading::Side::bid );
    book.get( Trading::Side::ask ) = ftxOrderbookMessage.levels.get( Trading::Side::ask );

    // TODO: only check in debug mode.
    auto bookChecksum = ftx_orderbook_checksum( book );
    if ( bookChecksum != ftxOrderbookMessage.checksum )
    {
        SYNTHFI_LOG_ERROR( _logger )
            << fmt::format
            (
                "[{}] Invalid orderbook snapshot, expected checksum: {}, checksum: {}",
                name( ),
                ftxOrderbookMessage.checksum,
                bookChecksum
            );

        co_return;
    }

    _onBook( tradingPairIndex, book );

    co_return co_await do_publish_statistics( tradingPairIndex );
}

template< Trading::Side SideValue >
static void update_side( const FtxOrderbookMessage & ftxOrderbookMessage, Trading::Book & book )
{
    auto & bookSide = book.get( SideValue );

    auto bookSideIt = bookSide.begin( );
    for ( const auto & [ priceString, quantityString ] : ftxOrderbookMessage.levels.get( SideValue ) )
    {
        Trading::Price price( priceString );
        Trading::Quantity quantity( quantityString );

        bookSideIt = std::lower_bound
        (
            bookSideIt,
            book.get( SideValue ).end( ),
            price,
            [ &price ]( const Trading::PriceQuantity & priceQuantity, const Trading::Price & findPrice )
            {
                return SideValue == Trading::Side::bid ? priceQuantity.price( ) > findPrice : priceQuantity.price( ) < findPrice;
            }
        );

        // Add new level to end of book.
        if ( bookSideIt == bookSide.end( ) )
        {
            BOOST_ASSERT_MSG( quantity != 0, "Received level update to end of book with zero qty" );

            bookSideIt = bookSide.emplace( bookSideIt, price, quantity );
        }
        // Update existing level.
        else if ( bookSideIt->price( ) == price )
        {
            // Delete level.
            if ( quantity == 0 )
            {
                bookSideIt = bookSide.erase( bookSideIt );
            }
            // Update quantity.
            else
            {
                bookSideIt->quantity( ) = quantity;
            }
        }
        // Insert new level.
        else
        {
            bookSideIt = bookSide.insert( bookSideIt, { price, quantity } );
        }
    }
};

boost::asio::awaitable< void > FtxMarketDataClientImpl::do_on_orderbook_update
(
    uint64_t tradingPairIndex,
    FtxOrderbookMessage ftxOrderbookMessage
)
{
    auto now = _clock.now( );

    auto & book = _books[ tradingPairIndex ];

    book.receive_timestamp( ) = now;
    book.exchange_timestamp( ) = std::chrono::time_point( ftxOrderbookMessage.time );

    update_side< Trading::Side::bid >( ftxOrderbookMessage, book );
    update_side< Trading::Side::ask >( ftxOrderbookMessage, book );

    // TODO: only check in debug mode.
    auto bookChecksum = ftx_orderbook_checksum( book );
    if ( bookChecksum != ftxOrderbookMessage.checksum )
    {
        SYNTHFI_LOG_ERROR( _logger )
            << fmt::format
            (
                "[{}] Invalid orderbook snapshot, expected checksum: {}, checksum: {}",
                name( ),
                ftxOrderbookMessage.checksum,
                bookChecksum
            );

        co_return;
    }

    _onBook( tradingPairIndex, book );

    co_return co_await do_publish_statistics( tradingPairIndex );
}

boost::asio::awaitable< void > FtxMarketDataClientImpl::do_publish_statistics( uint64_t tradingPairIndex )
{
    const auto & bids = _books[ tradingPairIndex ].get( Trading::Side::bid );
    const auto & asks = _books[ tradingPairIndex ].get( Trading::Side::ask );

    auto & bookMeasurement = _bookMeasurements[ tradingPairIndex ];
    bookMeasurement.resize( bids.size( ) + asks.size( ) );

    // Update bid measurements.
    for ( size_t levelIndex = 0; levelIndex < bids.size( ); ++levelIndex )
    {
        const auto & [ price, quantity ] = bids[ levelIndex ];
        auto & measurement = bookMeasurement[ levelIndex ];
        measurement.measurementName = "order_book";
        measurement.tags = 
            {
                { "source", "ftx" },
                { "side", "bid" },
                { "level_index", std::to_string( levelIndex ) },
                { "trading_pair_index", std::to_string( tradingPairIndex ) }
            };
        measurement.fields =
            {
                { "price", Statistics::InfluxDataType( price ) },
                { "quantity", Statistics::InfluxDataType( quantity ) }
            };
    };

    // Update ask measurements.
    for ( size_t levelIndex = 0; levelIndex < asks.size( );++levelIndex )
    {
        const auto & [ price, quantity ] = asks[ levelIndex ];
        auto & measurement = bookMeasurement[ levelIndex + bids.size( ) ];
        measurement.measurementName = "order_book";
        measurement.tags = 
            {
                { "source", "ftx" },
                { "side", "ask" },
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
    co_return co_await _statisticsPublisher.publish_statistics( bookMeasurement );
}

} // namespace Ftx
} // namespace Synthfi

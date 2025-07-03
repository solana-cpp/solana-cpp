#include "synthfi/Ftx/FtxOrderClient/FtxOrderClientImpl.hpp"

#include "synthfi/Ftx/FtxWsMessage.hpp"
#include "synthfi/Ftx/FtxUtils.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/this_coro.hpp>

#include <boost/asio/experimental/promise.hpp>

#include <boost/asio/experimental/awaitable_operators.hpp>

#include <fmt/core.h>

using namespace boost::asio::experimental::awaitable_operators;
using namespace std::chrono_literals;

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{
namespace Ftx
{

// Delay in ms to wait to subscribe to private WS channels after logging in.
static constexpr std::chrono::milliseconds ftx_authentication_delay_ms( ) { return 1000ms; }

FtxOrderClientImpl::FtxOrderClientImpl
(
    asio::io_context & ioContext,
    FtxRestClientService & ftxRestClientService,
    FtxReferenceDataClientService & ftxReferenceDataClientService,
    Statistics::StatisticsPublisherService & statisticsPublisherService,
    const FtxAuthenticationConfig & config
)
    : _host( config.host )
    , _apiKey( config.apiKey )
    , _apiSecret( config.apiSecret )
    , _strand( ioContext.get_executor( ) )
    , _restClient( ftxRestClientService )
    , _ftxReferenceDataClient( ftxReferenceDataClientService )
    , _statisticsPublisher( statisticsPublisherService, "ftx_order_client" )
    , _jsonWebSocket( &_strand, _host, "https", "/ws/", std::bind( &FtxOrderClientImpl::on_message, this, std::placeholders::_1 ) )
{ }

void FtxOrderClientImpl::on_message( simdjson::ondemand::document message )
{
    try
    {
        simdjson::ondemand::object messageObject( message );

        auto channel = *magic_enum::enum_cast< FtxChannelType >( messageObject[ "channel" ].get_string( ).value( ) );
        auto responseType = *magic_enum::enum_cast< FtxResponseType >( messageObject[ "type" ].get_string( ).value( ) );

        switch ( responseType )
        {
            case FtxResponseType::subscribed:
            {
                SYNTHFI_LOG_INFO( _logger )
                    << fmt::format
                    (
                        "[{}][{}] Subscribed to channel: {}",
                        name( ),
                        _host,
                        magic_enum::enum_name( channel )
                    );
                return;
            }
            case FtxResponseType::unsubscribed:
            {
                SYNTHFI_LOG_ERROR( _logger )
                    << fmt::format
                    (
                        "[{}][{}] Unsubscribed to channel: {}",
                        name( ),
                        _host,
                        magic_enum::enum_name( channel )
                    );
                return;
            }
            case FtxResponseType::partial:
            case FtxResponseType::update:
            {
                switch ( channel )
                {
                    case FtxChannelType::fills:
                    {
                        auto dataJson = messageObject[ "data" ].value( );
                        on_fill_message( json_to< FtxFillMessage >( dataJson ) );
                        return;
                    }
                    case FtxChannelType::orders:
                    {
                        auto dataJson = messageObject[ "data" ].value( );
                        on_order_message( json_to< FtxOrderMessage >( dataJson ) );
                        return;
                    }
                    default:
                    {
                        SYNTHFI_LOG_ERROR( _logger )
                            << fmt::format
                            (
                                "[{}][{}] Invalid channel type: ",
                                name( ),
                                _host,
                                magic_enum::enum_name( channel )
                            );
                        return;
                    }
                }
            }
            default:
            {
                SYNTHFI_LOG_ERROR( _logger )
                    << fmt::format
                    (
                        "[{}][{}] Invalid response type: {}",
                        name( ),
                        _host,
                        magic_enum::enum_name( responseType )
                    );
                return;
            }
        }
    }
    catch ( const std::exception & ex )
    {
        message.rewind( );
        SYNTHFI_LOG_ERROR( _logger )
            << fmt::format
            (
                    "[{}][{}] Error deserializing message, error: {}, message: {}",
                    name( ),
                    _host,
                    ex.what( ),
                    simdjson::to_json_string( message )
            );
    }
}

asio::awaitable< void > FtxOrderClientImpl::do_login( )
{
    SYNTHFI_LOG_INFO( _logger ) << fmt::format
    (
        "[{}][{}] Logging into FTX private WS channel",
        name( ),
        _host
    );


    FtxAuthenticationMessage loginMessage{ .apiKey = _apiKey, .apiSecret = _apiSecret, .requestTime = _clock.now( ) };
    co_await _jsonWebSocket.do_send_message( boost::json::value_from< FtxAuthenticationMessage >( std::move( loginMessage ) ) );

    // Load reference data.
    _referenceData = co_await _ftxReferenceDataClient.get_reference_data( );

    {
        // Wait for FTX to authenticate WS channel.
        asio::high_resolution_timer loginDelay( _strand, ftx_authentication_delay_ms( ) );
        co_await loginDelay.async_wait( asio::use_awaitable );
    }

    SYNTHFI_LOG_INFO( _logger ) << fmt::format
    (
        "[{}][{}] Subscribing to private FTX fills and orders channels",
        FtxOrderClientImpl::name( ),
        _host
    );
    co_await
    (
        _jsonWebSocket.do_send_message( boost::json::value_from< FtxSubscribeFillsMessage >( { } ) )
        && _jsonWebSocket.do_send_message( boost::json::value_from< FtxSubscribeOrdersMessage >( { } ) )
    );

    co_return;
}

asio::awaitable< Trading::Order > FtxOrderClientImpl::do_send_order( Trading::Order order )
{
    const auto & tradingPair = _referenceData.tradingPairs[ order.trading_pair_index( ) ];
    const auto & tradingPairName = tradingPair.marketInformation.name;

    // Set client order id, use current timestamp as unique id.
    order.client_order_id( ) = _clock.now( ).time_since_epoch( ).count( );

    BOOST_ASSERT_MSG
    (
        order.order_type( ) == Trading::OrderType::immediateOrCancel,
        "Unsupported FTX order type"
    );

    const auto & orderIt = _orders.emplace
    (
        order.client_order_id( ),
        OrderSubscription{ _strand, 30s, order.trading_pair_index( ), std::move( order ) }
    ).first;

    FtxOrderRequest orderRequest
    {
        .reduceOnly = false,
        .ioc = true,
        .postOnly = false,
        .orderType = FtxOrderType::limit,
        .side = order.side( ),
        .price = order.price( ),
        .quantity = order.quantity( ),
        .marketName = tradingPairName,
        .clientOrderId = order.client_order_id( )
    };

    auto [ orderRequestError, pendingOrderResponse ] = co_await _restClient.send_request< FtxOrderRequest, FtxOrderResponse >
    (
        std::move( orderRequest ),
        boost::asio::as_tuple( boost::asio::use_awaitable )
    );

    if ( orderRequestError )
    {
        // Cleanup order state and rethrow.
        _orders.erase( orderIt );

        std::rethrow_exception( orderRequestError );
    }

    // Wait until subscription completes.
    {
        auto [ expiryTimerError ] = co_await orderIt->second.expiryTimer.async_wait( asio::as_tuple( asio::use_awaitable ) );
        if ( expiryTimerError && expiryTimerError != asio::error::operation_aborted )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << fmt::format
                (
                    "[{}][{}] FTX order subscription timer error: {}",
                    name( ),
                    _host,
                    expiryTimerError.what( )
                );
            throw SynthfiError( "FTX WS subscription request timed out" );
        }
    }

    // Move order back to stack.
    order = std::move( orderIt->second.order );

    // Cleanup order state.
    _exchangeOrderIdMap.erase( order.order_id( ) );
    _orders.erase( orderIt );

    co_return order;
}

void FtxOrderClientImpl::on_fill_message( FtxFillMessage ftxFillMessage )
{
    SYNTHFI_LOG_DEBUG( _logger ) << "Received fill message";

    auto findOrder = _orders.find( ftxFillMessage.orderId );
    if ( findOrder == _orders.end( ) )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Received client order id";
    }

    return;
}

void FtxOrderClientImpl::on_order_message( FtxOrderMessage ftxOrderMessage )
{
    SYNTHFI_LOG_DEBUG( _logger ) << "Received order message";

    const auto & findOrder = _orders.find( ftxOrderMessage.clientOrderId );
    if ( findOrder == _orders.end( ) )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Unexpected client order id: " << ftxOrderMessage.clientOrderId;
        return;
    }

    switch ( ftxOrderMessage.orderStatus )
    {
        case FtxOrderStatus::NEW:
        {
            SYNTHFI_LOG_INFO( _logger ) << fmt::format
            (
                "[{}][{}] Received NEW order update, orderId: {:d}",
                name( ),
                _host,
                ftxOrderMessage.orderId
            );
            _exchangeOrderIdMap.emplace( ftxOrderMessage.orderId, &(findOrder->second) );

            // Update order state.
            auto & order = findOrder->second.order;
            order.order_id( ) = ftxOrderMessage.orderId;
            order.fill_quantity( ) = ftxOrderMessage.fillQuantity;
            order.order_state( ) = Trading::OrderState::NEW;
            order.average_fill_price( ) = ftxOrderMessage.averageFillPrice;

            co_spawn( _strand, publish_order_statistic( order ), asio::detached );

            return;
        }
        case FtxOrderStatus::OPEN:
        {
            SYNTHFI_LOG_INFO( _logger ) << fmt::format
            (
                "[{}][{}] Received OPEN order update, orderId: {:d}",
                name( ),
                _host,
                ftxOrderMessage.orderId
            );

            // Update order state.
            auto & order = findOrder->second.order;
            order.order_state( ) = Trading::OrderState::OPEN;
            order.fill_quantity( ) = ftxOrderMessage.fillQuantity;
            order.average_fill_price( ) = ftxOrderMessage.averageFillPrice;

            co_spawn( _strand, publish_order_statistic( order ), asio::detached );

            return;
        }
        case FtxOrderStatus::CLOSED:
        {
            SYNTHFI_LOG_INFO( _logger ) << fmt::format
            (
                "[{}][{}] Received CLOSED order update, orderId: {:d}",
                name( ),
                _host,
                ftxOrderMessage.orderId
            );

            // Update order state.
            auto & order = findOrder->second.order;
            order.order_state( ) = Trading::OrderState::CLOSED;
            order.fill_quantity( ) = ftxOrderMessage.fillQuantity;
            order.average_fill_price( ) = ftxOrderMessage.averageFillPrice;

            findOrder->second.expiryTimer.cancel( );

            co_spawn( _strand, publish_order_statistic( order ), asio::detached );

            return;
        }
        default:
        {
            BOOST_ASSERT_MSG( false, "Invalid FtxOrderStatus" );
            return;
        }
    }
}

asio::awaitable< void > FtxOrderClientImpl::publish_order_statistic( Trading::Order order )
{
    Statistics::InfluxMeasurement orderMeasurement;
    orderMeasurement.measurementName = "order";
    orderMeasurement.tags = 
        {
            { "source", "ftx" },
            { "trading_pair_index", std::to_string( order.trading_pair_index( ) ) }
        };

    orderMeasurement.fields =
        {
            { "price", Statistics::InfluxDataType( order.price( ) ) },
            { "quantity", Statistics::InfluxDataType( order.quantity( ) ) },
            { "side", std::string( magic_enum::enum_name( order.side( ) ) ) },
            { "order_type", std::string( magic_enum::enum_name( FtxOrderType::limit ) ) },
            { "client_order_id", Statistics::InfluxDataType( order.client_order_id( ) ) },
            { "order_state", Statistics::InfluxDataType( Trading::order_state_to_lower( order.order_state( ) ) ) },
            { "average_fill_price", Statistics::InfluxDataType( order.average_fill_price( ) ) },
            { "fill_quantity", Statistics::InfluxDataType( order.fill_quantity( ) ) }
        };

    co_await _statisticsPublisher.publish_statistic( std::move( orderMeasurement ) );

    co_return;
}

} // namespace Ftx
} // namespace Synthfi

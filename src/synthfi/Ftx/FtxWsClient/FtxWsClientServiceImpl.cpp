#include "synthfi/Ftx/FtxWsClient/FtxWsClientServiceImpl.hpp"

#include "synthfi/Ftx/FtxUtils.hpp"

using namespace std::chrono_literals;

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{
namespace Ftx
{

FtxWsClientServiceImpl::FtxWsClientServiceImpl
(
    asio::io_context & ioContext,
    Statistics::StatisticsPublisherService & statisticsPublisherService,
    std::string_view host
)
    : _strand( ioContext.get_executor( ) )
    , _jsonWebSocket( &_strand, host, "https", "/ws/", std::bind( &FtxWsClientServiceImpl::on_message, this, std::placeholders::_1 ) )
    , _statisticsPublisher( statisticsPublisherService, "ftx_ws_api" )
    , _messagesSentCounter
    (
        "ftx_ws_api_sent",
        { { "source", "ftx" } },
        "count",
        0UL,
        &_strand,
        &_statisticsPublisher
    )
    , _messagesReceivedCounter
    ( 
        "ftx_ws_api_received",
        { { "source", "ftx" } },
        "count",
        0UL,
        &_strand,
        &_statisticsPublisher
    )
{ }

boost::asio::awaitable< void > FtxWsClientServiceImpl::do_subscribe_orderbook
(
    std::string marketName,
    OnOrderbookMessageFn onOrderbookSnapshot,
    OnOrderbookMessageFn onOrderbookUpdate
)
{
    SYNTHFI_LOG_INFO( _logger )
        << fmt::format
        (
            "[{}] Adding orderbook subscription, market: {}",
            name( ),
            marketName
        );

    bool success = _orderbookSubscriptions.emplace
    (
        marketName,
        OrderbookSubscription( _strand, 30s, std::move( onOrderbookSnapshot ), std::move( onOrderbookUpdate ) )
    ).second;
    BOOST_ASSERT_MSG( success, "FtsWsClientService only supports a single subscriber per orderbook" );

    SubscriptionRequest subscriptionRequest{ FtxChannelType::orderbook, marketName };
    co_await _jsonWebSocket.do_send_message( boost::json::value_from( std::move( subscriptionRequest ) ) );

    // Update statistics.
    _messagesSentCounter.increment( 1 );

    // Yielding invalidates reference to subscription.
    auto findSubscription = _orderbookSubscriptions.find( marketName );
    BOOST_ASSERT_MSG( findSubscription != _orderbookSubscriptions.end( ), "Expected orderbook subscription" );

    boost::system::error_code errorCode;
    co_await findSubscription->second.expiryTimer.async_wait( boost::asio::redirect_error( boost::asio::use_awaitable, errorCode ) );
    if ( errorCode == boost::asio::error::timed_out )
    {
        SYNTHFI_LOG_ERROR( _logger ) << fmt::format( "[{}] Subscription timed out", name( ) );

        throw SynthfiError( "FTX WS Subscription request timed out" );
    }
    else if ( errorCode && errorCode != boost::asio::error::operation_aborted )
    {
        SYNTHFI_LOG_ERROR( _logger )
            << fmt::format( "[{}] Subscription error: {}", name( ), errorCode.what( ) );
    }

    // Yielding invalidates reference to subscription.
    findSubscription = _orderbookSubscriptions.find( marketName );
    BOOST_ASSERT_MSG( findSubscription != _orderbookSubscriptions.end( ), "Expected orderbook subscription" );

    if ( findSubscription->second.error )
    {
        // Erase invalid subscription, and rethrow exception.
        auto error = findSubscription->second.error;
        _orderbookSubscriptions.erase( findSubscription );
        throw error;
    }

    co_return;
}

void FtxWsClientServiceImpl::on_message( simdjson::ondemand::document message )
{
    message.rewind( );

    simdjson::ondemand::object messageObject( message );
    try
    {
        const auto channelType = magic_enum::enum_cast< FtxChannelType >( messageObject[ "channel" ].get_string( ).value( ) ).value( );
        const auto messageType = magic_enum::enum_cast< FtxResponseType >( messageObject[ "type" ].value( ).get_string( ).value( ) ).value( );

        std::string_view marketName;
        auto marketNameError = messageObject[ "market" ].get( marketName );
        if ( marketNameError )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << fmt::format
                (
                    "[{}] Received unexpected message, type: {}, channel: {}",
                    name( ),
                    magic_enum::enum_name( messageType ),
                    magic_enum::enum_name( channelType )
                );
        }
        else
        {
            switch ( channelType )
            {
                case FtxChannelType::orderbook:
                {
                    auto findOrderbook = _orderbookSubscriptions.find( marketName );
                    if ( findOrderbook == _orderbookSubscriptions.end( ) )
                    {
                        SYNTHFI_LOG_ERROR( _logger )
                            << fmt::format( "[{}] Missing orderbook subscription for: {}", name( ), marketName );
                        return;
                    }
                    switch ( messageType )
                    {
                        case FtxResponseType::subscribed:
                        {
                            SYNTHFI_LOG_INFO( _logger )
                                << fmt::format( "[{}] Successfully subscribed to orderbook: {}", name( ), marketName );
                            findOrderbook->second.expiryTimer.cancel( );
                            break;
                        }
                        case FtxResponseType::unsubscribed:
                        {
                            SYNTHFI_LOG_ERROR( _logger )
                                << fmt::format( "[{}] Unsupported opcode: Unsubscribed to orderbook: {}", name( ), marketName );
                            break;
                        }
                        case FtxResponseType::partial:
                        {
                            try
                            {
                                findOrderbook->second.onOrderbookSnapshot
                                (
                                    json_to< FtxOrderbookMessage >( messageObject[ "data" ].value( ) )
                                );
                            }
                            catch ( const std::exception & ex )
                            {
                                SYNTHFI_LOG_ERROR( _logger )
                                    << fmt::format( "[{}] Error deserializing orderbook snapshot: {}", name( ), ex.what( ) );
                            }

                            break;
                        }
                        case FtxResponseType::update:
                        {
                                findOrderbook->second.onOrderbookUpdate
                                (
                                    json_to< FtxOrderbookMessage >( messageObject[ "data" ].value( ) )
                                );
                            break;
                        }
                        case FtxResponseType::error:
                        case FtxResponseType::info:
                        {
                            uint64_t code = 0;
                            auto codeError = messageObject[ "code" ].get( code );

                            std::string_view message;
                            auto messageError = messageObject[ "message" ].get( message );

                            SYNTHFI_LOG_ERROR( _logger )
                                << fmt::format
                                (
                                    "[{}] Received orderbook response, error type: {}, code: {}, message: {}",
                                    name( ),
                                    magic_enum::enum_name( messageType ),
                                    ( codeError ?  "(none)" : std::to_string( code ) ),
                                    ( messageError ? "(none)" : message )
                                );
                            break;
                        }
                        default:
                        {
                            BOOST_ASSERT_MSG( false, "Unexpected FTX WS response type" );
                            break;
                        }
                    }
                    break;
                }
                default:
                {
                    SYNTHFI_LOG_ERROR( _logger )
                        << fmt::format
                        (
                            "[{}] Unsupported channel type: {}",
                            name( ),
                            magic_enum::enum_name( channelType )
                        );
                    break;
                }
            }
        }
    }
    catch ( const std::exception & ex )
    {
        SYNTHFI_LOG_ERROR( _logger )
            << fmt::format( "[{}] Error deserializing message: {}", name( ), ex.what( ) );
    }

    // Update statistics.
    _messagesReceivedCounter.increment( 1 );
}

} // namespace Ftx
} // namespace Synthfi

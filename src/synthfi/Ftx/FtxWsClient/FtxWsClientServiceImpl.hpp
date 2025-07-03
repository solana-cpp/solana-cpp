#pragma once

#include "synthfi/Ftx/FtxTypes.hpp"
#include "synthfi/Ftx/FtxWsMessage.hpp"

#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

#include "synthfi/Io/WebSocket/SSLWebSocket.hpp"
#include "synthfi/Io/WebSocket/JsonWebSocket.hpp"

#include "synthfi/Util/Logger.hpp"
#include "synthfi/Util/StringHash.hpp"
#include "synthfi/Util/Utils.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/strand.hpp>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>

#include <boost/json/value_to.hpp>
#include <boost/json/value_from.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>

#include <simdjson.h>

#include <chrono>

namespace Synthfi
{
namespace Ftx
{

class FtxWsClientServiceImpl
{
public:
    using OnOrderbookMessageFn = std::function< void( FtxOrderbookMessage orderbookMessage ) >;

    FtxWsClientServiceImpl
    (
        boost::asio::io_context & ioContext,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        std::string_view host
    );

    template< class CompletionHandlerType >
    void subscribe_orderbook
    (
        std::string marketName,
        OnOrderbookMessageFn onOrderbookSnapshot,
        OnOrderbookMessageFn onOrderbookUpdate,
        CompletionHandlerType && completionHandler
    )
    {
        boost::asio::co_spawn
        (
            _strand,
            do_subscribe_orderbook( std::move( marketName ), std::move( onOrderbookSnapshot ), std::move( onOrderbookUpdate ) ),
            completionHandler
        );
    }

    constexpr std::string name( ) const & { return "FtxWsClient"; }

private:
    struct OrderbookSubscription
    {
        OrderbookSubscription
        (
            boost::asio::strand< boost::asio::io_context::executor_type > & strand,
            std::chrono::seconds timeout,
            OnOrderbookMessageFn onOrderbookSnapshot,
            OnOrderbookMessageFn onOrderbookUpdate
        )
            : expiryTimer( strand, timeout )
            , onOrderbookSnapshot( std::move( onOrderbookSnapshot ) )
            , onOrderbookUpdate( std::move( onOrderbookUpdate ) )
        { }

        std::exception_ptr error;
        boost::asio::high_resolution_timer expiryTimer;

        OnOrderbookMessageFn onOrderbookSnapshot;
        OnOrderbookMessageFn onOrderbookUpdate;
    };

    boost::asio::awaitable< void > do_subscribe_orderbook( std::string marketName, OnOrderbookMessageFn onOrderbookSnapshot, OnOrderbookMessageFn onOrderbookUpdate );

    void on_message( simdjson::ondemand::document message );

    // Asynchronous operations on a tcp socket must be performed within the same strand.
    boost::asio::strand< boost::asio::io_context::executor_type > _strand;

    std::chrono::high_resolution_clock _clock;

    Io::JsonWebSocket< Io::SSLWebSocket > _jsonWebSocket;

    TransparentStringMap< OrderbookSubscription > _orderbookSubscriptions;

    Statistics::StatisticsPublisher _statisticsPublisher;

    Statistics::Counter< uint64_t, Statistics::StatisticsPublisher > _messagesSentCounter;
    Statistics::Counter< uint64_t, Statistics::StatisticsPublisher > _messagesReceivedCounter;

    mutable SynthfiLogger _logger;
};

} // namespace Ftx
} // namespace Synthfi

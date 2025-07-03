#pragma once

#include "synthfi/Ftx/FtxRestClient/FtxRestClient.hpp"
#include "synthfi/Ftx/FtxReferenceDataClient/FtxReferenceDataClient.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

#include "synthfi/Ftx/FtxRestMessage.hpp"
#include "synthfi/Ftx/FtxTypes.hpp"
#include "synthfi/Ftx/FtxWsMessage.hpp"

#include "synthfi/Io/WebSocket/JsonWebSocket.hpp"
#include "synthfi/Io/WebSocket/SSLWebSocket.hpp"

#include "synthfi/Trading/Order.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>
#include <functional>
#include <optional>
#include <string>

namespace Synthfi
{
namespace Ftx
{

class FtxOrderClientImpl
{
public:
    FtxOrderClientImpl
    (
        boost::asio::io_context & ioContext,
        FtxRestClientService & ftxRestClientService,
        FtxReferenceDataClientService & ftxReferenceDataClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        const FtxAuthenticationConfig & config
    );

    template< class CompletionHandlerType >
    void login( CompletionHandlerType completionHandler )
    {
        co_spawn( _strand, do_login( ), completionHandler );
    }

    template< class CompletionHandlerType >
    void send_order( Trading::Order order, CompletionHandlerType completionHandler )
    {
        co_spawn
        (
            _strand,
            do_send_order( std::move( order ) ),
            completionHandler
        );
    }

    const std::string name( ) const & { return "FtxOrderClient"; }

private:
    struct OrderSubscription
    {
        OrderSubscription
        (
            boost::asio::strand< boost::asio::io_context::executor_type > & strand,
            std::chrono::seconds timeout,
            uint8_t tradingPairIndex,
            Trading::Order order
        )
        : expiryTimer( strand, timeout )
        , tradingPairIndex( tradingPairIndex )
        , order( std::move( order ) )
        { }

        std::exception_ptr error;
        boost::asio::high_resolution_timer expiryTimer;

        std::optional< uint64_t > orderId;
        uint8_t tradingPairIndex;
        Trading::Order order;
    };

    boost::asio::awaitable< void > do_login( );

    boost::asio::awaitable< Trading::Order > do_send_order( Trading::Order order );

    boost::asio::awaitable< void > publish_order_statistic( Trading::Order order );

    void on_message( simdjson::ondemand::document message );

    void on_fill_message( FtxFillMessage fillMessage );
    void on_order_message( FtxOrderMessage ftxOrderMessage );

    std::string _host;
    std::string _apiKey;
    std::string _apiSecret;

    std::chrono::high_resolution_clock _clock;

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;

    FtxRestClient _restClient;
    FtxReferenceDataClient _ftxReferenceDataClient;
    Statistics::StatisticsPublisher _statisticsPublisher;

    FtxReferenceData _referenceData;

    Io::JsonWebSocket< Io::SSLWebSocket > _jsonWebSocket;

    std::unordered_map< uint64_t, OrderSubscription* > _exchangeOrderIdMap;
    std::unordered_map< uint64_t, OrderSubscription > _orders;

    mutable SynthfiLogger _logger;
};

} // namespace Ftx
} // namespace Synthfi

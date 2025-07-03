#pragma once

#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"
#include "synthfi/Solana/SolanaTypes.hpp"

#include "synthfi/Io/JsonHttpSocket/JsonHttpSocket.hpp"
#include "synthfi/Io/RpcSocket/RpcSocket.hpp"
#include "synthfi/Util/Utils.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/strand.hpp>

#include <boost/json/value.hpp>

#include "synthfi/Util/Logger.hpp"

namespace Synthfi
{
namespace Solana
{

class SolanaHttpClientImpl
{
public:
    SolanaHttpClientImpl
    (
        boost::asio::io_context & ioContext,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        const SolanaEndpointConfig & config
    );

    template< class RequestType, class ResponseType, class CompletionHandlerType >
    void send_request( RequestType request, CompletionHandlerType && completionHandler )
    {
        boost::asio::co_spawn( _strand, _jsonHttpSocket.do_send_request< RequestType, ResponseType >( std::move( request ) ), completionHandler );
    }

    // Blocking call to shutdown HTTP connection.
    void shutdown( );

    constexpr std::string name( ) const & { return "SolanaHttpClient"; }

private:
    // Handle invalid response - the only protocol over this socket should be JSON RPC.
    void on_message( simdjson::ondemand::document response );

    // Asynchronous operations on a tcp socket must be performed within the same strand.
    boost::asio::strand< boost::asio::io_context::executor_type > _strand;

    Statistics::StatisticsPublisher _statisticsPublisher;
    Io::RpcSocket< Io::JsonHttpSocket > _jsonHttpSocket;

    SynthfiLogger _logger;
};

} // namespace Solana
} // namespace Synthfi

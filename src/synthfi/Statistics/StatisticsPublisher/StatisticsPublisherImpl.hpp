#pragma once

#include "synthfi/Statistics/StatisticsTypes.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

#include <deque>

namespace Synthfi
{
namespace Statistics
{

class StatisticsPublisherImpl
{
public:
    StatisticsPublisherImpl( boost::asio::io_context & ioContext, const StatisticsPublisherConfig & config );

    template< class CompletionHandlerType >
    void publish_statistic( InfluxMeasurement measurement, CompletionHandlerType completionHandler )
    {
        // Tag instance id.
        measurement.tags.emplace_back( "instance_id", _instanceId );

        co_spawn( _strand, do_publish_statistic( std::move( measurement ) ), completionHandler );
    }

    template< class CompletionHandlerType >
    void publish_statistics( std::vector< InfluxMeasurement > measurements, CompletionHandlerType completionHandler )
    {
        // Tag instance id.
        for ( auto & measurement : measurements )
        {
            measurement.tags.emplace_back( "instance_id", _instanceId );
        }

        co_spawn( _strand, do_publish_statistics( std::move( measurements ) ), completionHandler );
    }

    boost::asio::strand< boost::asio::io_context::executor_type > & strand( ) { return _strand; }

    constexpr std::string name( ) const & { return "StatisticsPublisher"; }

private:
    void start_reconnect( );

    boost::asio::awaitable< void > do_connect( );
    boost::asio::awaitable< void > do_read( );
    boost::asio::awaitable< void > do_write( );

    boost::asio::awaitable< void > do_connect_influx( );

    boost::asio::awaitable< void > do_publish_statistic( InfluxMeasurement measurement );
    boost::asio::awaitable< void > do_publish_statistics( std::vector< InfluxMeasurement > measurement );

    StatisticsPublisherConfig _config;

    std::chrono::high_resolution_clock _clock;
    std::string _instanceId;

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;

    // Simultaneous connect attemps would reopen socket and handshake, cancelling pending reads.
    bool _isConnecting;

    // Writers wait on this timer for reconnect to complete or timeout before writing.
    boost::asio::high_resolution_timer _reconnectTimer;

    boost::asio::ip::tcp::resolver _resolver;
    boost::beast::tcp_stream _socket;

    bool _isWriting = false;
    boost::beast::http::header< true > _writeHeader;
    std::string _writeBuffer;

    std::string _host;
    mutable SynthfiLogger _logger;
};

} // namespace Statistics
} // namespace Synthfi

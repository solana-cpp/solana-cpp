#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherImpl.hpp"

#include "synthfi/Util/Utils.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/detached.hpp>

#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/version.hpp>

#include <boost/asio/experimental/awaitable_operators.hpp>

#include <fmt/ostream.h>
#include <fmt/core.h>

using namespace boost::asio::experimental::awaitable_operators;
using namespace std::chrono_literals;

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{
namespace Statistics
{

StatisticsPublisherImpl::StatisticsPublisherImpl
(
    boost::asio::io_context & ioContext,
    const StatisticsPublisherConfig & config
)
    : _config( config )
    , _instanceId( std::to_string( _clock.now( ).time_since_epoch( ).count( ) ) ) // Unique per instance of service.
    , _strand( ioContext.get_executor( ) )
    , _reconnectTimer( _strand )
    , _resolver( ioContext )
    , _socket( _strand )
{
    _writeHeader.set( beast::http::field::authorization, fmt::format( "Token {}", _config.influxApiToken ) );
    _writeHeader.set( beast::http::field::host, fmt::format( "{}:{}", _config.influxAddress, _config.influxPort ) ); // Required.
    _writeHeader.set( beast::http::field::accept, "application/json" );
    _writeHeader.target( fmt::format( "/api/v2/write?bucket={}&org=synthfi&precision=ns", _config.bucket ) );
    _writeHeader.method( beast::http::verb::post );

    start_reconnect( );
}

void StatisticsPublisherImpl::start_reconnect( )
{
    _isConnecting = true;
    // Set deadline timer for writers and check for reconnect failure.
    _reconnectTimer.expires_from_now( 30s );
    asio::co_spawn( _strand, do_connect( ), asio::detached );
}


asio::awaitable< void > StatisticsPublisherImpl::do_connect( )
{
    SYNTHFI_LOG_INFO( _logger ) << fmt::format( "[{}][{}:{}] Opening HTTP connection", name( ), _config.influxAddress, _config.influxPort );

    auto const results = co_await _resolver.async_resolve( _config.influxAddress, std::to_string( _config.influxPort ), boost::asio::use_awaitable );

    _socket.expires_after( std::chrono::seconds( 15 ) );
    auto const connection = co_await _socket.async_connect( results, boost::asio::use_awaitable );

    _socket.socket( ).set_option( asio::socket_base::keep_alive( true ) );
    _socket.expires_never( );

    SYNTHFI_LOG_INFO( _logger ) <<
        fmt::format( "[{}][{}:{}] Connected to InfluxDB: {}", name( ), _config.influxAddress, _config.influxPort, connection );

    _reconnectTimer.cancel( );
    _isConnecting = false;

    asio::co_spawn( _strand, do_read( ), asio::detached );

    co_return;
}

namespace
{
    // helper constant for the visitor #3
    template< class > inline constexpr bool always_false_v = false;
}

static std::string encode_measurement
(
    const InfluxMeasurement & measurement,
    std::chrono::high_resolution_clock::time_point timeStamp
)
{
    // Encode line protocol:
    // 
    // Syntax:
    // <measurement>[,<tag_key>=<tag_value>[,<tag_key>=<tag_value>]] <field_key>=<field_value>[,<field_key>=<field_value>] [<timestamp>]
    //
    // Example:
    // myMeasurement,tag1=value1,tag2=value2 fieldKey="fieldValue" 1556813561098000000

    const auto & [ measurementName, tags, fields ] = measurement;

    auto out = fmt::memory_buffer( );
    out.reserve( 2 + tags.size( ) + fields.size( ) );

    // Encode required measurement name.
    fmt::format_to( std::back_inserter( out ), "{}", measurementName );

    // Encode optional tags.
    for ( const auto & [ key, value ] : tags )
    {
        fmt::format_to( std::back_inserter( out ), ",{}={}", key, value );
    }

    // Encode required first field.
    {
        auto & [ key, value ] = fields.front( );
        std::visit
        (
            [ &out, &key ]( auto && value )
            {
                using T = std::decay_t< decltype ( value ) >;

                if constexpr ( std::is_same_v< T, uint64_t > )
                    fmt::format_to( std::back_inserter( out ), " {}={}", key, value );
                else if constexpr ( std::is_same_v< T, double > )
                    fmt::format_to( std::back_inserter( out ), " {}={}", key, value );
                else if constexpr ( std::is_same_v< T, int64_t > )
                    fmt::format_to( std::back_inserter( out ), " {}={}", key, value );
                else if constexpr ( std::is_same_v< T, bool > )
                    fmt::format_to( std::back_inserter( out ), " {}={}", key, value );
                else if constexpr ( std::is_same_v< T, std::string > )
                    fmt::format_to( std::back_inserter( out ), " {}=\"{}\"", key, value );
                else if constexpr ( std::is_same_v< T, FixedFloat > )
                    fmt::format_to( std::back_inserter( out ), " {}={}", key, value );
                else
                    static_assert( always_false_v< T >, "Invalid influx field type" );
            },
            value
        );
    }

    // Encode optional remaining fields.
    for ( const auto & [ key, value ] : std::ranges::subrange( fields.begin( ) + 1, fields.end( ) ) )
    {
        std::visit
        (
            [ &out, &key ]( auto && value )
            {
                using T = std::decay_t< decltype ( value ) >;

                if constexpr ( std::is_same_v< T, uint64_t > )
                    fmt::format_to( std::back_inserter( out ), ",{}={}", key, value );
                else if constexpr ( std::is_same_v< T, double > )
                    fmt::format_to( std::back_inserter( out ), ",{}={}", key, value );
                else if constexpr ( std::is_same_v< T, int64_t > )
                    fmt::format_to( std::back_inserter( out ), ",{}={}", key, value );
                else if constexpr ( std::is_same_v< T, bool > )
                    fmt::format_to( std::back_inserter( out ), ",{}={}", key, value );
                else if constexpr ( std::is_same_v< T, std::string > )
                    fmt::format_to( std::back_inserter( out ), ",{}=\"{}\"", key, value );
                else if constexpr ( std::is_same_v< T, FixedFloat > )
                    fmt::format_to( std::back_inserter( out ), ",{}={}", key, value );
                else
                    static_assert( always_false_v< T >, "Invalid influx field type" );
            },
            value
        );
    }

    // Encode timestamp.
    fmt::format_to( std::back_inserter( out ), " {}", timeStamp.time_since_epoch( ).count( ) );

    return fmt::to_string( out );
}

asio::awaitable< void > StatisticsPublisherImpl::do_publish_statistic( InfluxMeasurement measurement )
{
    if ( measurement.fields.empty( ) )
    {
        SYNTHFI_LOG_INFO( _logger ) <<
            fmt::format
            (
                "[{}][{}:{}] Invalid measurement: line protocol requires must have at least one field: {}",
                name( ),
                _config.influxAddress,
                _config.influxPort,
                measurement.measurementName
            );
        throw SynthfiError( "Measurements must have at least one field" );
    }

    const auto now = _clock.now( );

    // Push measurement to write buffer.
    if ( _writeBuffer.empty( ) )
    {
        _writeBuffer = encode_measurement( measurement, now );
    }
    else
    {
        _writeBuffer += "\n" + encode_measurement( measurement, now );
    }

    if ( !_isWriting )
    {
        _isWriting = true;
        co_spawn( _strand, do_write( ), asio::detached );
    }

    co_return;
}

boost::asio::awaitable< void > StatisticsPublisherImpl::do_publish_statistics( std::vector< InfluxMeasurement > measurements )
{
    if ( measurements.empty( ) )
    {
        co_return;
    }

    const auto now = _clock.now( );

    for ( const auto & measurement : measurements )
    {
        if ( measurement.fields.empty( ) )
        {
            SYNTHFI_LOG_INFO( _logger ) <<
                fmt::format
                (
                    "[{}][{}:{}] Invalid measurement: line protocol requires must have at least one field: {}",
                    name( ),
                    _config.influxAddress,
                    _config.influxPort,
                    measurement.measurementName
                );
            throw SynthfiError( "Measurements must have at least one field" );
        }

        if ( _writeBuffer.empty( ) )
        {
            _writeBuffer = encode_measurement( measurement, now );
        }
        else
        {
            _writeBuffer += "\n" + encode_measurement( measurement, now );
        }
    }

    if ( !_isWriting )
    {
        _isWriting = true;
        co_spawn( _strand, do_write( ), asio::detached );
    }

    co_return;
}

asio::awaitable< void > StatisticsPublisherImpl::do_write( )
{
    BOOST_ASSERT_MSG( _isWriting, "do_write called while publisher is already writing" );

    while ( !_writeBuffer.empty( ) )
    {
        if ( _isConnecting )
        {
            auto [ errorCode ] = co_await _reconnectTimer.async_wait( asio::as_tuple( asio::use_awaitable ) );
            if ( errorCode && errorCode != asio::error::operation_aborted )
            {
                SYNTHFI_LOG_ERROR( _logger ) <<
                    fmt::format( "[{}][{}:{}] Reconnect timer error: {}", name( ), _config.influxAddress, _config.influxPort, errorCode.what( ) );
                co_return;
            }
        }

        // Copy the write buffer to the local stack.
        beast::http::request< beast::http::string_body > request
        (
            _writeHeader,
            std::move( _writeBuffer )
        );
        request.prepare_payload( );

        auto [ errorCode, bytesWritten ] = co_await beast::http::async_write
        (
            _socket,
            request,
            asio::as_tuple( asio::use_awaitable )
        );

        // Reset write buffer.
        _writeBuffer = std::string{ };

        if ( errorCode )
        {
            SYNTHFI_LOG_ERROR( _logger ) <<
                fmt::format
                (
                        "[{}][{}:{}] Write error: {}, bytes written: {}",
                        name( ),
                        _config.influxAddress,
                        _config.influxPort,
                        errorCode.what( ),
                        bytesWritten
                );
        }
        else
        {
            SYNTHFI_LOG_TRACE( _logger ) <<
                fmt::format( "[{}][{}:{}] Successfully wrote: {}", name( ), _config.influxAddress, _config.influxPort, request );
        }
    }

    _isWriting = false;
    co_return;
}

asio::awaitable< void > StatisticsPublisherImpl::do_read( )
{
    beast::multi_buffer readBuffer; // Read buffer must be persisted between calls.

    // Read until no pending requests, or socket closed.
    while ( true )
    {
        // Receive the HTTP response
        beast::http::response< beast::http::string_body > httpResponse; // Message object should not have previous contents.
        auto [ errorCode, bytesRead ] = co_await beast::http::async_read
        (
            _socket,
            readBuffer,
            httpResponse,
            asio::as_tuple( asio::use_awaitable )
        );

        if ( errorCode == asio::error::operation_aborted )
        {
            // Connection was closed by client.
            // TODO: cleanup pending requests.
            co_return;
        }
        else if ( errorCode == beast::http::error::end_of_stream )
        {
            // Connection was closed by server, attempt reconnect.
            // TODO: cleanup pending requests.
            SYNTHFI_LOG_ERROR( _logger ) <<
                fmt::format
                (
                    "[{}][{}:{}] Connection closed by server, attempting to reconnect",
                    name( ),
                    _config.influxAddress,
                    _config.influxPort,
                    errorCode.what( )
                );
            start_reconnect( );
            co_return;
        }
        else if ( errorCode )
        {
            SYNTHFI_LOG_ERROR( _logger ) <<
                fmt::format
                (
                    "[{}][{}:{}] Read error: {}, code: {:d}, catagory: {} ",
                    name( ),
                    _config.influxAddress,
                    _config.influxPort,
                    errorCode.what( ),
                    errorCode.value( ),
                    errorCode.category( ).name( )
                );
            co_return;
        }

        if ( httpResponse.result( ) == beast::http::status::no_content )
        {
            SYNTHFI_LOG_TRACE( _logger ) <<
                fmt::format
                (
                    "[{}][{}:{}] Received statistics ack",
                    name( ),
                    _config.influxAddress,
                    _config.influxPort
                );
        }
        else
        {
            SYNTHFI_LOG_ERROR( _logger ) <<
                fmt::format
                (
                    "[{}][{}:{}] Invalid response, code: {}, message{}",
                    name( ),
                    _config.influxAddress,
                    _config.influxPort,
                    httpResponse.result( ),
                    httpResponse
                );
        }
    }

    co_return;
}

} // namespace Statistics
} // namespace Synthfi

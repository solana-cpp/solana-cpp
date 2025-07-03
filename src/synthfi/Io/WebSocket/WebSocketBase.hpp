#pragma once

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <boost/beast/core/buffers_prefix.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/make_printable.hpp>
#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/beast/websocket/rfc6455.hpp>

#include <simdjson.h>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include <functional>
#include <span>

namespace Synthfi
{
namespace Io
{

template< class ConcreteWebSocket >
class WebSocketBase
{
public:
    using OnMessageFn = std::function< void( std::string_view, size_t ) >;

    WebSocketBase( boost::asio::strand< boost::asio::io_context::executor_type > * strand, OnMessageFn onMessageFn )
        : _onMessageFn( onMessageFn )
        , _strand( strand )
        , _reconnectTimer( *_strand, std::chrono::seconds( 15 ) )
    { }

    boost::asio::awaitable< void > do_send_message( std::string message )
    {
        boost::system::error_code errorCode;
        _writeBuffer.push_back( std::move( message ) );

        // If first pending request, start writer coro.
        if ( _writeBuffer.size( ) == 1 )
        {
            boost::asio::co_spawn( *get_strand( ), do_write( ), boost::asio::detached );
        }

        co_return;
    }

    boost::asio::awaitable< void > do_close( )
    {
        SYNTHFI_LOG_DEBUG( get_logger( ) ) << fmt::format( "[{}] Gracefully closing WebSocket connection", name( ) );

        co_await impl( ).get_next_layer( ).async_close( boost::beast::websocket::close_code::normal, boost::asio::use_awaitable );
    }

    constexpr std::string name( ) const & { return "WebSocketBase"; }

protected:
    SynthfiLogger & get_logger( ) { return _logger; }
    boost::asio::high_resolution_timer & reconnect_timer( ) { return _reconnectTimer; }
    boost::asio::strand< boost::asio::io_context::executor_type > * get_strand( ) { return _strand; }

    boost::asio::awaitable< void > do_read( )
    {
        // Receive the ws response
        boost::beast::flat_buffer readBuffer;
        while ( true )
        {
            boost::system::error_code errorCode;

            size_t bytesRead = co_await impl( ).get_next_layer( ).async_read
            (
                readBuffer,
                boost::asio::redirect_error( boost::asio::use_awaitable, errorCode )
            );

            // TODO: handle reconnect.
            if ( errorCode == boost::beast::websocket::error::closed )
            {
                SYNTHFI_LOG_ERROR( get_logger( ) )
                    << fmt::format
                    (
                        "[{}] Connection closed by server, reason: {}",
                        name( ),
                        impl( ).get_next_layer( ).reason( )
                    );
                co_return;
            }
            if ( errorCode == boost::asio::error::operation_aborted )
            {
                SYNTHFI_LOG_DEBUG( get_logger( ) ) << fmt::format( "[{}] Connection closed by client", name( ) );
                co_return;
            }
            if ( errorCode )
            {
                SYNTHFI_LOG_ERROR( get_logger( ) )
                    << fmt::format( "[{}] Read error: {}", name( ), errorCode.what( ) );
                co_return;
            }

            readBuffer.reserve( bytesRead + simdjson::SIMDJSON_PADDING ); // reserve space for simdjson padding.

            SYNTHFI_LOG_TRACE( impl( )._logger )
                << fmt::format
                (
                    "[{}] Read: {}",
                    name( ),
                    std::string_view
                    (
                        reinterpret_cast< const char * >( readBuffer.data( ).data( ) ),
                        bytesRead
                    )
                );
            _onMessageFn
            (
                std::string_view
                (
                    reinterpret_cast< const char * >( readBuffer.data( ).data( ) ),
                    bytesRead
                ),
                bytesRead + simdjson::SIMDJSON_PADDING
            );

            readBuffer.consume( bytesRead );
        }
    }

private:
    ConcreteWebSocket & impl( ) { return static_cast< ConcreteWebSocket & >( *this ); }

    boost::asio::awaitable< void > do_write( )
    {
        while( !_writeBuffer.empty( ) )
        {
            boost::beast::error_code errorCode;

            if ( !impl( ).get_next_layer( ).is_open( ) )
            {
                co_await _reconnectTimer.async_wait( boost::asio::redirect_error( boost::asio::use_awaitable, errorCode ) );
                if ( errorCode && errorCode != boost::asio::error::operation_aborted )
                {
                    SYNTHFI_LOG_ERROR( get_logger( ) )
                        << fmt::format( "[{}] Reconnect timer error: {}", name( ), errorCode.what( ) );
                    co_return;
                }
            }

            auto bytesWritten = co_await impl( ).get_next_layer( ).async_write
            (
                boost::asio::buffer( _writeBuffer.front( ) ),
                boost::asio::redirect_error( boost::asio::use_awaitable, errorCode )
            );

            if ( errorCode )
            {
                SYNTHFI_LOG_ERROR( get_logger( ) )
                    << fmt::format
                    (
                        "[{}] Write error: {}, bytes written: {}",
                        name( ),
                        errorCode.what( ),
                        bytesWritten
                    );
                _writeBuffer.clear( );
                co_return;
            }

            _writeBuffer.pop_front( );
        }
    }

    OnMessageFn _onMessageFn;

    boost::asio::strand< boost::asio::io_context::executor_type > * _strand;
    // Writers wait on this timer for reconnect to complete or timeout before writing.
    boost::asio::high_resolution_timer _reconnectTimer;

    std::deque< std::string > _writeBuffer;
    mutable SynthfiLogger _logger;
};

} // namespace Io
} // namespace Synthfi

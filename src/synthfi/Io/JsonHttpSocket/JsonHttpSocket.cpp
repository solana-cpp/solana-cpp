#include "synthfi/Io/JsonHttpSocket/JsonHttpSocket.hpp"

#include "synthfi/Util/JsonBody.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>

#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/core/multi_buffer.hpp>

#include <boost/json/serialize.hpp>

#include <fmt/core.h>
#include <fmt/ostream.h>

namespace asio = boost::asio;
namespace beast = boost::beast;

using namespace std::chrono_literals;

namespace Synthfi
{
namespace Io
{

// TODO: figure out "nice" delay value, and why we can't keep alive.
static std::chrono::milliseconds reconnect_delay( ){ return 100ms; }

JsonHttpSocket::JsonHttpSocket
(
    asio::strand< boost::asio::io_context::executor_type > * strand,
    std::string_view host,
    std::string_view service,
    std::string_view target,
    OnJsonMessageFn onJsonMessageFn
)
    : _host( host )
    , _service ( service )
    , _target( target )
    , _onJsonMessageFn( onJsonMessageFn )
    , _strand( strand )
    , _reconnectDelayTimer( *strand )
    , _reconnectTimer( *strand )
    , _sslContext( asio::ssl::context::method::sslv23_client )
    , _tcpResolver( *strand )
    , _tcpSocket( nullptr )
{
    _sslContext.set_verify_mode( asio::ssl::verify_none );

    // No reconnect delay on initial reconnect.
    start_reconnect( 0ms );
}

void JsonHttpSocket::start_reconnect( std::chrono::milliseconds reconnectDelay )
{
    _isConnecting = true;
    // Set deadline timer for writers and check for reconnect failure.
    _reconnectTimer.expires_from_now( 30s );
    asio::co_spawn( *get_strand( ), do_connect( reconnectDelay ), asio::detached );
}

asio::awaitable< void > JsonHttpSocket::do_send_message( boost::json::value jsonRequest )
{
    boost::system::error_code errorCode;

    beast::http::request< beast::http::string_body > httpRequest;
    httpRequest.method( beast::http::verb::post );
    httpRequest.target( _target );
    httpRequest.set( beast::http::field::content_type, "application/json" );

    httpRequest.body( ) = boost::json::serialize( jsonRequest );
    httpRequest.prepare_payload( ); // Set Content-Length and Transfer-Encoding field values.

    // Queue HTTP request for writer coro.
    _writeBuffer.push_back( std::move( httpRequest ) );

    // If first pending request, start writer coro.
    if ( !_isWriting )
    {
        asio::co_spawn( *get_strand( ), do_write( ), asio::detached );
    }
    co_return;
}

asio::awaitable< void > JsonHttpSocket::do_close( )
{
    SYNTHFI_LOG_DEBUG( get_logger( ) )
        << fmt::format( "[{}][{}:{}{}] Gracefully closing connection", name( ), _host, _service, _target );


    // Blocking /shrug, presumably so is the destructor.
    beast::get_lowest_layer( *_tcpSocket ).close( );

    co_return;
}

asio::awaitable< void > JsonHttpSocket::do_connect( std::chrono::milliseconds reconnectDelay )
{
    SYNTHFI_LOG_INFO( get_logger( ) )
        << fmt::format
        (
            "[{}][{}:{}{}] Opening connection, reconnect delay: {}",
            name( ),
            _host,
            _service,
            _target,
            reconnectDelay
        );

    // Play nice with server and wait before attempting reconnect.
    if ( reconnectDelay > 0ms )
    {
        _reconnectDelayTimer.expires_from_now( reconnectDelay );
        // Timer should cleanly expire.
        co_await _reconnectDelayTimer.async_wait( asio::use_awaitable );
    }

    beast::error_code errorCode;
    // Look up the domain name.
    const auto resolveResults = co_await _tcpResolver.async_resolve
    (
        _host,
        _service,
        asio::redirect_error( asio::use_awaitable, errorCode )
    );
    if ( errorCode )
    {
        SYNTHFI_LOG_INFO( get_logger( ) )
            << fmt::format( "[{}][{}:{}{}] Resolve error: {}", name( ), _host, _service, _target, errorCode.what( ) );
        co_return;
    }

    _tcpSocket = std::make_unique< beast::ssl_stream< beast::tcp_stream > >( *_strand, _sslContext );
    // Set a timeout on the operation.
    beast::get_lowest_layer( *_tcpSocket ).expires_after( 30s );
    // Make the connection on the IP address we get from a lookup.
    auto connection = co_await beast::get_lowest_layer( *_tcpSocket ).async_connect( resolveResults, asio::redirect_error( asio::use_awaitable, errorCode ) );
    if ( errorCode )
    {
        SYNTHFI_LOG_INFO( get_logger( ) )
            << fmt::format( "[{}][{}:{}{}] Connect error: {}", name( ), _host, _service, _target, errorCode.what( ) );
        co_await _reconnectTimer.async_wait( asio::redirect_error( asio::use_awaitable, errorCode ) );
        if ( errorCode )
        {
            SYNTHFI_LOG_INFO( get_logger( ) )
                << fmt::format( "[{}][{}:{}{}] Reconnect timer error: {}", name( ), _host, _service, _target, errorCode.what( ) );
            co_return;
        }

        start_reconnect( reconnectDelay );
        co_return;
    }

    // Perform the SSL handshake.
    co_await _tcpSocket->async_handshake( asio::ssl::stream_base::client, asio::use_awaitable );

    SYNTHFI_LOG_INFO( get_logger( ) )
        << fmt::format( "[{}][{}:{}{}] Successfully opened connection: {}", name( ), _host, _service, _target, fmt::to_string( connection ) );

    beast::get_lowest_layer( *_tcpSocket ).socket( ).set_option( asio::socket_base::keep_alive( true ) );
    beast::get_lowest_layer( *_tcpSocket ).expires_never( );

    _reconnectTimer.cancel( );
    _isConnecting = false;

    asio::co_spawn( *get_strand( ), do_read( ), asio::detached );
}

asio::awaitable< void > JsonHttpSocket::do_write( )
{
    // Only a single writer coro should be writing at a time.
    _isWriting =  true;

    while ( _writeBuffer.size( ) > 0 )
    {
        beast::error_code errorCode;

        if ( _isConnecting )
        {
            co_await _reconnectTimer.async_wait( asio::redirect_error( asio::use_awaitable, errorCode ) );
            if ( errorCode && errorCode != asio::error::operation_aborted )
            {
                SYNTHFI_LOG_ERROR( get_logger( ) )
                    << fmt::format
                    (
                        "[{}][{}:{}{}] Reconnect timer error: {}",
                        name( ),
                        _host,
                        _service,
                        _target,
                        errorCode.what( )
                    );
                co_return;
            }
            if ( !errorCode )
            {
                    SYNTHFI_LOG_ERROR( get_logger( ) )
                        << fmt::format
                        (
                            "[{}][{}:{}{}] Reconnect timer timed out",
                            name( ),
                            _host,
                            _service,
                            _target
                        );

                co_return;
            }
        }

        auto bytesWritten = co_await beast::http::async_write
        (
            *_tcpSocket,
            _writeBuffer.front( ),
            asio::redirect_error( asio::use_awaitable, errorCode )
        );

        if ( bytesWritten > 0 )
        {
            SYNTHFI_LOG_TRACE( get_logger( ) )
                << fmt::format
                (
                    "[{}][{}:{}{}] Successfully wrote: {}",
                    name( ),
                    _host,
                    _service,
                    _target,
                    _writeBuffer.front( )
                );
            _writeBuffer.pop_front( );

            // Reader will spawn a new writer coro when it adds an item to write queue.
            _isWriting = false;
        }

        if ( errorCode )
        {
            SYNTHFI_LOG_ERROR( get_logger( ) )
                << fmt::format
                (
                    "[{}][{}:{}{}] Write error: {}, bytes written: {}",
                    name( ),
                    _host,
                    _service,
                    _target,
                    errorCode.what( ),
                    bytesWritten
                );
            _writeBuffer.clear( );
        }
    }
}

asio::awaitable< void > JsonHttpSocket::do_read( )
{
    beast::multi_buffer readBuffer; // Read buffer must be persisted between calls.

    // Read until no pending requests, or socket closed.
    while ( true )
    {
        beast::error_code errorCode;

        // Receive the HTTP response
        beast::http::response< PaddedResponseBody > httpResponse; // Message object should not have previous contents.
        size_t bytesRead = co_await beast::http::async_read( *_tcpSocket, readBuffer, httpResponse, asio::redirect_error( asio::use_awaitable, errorCode ) );

        if ( errorCode == asio::error::operation_aborted )
        {
            // Connection was closed by client.
            // TODO: cleanup pending requests.
            co_return;
        }
        else if
        (
            errorCode == beast::http::error::end_of_stream ||
            errorCode == boost::asio::ssl::error::stream_truncated

        )
        {
            // Connection was closed by server, attempt reconnect.
            // TODO: cleanup pending requests.
            SYNTHFI_LOG_ERROR( get_logger( ) )
                << fmt::format
                (
                    "[{}][{}:{}{}] Attempting to reconnect, connection closed by server with error: {}, bytes read: {} ",
                    name( ),
                    _host,
                    _service,
                    _target,
                    errorCode.what( ),
                    bytesRead
                );
            beast::get_lowest_layer( *_tcpSocket ).close( );
            start_reconnect( reconnect_delay( ) );

            co_return;
        }
        else if ( errorCode )
        {
            SYNTHFI_LOG_ERROR( get_logger( ) )
                << fmt::format
                (
                    "[{}][{}:{}{}] Read error: {}, code: {:d}, catagory: {}, bytes read: {}",
                    name( ),
                    _host,
                    _service,
                    _target,
                    errorCode.what( ),
                    errorCode.value( ),
                    fmt::to_string( errorCode.category( ).name( ) ),
                    bytesRead
                );
            co_return;
        }

        if ( httpResponse.result( ) != beast::http::status::ok )
        {
            SYNTHFI_LOG_ERROR( get_logger( ) )
                << fmt::format
                (
                    "[{}][{}:{}{}] Invalid response result code: {}",
                    name( ),
                    _host,
                    _service,
                    _target,
                    httpResponse.result( )
                );
        }
        else
        {
            // Valid response, pass json body up to layer above.
            try
            {
                const auto & paddedBody = httpResponse.body( );
                simdjson::ondemand::document doc = _parser.iterate( paddedBody.data( ), PaddedReader::size( paddedBody ), paddedBody.size( ) ).take_value( );

                SYNTHFI_LOG_TRACE( get_logger( ) ) << "Read: "
                    << std::string_view( reinterpret_cast< const char * >( paddedBody.data( ) ), PaddedReader::size( paddedBody ) );

                _onJsonMessageFn( std::move( doc ) );
            }
            catch ( const std::exception & ex)
            {
                SYNTHFI_LOG_ERROR( get_logger( ) )
                    << fmt::format
                    (
                        "[{}][{}:{}{}] Error handling json response: {}",
                        name( ),
                        _host,
                        _service,
                        _target,
                        ex.what( )
                    );
            }
        }
    }
    co_return;
}

} // namespace Io
} // namespace Synhfi

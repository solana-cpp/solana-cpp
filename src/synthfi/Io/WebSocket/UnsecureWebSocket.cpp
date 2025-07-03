#include "synthfi/Io/WebSocket/UnsecureWebSocket.hpp"

#include "synthfi/Util/Utils.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>

using namespace std::chrono_literals;

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{
namespace Io
{

UnsecureWebSocket::UnsecureWebSocket
(
    boost::asio::strand< boost::asio::io_context::executor_type > * strand,
    std::string_view host,
    std::string_view service,
    std::string_view target,
    OnMessageFn onMessageFn
)
    : WebSocketBase( strand, std::move( onMessageFn ) )
    , _host( host )
    , _service( service )
    , _target( target )
    , _tcpResolver( *get_strand( ) )
    , _webSocket( *get_strand( ) )
{
    // Set deadline timer for writers and check for reconnect failure.
    reconnect_timer( ).expires_from_now( 30s );
    boost::asio::co_spawn( *get_strand( ), do_connect( ), boost::asio::detached );
}

boost::asio::awaitable< void > UnsecureWebSocket::do_connect( )
{
    SYNTHFI_LOG_DEBUG( get_logger( ) ) << "Opening unsecure WebSocket connection: " << _host << ':' << _service;

    boost::beast::error_code errorCode;
    // Look up the domain name.
    const auto resolveResults = co_await _tcpResolver.async_resolve
    (
        _host,
        _service,
        boost::asio::redirect_error( boost::asio::use_awaitable, errorCode )
    );
    if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( get_logger( ) ) << "Resolve error: " << errorCode.message( );
        co_return;
    }

    // Set a timeout on the operation.
    boost::beast::get_lowest_layer( _webSocket ).expires_after( std::chrono::seconds{ 15 } );
    // Make the connection on the IP address we get from a lookup.
    const auto connection = co_await boost::beast::get_lowest_layer( _webSocket ).async_connect
    (
        resolveResults,
        boost::asio::redirect_error( boost::asio::use_awaitable, errorCode )
     );
    if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( get_logger( ) ) << "Connect error: " << errorCode.message( );
        co_return;
    }

    // Turn off the timeout on the tcp_stream, because the websocket stream has its own timeout system.
    boost::beast::get_lowest_layer( _webSocket ).expires_never( );

    // Update the host_ string. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    auto resolvedHost = _host + ':' + std::to_string( connection.port( ) );

    // Perform the websocket handshake
    co_await _webSocket.async_handshake( resolvedHost, _target, boost::asio::redirect_error( boost::asio::use_awaitable, errorCode ) );
    if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( get_logger( ) ) << "Handshake error: " << errorCode.message( );
        co_return;
    }

    SYNTHFI_LOG_INFO( get_logger( ) ) << "Successfully opened unsecure WebSocket connection: " << connection;

    reconnect_timer( ).cancel( );
    boost::asio::co_spawn( *get_strand( ), do_read( ), boost::asio::detached );

    co_return;
}

} // namespace Io
} // namespace Synhfi

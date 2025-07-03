#include "synthfi/Io/WebSocket/SSLWebSocket.hpp"

#include "synthfi/Util/Utils.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>

#include <fmt/core.h>
#include <fmt/ostream.h>

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{
namespace Io
{

SSLWebSocket::SSLWebSocket
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
    , _sslContext( asio::ssl::context::method::sslv23_client )
    , _webSocket( *get_strand( ), _sslContext )
{
    // TOOO: Verify the remote server's certificate.
    _sslContext.set_verify_mode( asio::ssl::verify_none );

    boost::asio::co_spawn( *get_strand( ), do_connect( ), boost::asio::detached );
}

boost::asio::awaitable< void > SSLWebSocket::do_connect( )
{
    SYNTHFI_LOG_DEBUG( get_logger( ) )
        << fmt::format( "[{}][{}:{}] Opening SSL WebSocket connection ", name( ), _host, _service );

    boost::system::error_code errorCode;
    const auto results = co_await _tcpResolver.async_resolve
    (
        _host,
        _service,
        boost::asio::redirect_error( boost::asio::use_awaitable, errorCode )
    );
    if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( get_logger( ) )
            << fmt::format( "[{}][{}:{}] Resolve error: {}", name( ), _host, _service, errorCode.what( ) );
        co_return;
    }

    boost::beast::get_lowest_layer( _webSocket ).expires_after( std::chrono::seconds ( 30 ) );
    const auto connection = co_await boost::beast::get_lowest_layer( _webSocket ).async_connect
    (
        results,
        boost::asio::redirect_error( boost::asio::use_awaitable, errorCode )
    );
    if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( get_logger( ) )
            << fmt::format( "[{}][{}:{}] Connect error: {}", name( ), _host, _service, errorCode.what( ) );
        co_return;
    }

    // Set SNI Hostname ( FTX needs this to handshake successfully ).
    if ( !SSL_set_tlsext_host_name( _webSocket.next_layer( ).native_handle( ), _host.c_str( ) ) )
    {
        errorCode.assign( static_cast< int >( ::ERR_get_error( ) ), boost::asio::error::get_ssl_category( ) );
        SYNTHFI_LOG_ERROR( get_logger( ) )
            << fmt::format( "[{}][{}:{}] Error setting SNI hostname: {}", name(), _host, _service, errorCode.what( ) );
        throw SynthfiError( errorCode.message( ) );
    }

    // Turn off the timeout on the tcp_stream, because the websocket stream has its own timeout system.
    boost::beast::get_lowest_layer( _webSocket ).expires_after( std::chrono::seconds ( 30 ) );
    // Set suggested timeout settings for the websocket.
    _webSocket.set_option( beast::websocket::stream_base::timeout::suggested( beast::role_type::client ) );
    // Perform the SSL handshake.
    co_await _webSocket.next_layer( ).async_handshake
    (
        boost::asio::ssl::stream_base::client,
        boost::asio::redirect_error( boost::asio::use_awaitable, errorCode )
    );
    if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( get_logger( ) )
            << fmt::format( "[{}][{}:{}] SSL handshake error: {}", name( ), _host, _service, errorCode.what( ) );
        co_return;
    }

    // Turn off the timeout on the tcp_stream, because the websocket stream has its own timeout system.
    beast::get_lowest_layer( _webSocket ).expires_never( );

    // Update the host string. This will provide the value of the
    // Host HTTP header during the WebSocket handshake.
    // See https://tools.ietf.org/html/rfc7230#section-5.4
    auto resolvedHost = _host + ':' + std::to_string( connection.port( ) );
    // Perform the websocket handshake
    co_await _webSocket.async_handshake( resolvedHost, _target, boost::asio::redirect_error( boost::asio::use_awaitable, errorCode ) );
    if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( get_logger( ) )
            << fmt::format( "[{}][{}:{}] WS handshake error: {}", name( ), _host, _service, errorCode.what( ) );
        co_return;
    }

    SYNTHFI_LOG_INFO( get_logger( ) )
        << fmt::format
        (
            "[{}][{}:{}{}] Successfully opened SSL WebSocket connection: {}",
            name( ),
            _host,
            _service,
            _target,
            fmt::to_string( connection )
        );

    reconnect_timer( ).cancel( );
    boost::asio::co_spawn( *get_strand( ), do_read( ), boost::asio::detached );

    co_return;

}

} // namespace Io
} // namespace Synhfi

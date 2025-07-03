#pragma once

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

#include <boost/asio/ssl/context.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>

#include <boost/json/value.hpp>

#include <simdjson.h>

#include <deque>
#include <functional>

namespace Synthfi
{
namespace Io
{

// Unsecure async JSON HTTP socket.
class JsonHttpSocket
{
public:
    using OnJsonMessageFn = std::function< void( simdjson::ondemand::document ) >;

    JsonHttpSocket
    (
        boost::asio::strand< boost::asio::io_context::executor_type > * strand,
        std::string_view host,
        std::string_view service,
        std::string_view target,
        OnJsonMessageFn onJsonMessageFn
    );

    boost::asio::awaitable< void > do_send_message( boost::json::value jsonRequest );
    boost::asio::awaitable< void > do_close( );

    constexpr std::string name( ) const & { return "JsonHttpSocket"; }

protected:
    SynthfiLogger & get_logger( ) { return _logger; }
    boost::asio::strand< boost::asio::io_context::executor_type > * get_strand( ) { return _strand; }

private:
    void start_reconnect( std::chrono::milliseconds reconnectDelay );

    boost::asio::awaitable< void > do_connect( std::chrono::milliseconds reconnectDelay );
    boost::asio::awaitable< void > do_write( );
    boost::asio::awaitable< void > do_read( );

    std::string _host;
    std::string _service;
    std::string _target;

    OnJsonMessageFn _onJsonMessageFn;

    // Asynchronous operations on a web socket must be performed within the same strand.
    boost::asio::strand< boost::asio::io_context::executor_type > * _strand;

    // Simultaneous connect attemps would reopen socket and handshake, cancelling pending reads.
    bool _isConnecting = false;
    bool _isWriting = false;

    // The connector waits on this timer before attempting reconnect.
    // This timer should help us play nicer with the solana server.
    boost::asio::high_resolution_timer _reconnectDelayTimer;

    // Writers wait on this timer for reconnect to complete or timeout before writing.
    boost::asio::high_resolution_timer _reconnectTimer;

    boost::asio::ssl::context _sslContext;
    boost::asio::ip::tcp::resolver _tcpResolver;
    std::unique_ptr< boost::beast::ssl_stream< boost::beast::tcp_stream > > _tcpSocket;

    std::deque< boost::beast::http::request< boost::beast::http::string_body > > _writeBuffer;

    simdjson::ondemand::parser _parser;

    mutable SynthfiLogger _logger;
};

} // namespace Io
} // namespace Synthfi

#pragma once

#include "synthfi/Io/WebSocket/WebSocketBase.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/strand.hpp>

#include <boost/beast/core/multi_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/websocket/stream.hpp>

#include <functional>
#include <span>

namespace Synthfi
{
namespace Io
{

// Solana rpc REST API client
class UnsecureWebSocket : public WebSocketBase< UnsecureWebSocket >
{
public:
    UnsecureWebSocket
    (
        boost::asio::strand< boost::asio::io_context::executor_type > * strand,
        std::string_view host,
        std::string_view service,
        std::string_view target,
        WebSocketBase::OnMessageFn onMessageFn
    );

    auto & get_next_layer( ) { return _webSocket; }

private:
    boost::asio::awaitable< void > do_connect( );

    std::string _host;
    std::string _service;
    std::string _target;

    boost::asio::ip::tcp::resolver _tcpResolver;
    boost::beast::websocket::stream< boost::beast::tcp_stream > _webSocket;
};

} // namespace Io
} // namespace Synthfi

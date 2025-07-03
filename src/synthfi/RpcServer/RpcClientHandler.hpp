#pragma once

#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClient.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/spawn.hpp>

#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>

namespace Synthfi
{

class RpcClientHandler
{
public:
    RpcClientHandler(
        Solana::SolanaHttpClient * rpcClient,
        boost::beast::tcp_stream tcpStream,
        boost::asio::io_context * ioContext );

private:
    using HttpRequest = boost::beast::http::request< boost::beast::http::string_body >;
    using HttpResponse = boost::beast::http::request< boost::beast::http::string_body >;

    void do_read( boost::asio::yield_context yield );

    void do_handle_request( const HttpRequest & request, boost::asio::yield_context yield );

    Solana::SolanaHttpClient * _rpcHttpClient;

    // Asynchronous operations on a tcp socket must be performed within the same strand.
    boost::asio::strand< boost::asio::io_context::executor_type > _io;
    boost::beast::tcp_stream _tcpStream;

    SynthfiLogger _logger;
};

} //namespace Synthfi

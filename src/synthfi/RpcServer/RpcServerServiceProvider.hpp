#pragma once

#include <boost/asio/io_context.hpp>

namespace Synthfi
{

template< typename Service >
class RpcServerServiceProvider
{
public:
    RpcServerServiceProvider
    (
        boost::asio::io_context & ioContext,
        std::string_view rpcHost,
        uint16_t rpcPort,
        boost::asio::ip::tcp::endpoint listenEndpoint
    )
        : _service( &boost::asio::make_service< Service >( ioContext, rpcHost, rpcPort, listenEndpoint ) )
    { }

private:
    Service * _service;
};

} // namespace Synthfi

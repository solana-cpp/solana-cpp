#pragma once

#include "synthfi/RpcServer/RpcClientHandler.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/execution_context.hpp>
#include <boost/asio/strand.hpp>

#include <unordered_map>

// include after system headers
#include <synthesizer/synthesizer.h>

namespace Synthfi
{

class RpcServerService : public boost::asio::execution_context::service
{
public:
    RpcServerService
    (
        boost::asio::execution_context & executionContext,
        std::string_view rpcHost,
        uint16_t rpcPort,
        boost::asio::ip::tcp::endpoint listenEndpoint
    );

    static inline boost::asio::execution_context::id id;

private:
    // Destroy all user-defined handler objects owned by the service.
    void shutdown( ) noexcept override
    {
        boost::system::error_code errorCode;
        _tcpAcceptor.close( errorCode );
        if ( errorCode )
        {
            SYNTHFI_LOG_ERROR( _logger ) << "Close error: " << errorCode.message( );
        }
    }

    boost::asio::awaitable< void > do_listen( boost::asio::ip::tcp::endpoint listenEndpoint );
    boost::asio::awaitable< void > do_accept( boost::asio::ip::tcp::socket socket );

    // Private io_context used for performing logging operations.
    boost::asio::io_context _ioContext;
    // A work-tracking executor giving work for the private io_context to perform.
    boost::asio::any_io_executor _work;
    std::jthread _workThread;

    boost::asio::ip::tcp::acceptor _tcpAcceptor;

    Solana::SolanaHttpClient _rpcHttpClient;
    std::unordered_map< uint16_t, RpcClientHandler > _rpcClientHandlers;

    SynthfiLogger _logger;
};

} // namespace Synthfi

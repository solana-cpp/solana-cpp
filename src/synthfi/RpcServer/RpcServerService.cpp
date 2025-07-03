#include "synthfi/RpcServer/RpcServer.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{

RpcServerService::RpcServerService
(
    boost::asio::execution_context & executionContext,
    std::string_view rpcHost,
    uint16_t rpcPort,
    boost::asio::ip::tcp::endpoint listenEndpoint
)
    : boost::asio::execution_context::service( executionContext )
    , _work( boost::asio::require( _ioContext.get_executor( ),
             boost::asio::execution::outstanding_work.tracked ) )
    , _workThread( [ this ]( ){ return this->_ioContext.run( ); } )
    , _tcpAcceptor( _ioContext )
    , _rpcHttpClient( _ioContext, rpcHost, rpcPort )
{
    SYNTHFI_LOG_INFO( _logger ) << "Listening on: " <<  rpcHost << ":" << std::to_string( rpcPort );

    boost::asio::co_spawn
    (
        _ioContext,
        do_listen( std::move( listenEndpoint ) ),
        boost::asio::detached
    );
}

asio::awaitable< void > RpcServerService::do_listen( boost::asio::ip::tcp::endpoint listenEndpoint )
{
    beast::error_code errorCode;
    _tcpAcceptor.open( listenEndpoint.protocol( ), errorCode );
    if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Open error: " << errorCode;
        co_return;
    }

    // Allow address reuse
    _tcpAcceptor.set_option( asio::socket_base::reuse_address( true ), errorCode );
    if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Error setting tcp acceptor options: " << errorCode.message( );
        co_return;
    }

    // Bind to the server address
    _tcpAcceptor.bind( listenEndpoint, errorCode );
    if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Bind error: " << errorCode.message( );
        co_return;
    }

    // Start listening for connections
    _tcpAcceptor.listen( asio::socket_base::max_listen_connections, errorCode );
    if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Listen error: " << errorCode.message( );
        co_return;
    }

    while ( true )
    {
        asio::ip::tcp::socket clientSocket( _ioContext );
        co_await _tcpAcceptor.async_accept( clientSocket, boost::asio::redirect_error( boost::asio::use_awaitable, errorCode ) );
        if ( errorCode )
        {
            SYNTHFI_LOG_ERROR( _logger ) << "Accept error: " << errorCode.message( );
            co_return;
        }
        boost::asio::co_spawn
        (
            _ioContext,
            do_accept( std::move( clientSocket ) ),
            boost::asio::detached
        );
    }
}

asio::awaitable< void > RpcServerService::do_accept( asio::ip::tcp::socket socket )
{
    _rpcClientHandlers.emplace
    (
        std::piecewise_construct,
        std::forward_as_tuple( socket.remote_endpoint( ).port( ) ),
        std::forward_as_tuple( &_rpcHttpClient, beast::tcp_stream( std::move( socket ) ), &_ioContext )
    );
    co_return;
}

} // namespace Synthfi

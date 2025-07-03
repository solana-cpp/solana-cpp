#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClientImpl.hpp"

#include <boost/asio/use_future.hpp>

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{
namespace Solana
{

SolanaHttpClientImpl::SolanaHttpClientImpl
(
    asio::io_context & ioContext,
    Statistics::StatisticsPublisherService & statisticsPublisherService,
    const SolanaEndpointConfig & config
)

    : _strand( ioContext.get_executor( ) )
    , _statisticsPublisher( statisticsPublisherService, "solana_http_api" )
    , _jsonHttpSocket
    (
        &_strand,
        &_statisticsPublisher,
        config.host,
        config.service,
        config.target,
        std::bind( &SolanaHttpClientImpl::on_message, this, std::placeholders::_1 )
    )
{ }

void SolanaHttpClientImpl::shutdown( )
{
    boost::asio::co_spawn( _strand, _jsonHttpSocket.do_close( ), boost::asio::use_future ).get( );
}

void SolanaHttpClientImpl::on_message( simdjson::ondemand::document message )
{
    SYNTHFI_LOG_ERROR( _logger )
        << fmt::format( "[{}] Received invalid message: {}", name( ), message.raw_json( ).value( ) );
    return;
}

} // namespace Solana
} // namespace Synhfi

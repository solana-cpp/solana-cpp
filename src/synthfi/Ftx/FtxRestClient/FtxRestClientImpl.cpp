#include "synthfi/Ftx/FtxRestClient/FtxRestClientImpl.hpp"

#include "synthfi/Ftx/FtxUtils.hpp"

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Util/StringEncode.hpp"

#include <boost/json/serialize.hpp>
namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{

namespace Ftx
{

FtxRestClientImpl::FtxRestClientImpl
(
    asio::io_context & ioContext,
    Statistics::StatisticsPublisherService & statisticsPublisherService,
    const FtxAuthenticationConfig & config
)
    : _host( config.host )
    , _apiKey( config.apiKey )
    , _apiSecret( config.apiSecret )
    , _strand( ioContext.get_executor( ) )
    , _statisticsPublisher( statisticsPublisherService, "ftx_rest_api" )
    , _restRequestCounter
    (
        "ftx_rest_api_requests",
        { { "source", "ftx" } },
        "count",
        0UL,
        &_strand,
        &_statisticsPublisher
    )
    , _sslContext( asio::ssl::context::method::sslv23_client )
    , _tcpResolver( _strand )
{
    // TOOO: Verify the remote server's certificate.
    _sslContext.set_verify_mode( asio::ssl::verify_none );
}

boost::asio::awaitable< boost::beast::ssl_stream< boost::beast::tcp_stream > > FtxRestClientImpl::open_connection( )
{
    boost::beast::ssl_stream< boost::beast::tcp_stream > sslSocket( _strand, _sslContext );

    const auto results = co_await _tcpResolver.async_resolve( _host, "https", boost::asio::use_awaitable );

    // Set SNI Hostname ( FTX needs this to handshake successfully ).
    boost::system::error_code errorCode;
    if ( !SSL_set_tlsext_host_name( sslSocket.native_handle( ), _host.c_str( ) ) )
    {
        errorCode.assign( static_cast< int >( ::ERR_get_error( ) ), boost::asio::error::get_ssl_category( ) );
        SYNTHFI_LOG_ERROR( _logger ) << "Error setting SNI hostname: " << errorCode.message( );
        throw SynthfiError( errorCode.message( ) );
    }

    boost::beast::get_lowest_layer( sslSocket ).expires_after( std::chrono::seconds ( 30 ) );
    const auto connection = co_await boost::beast::get_lowest_layer( sslSocket ).async_connect( results, boost::asio::use_awaitable );
    SYNTHFI_LOG_INFO( _logger )
        << fmt::format( "[{}] Connected to FTX REST API: {}", name( ), fmt::to_string( connection ) );

    // Perform the SSL handshake.
    co_await sslSocket.async_handshake( boost::asio::ssl::stream_base::client, boost::asio::use_awaitable );

    co_return std::move( sslSocket );
}

beast::http::request< beast::http::empty_body > FtxRestClientImpl::build_get_request( std::string endpoint ) const
{
    boost::beast::http::request< boost::beast::http::empty_body > request;

    request.method( boost::beast::http::verb::get );
    request.target( std::move( endpoint ) );

    request.set( boost::beast::http::field::host, _host );

    authenticate_request< false >
    (
        request,
        _apiKey,
        _apiSecret,
        _clock.now( )
    );

    return request;
}

beast::http::request< beast::http::string_body > FtxRestClientImpl::build_post_request( std::string endpoint, boost::json::value body ) const
{
    boost::beast::http::request< boost::beast::http::string_body > request;

    request.method( boost::beast::http::verb::post );
    request.target( std::move( endpoint ) );

    request.body( ) = boost::json::serialize( std::move( body ) );
    request.prepare_payload( );
    request.set( boost::beast::http::field::content_type, "application/json" );
    request.set( boost::beast::http::field::host, _host );

    authenticate_request< true >
    (
        request,
        _apiKey,
        _apiSecret,
        _clock.now( )
    );

    return request;
}

} // namespace Ftx

} // namespace Synthfi

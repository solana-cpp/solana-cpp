#pragma once

#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"
#include "synthfi/Ftx/FtxTypes.hpp"

#include "synthfi/Util/JsonBody.hpp"
#include "synthfi/Util/Logger.hpp"
#include "synthfi/Util/JsonUtils.hpp"
#include "synthfi/Util/Utils.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/ssl/context.hpp>

#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/strand.hpp>

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/empty_body.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <boost/beast/http/write.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>

#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <boost/json/value_from.hpp>

#include <chrono>

#include <fmt/core.h>
#include <fmt/ostream.h>


namespace Synthfi
{

namespace Ftx
{

class FtxRestClientImpl
{
public:
    FtxRestClientImpl
    (
        boost::asio::io_context & ioContext,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        const FtxAuthenticationConfig & config
    );

    template< class RequestType, class ResponseType, class CompletionHandlerType >
    void send_request( RequestType request, CompletionHandlerType completionHandler )
    requires( RequestType::request_verb( ) == boost::beast::http::verb::get )
    {
        boost::asio::co_spawn( _strand, do_send_get_request< RequestType, ResponseType >( std::move( request ) ), completionHandler );
        return;
    }

    template< class RequestType, class ResponseType, class CompletionHandlerType >
    void send_request( RequestType request, CompletionHandlerType completionHandler )
    requires( RequestType::request_verb( ) == boost::beast::http::verb::post )
    {
        boost::asio::co_spawn( _strand, do_send_post_request< RequestType, ResponseType >( std::move( request ) ), completionHandler );
        return;
    }

    constexpr std::string name( ) const & { return "FtxRestClient"; }

private:
    // Returns number of milliseconds since Unix epoch.
    std::string now_string( ) const;

    boost::beast::http::request< boost::beast::http::empty_body > build_get_request( std::string endpoint ) const;
    boost::beast::http::request< boost::beast::http::string_body > build_post_request( std::string endpoint, boost::json::value body ) const;

    boost::asio::awaitable< boost::beast::ssl_stream< boost::beast::tcp_stream > > open_connection( );

    template< class RequestType, class ResponseType >
    boost::asio::awaitable< ResponseType > do_send_get_request( RequestType request )
    {
        auto connection = co_await open_connection( );

        auto httpRequest = build_get_request( request.target( ) );
        SYNTHFI_LOG_TRACE( _logger )
            << fmt::format( "[{}] Sending GET request: {}", name( ), httpRequest );
        co_await boost::beast::http::async_write( connection, std::move( httpRequest ), boost::asio::use_awaitable );

        // Update statistics.
        _restRequestCounter.increment( 1 );

        boost::beast::http::response< PaddedResponseBody > httpResponse;
        {
            boost::beast::flat_buffer readBuffer;
            co_await boost::beast::http::async_read( connection, readBuffer, httpResponse, boost::asio::use_awaitable );
        }

        if ( httpResponse.result( ) != boost::beast::http::status::ok )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << fmt::format( "[{}] Received invalid FTX response, status code: {}", name( ), httpResponse.result( ) );
             throw SynthfiError( "Invalid status code" );
        }

        auto & paddedBody = httpResponse.body( );
        simdjson::ondemand::document doc = _parser.iterate( paddedBody.data( ), PaddedReader::size( paddedBody ), paddedBody.size( ) ).value( );

        SYNTHFI_LOG_TRACE( _logger )
            << fmt::format
            (
                "[{}] Received response: {}",
                name( ),
                std::string_view( reinterpret_cast< const char * >( &paddedBody[ 0 ] ), PaddedReader::size( paddedBody ) )
            );

        simdjson::ondemand::value jsonValue( doc );
        co_return json_to< ResponseType >( jsonValue[ "result" ].value( ) );
    }

    template< class RequestType, class ResponseType >
    boost::asio::awaitable< ResponseType > do_send_post_request( RequestType request )
    {
        auto connection = co_await open_connection( );

        auto httpRequest = build_post_request( request.target( ), boost::json::value_from( request ) );

        SYNTHFI_LOG_TRACE( _logger )
            << fmt::format( "[{}] Sending POST request: {}", name( ), httpRequest );

        co_await boost::beast::http::async_write( connection, httpRequest, boost::asio::use_awaitable );

        // Update statistics.
        _restRequestCounter.increment( 1 );

        boost::beast::http::response< PaddedResponseBody > httpResponse;
        {
            boost::beast::flat_buffer readBuffer;
            co_await boost::beast::http::async_read( connection, readBuffer, httpResponse, boost::asio::use_awaitable );
        }

        if ( httpResponse.result( ) != boost::beast::http::status::ok )
        {
            SYNTHFI_LOG_ERROR( _logger )
                << fmt::format( "[{}] Received invalid FTX response, status code: {}", name( ), httpResponse.result( ) );
 
            throw SynthfiError( "Invalid status code" );
        }

        auto & paddedBody = httpResponse.body( );
        simdjson::ondemand::document doc = _parser.iterate( paddedBody.data( ), PaddedReader::size( paddedBody ), paddedBody.size( ) ).value( );

        SYNTHFI_LOG_TRACE( _logger )
            << fmt::format
            (
                "[{}] Received response: {}",
                name( ),
                std::string_view( reinterpret_cast< const char * >( &paddedBody[ 0 ] ), PaddedReader::size( paddedBody ) )
            );

        simdjson::ondemand::value jsonValue( doc );
        co_return json_to< ResponseType >( jsonValue[ "result" ] );
    }

    std::string _host;
    std::string _apiKey;
    std::string _apiSecret;

    // Asynchronous operations on a tcp socket must be performed within the same strand.
    boost::asio::strand< boost::asio::io_context::executor_type > _strand;

    Statistics::StatisticsPublisher _statisticsPublisher;
    Statistics::Counter< uint64_t, Statistics::StatisticsPublisher > _restRequestCounter;

    // SSL context.
    boost::asio::ssl::context _sslContext;
    boost::asio::ip::tcp::resolver _tcpResolver;

    simdjson::ondemand::parser _parser;

    std::chrono::high_resolution_clock _clock;


    mutable SynthfiLogger _logger;
};

} // namespace Ftx

} // namespace Synthfi

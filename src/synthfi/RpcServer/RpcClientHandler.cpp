#include "synthfi/RpcServer/RpcClientHandler.hpp"
#include "synthfi/RpcServer/ClientRequest.hpp"

#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/http/parser.hpp>
#include <boost/beast/http/read.hpp>
#include <boost/beast/http/write.hpp>

#include <boost/json/parser.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>

#include "synthfi/Util/Logger.hpp"

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{

RpcClientHandler::RpcClientHandler( Solana::SolanaHttpClient * rpcHttpClient, beast::tcp_stream tcpStream, boost::asio::io_context * ioContext )
    : _rpcHttpClient( rpcHttpClient )
    , _io( ioContext->get_executor( ) )
    , _tcpStream( std::move( tcpStream  ) )
{
    asio::spawn( _io, std::bind( &RpcClientHandler::do_read, this, std::placeholders::_1 ) );
}

void RpcClientHandler::do_read( asio::yield_context yield )
{
    beast::error_code errorCode;
    beast::flat_buffer buffer;
    beast::http::request< beast::http::string_body > httpRequest;
    while( true )
    {
        beast::http::async_read( _tcpStream, buffer, httpRequest, yield[ errorCode ] );
        if ( errorCode )
        {
            SYNTHFI_LOG_ERROR( _logger ) << "Read error: " << errorCode.message( );
            _tcpStream.close( );
            return;
        }
        SYNTHFI_LOG_INFO( _logger ) << "Received request: " << httpRequest;
        asio::spawn( _tcpStream.get_executor( ), std::bind( &RpcClientHandler::do_handle_request, this, httpRequest, std::placeholders::_1 ) );
    }
}

void RpcClientHandler::do_handle_request( const HttpRequest & httpRequest, boost::asio::yield_context yield )
{
    boost::system::error_code errorCode;

    const auto doWrite = [ this ]( auto & httpResponse, auto & yield, auto & errorCode )
    {
        httpResponse.prepare_payload( );
        // TODO: unsafe https://stackoverflow.com/questions/36016135/asioasync-write-and-strand
        beast::http::async_write( this->_tcpStream, httpResponse, yield[ errorCode ] );
        if ( errorCode )
        {
            SYNTHFI_LOG_ERROR( _logger ) << "write error: " << errorCode.message( );
            return;
        }
        SYNTHFI_LOG_INFO( _logger ) << "Sent response: " << httpResponse;
    };

    beast::http::response< beast::http::string_body > httpResponse;
    if ( httpRequest.method( ) != beast::http::verb::post )
    {
        httpResponse.result( beast::http::status::method_not_allowed );
        httpResponse.body( ) = "Used HTTP Method is not allowed. POST or OPTIONS is required";

        doWrite( httpResponse, yield, errorCode );
        return;
    }

    const auto & findContentType = httpRequest.find( beast::http::field::content_type );
    if ( findContentType == httpRequest.end( ) || findContentType->value( ) != "application/json" )
    {
        httpResponse.result( beast::http::status::unsupported_media_type );
        httpResponse.body( ) = "Supplied content type is not allowed. Content-Type: application/json is required";

        doWrite( httpResponse, yield, errorCode );
        return;
    }

    httpResponse.result( beast::http::status::ok );
    httpResponse.set( beast::http::field::content_type, "application/json" );

    boost::json::object jsonResponse;
    jsonResponse.emplace( "jsonrpc", "2.0" );
    auto & responseId = jsonResponse.emplace( "id", boost::json::value{ } ).first->value( );

    boost::json::parser jsonParser;
    jsonParser.write( httpRequest.body( ).data( ), errorCode );
    if ( errorCode )
    {
        jsonResponse.emplace( "error", boost::json::value_from< ErrorResponse >( ErrorResponse{ "parse error" } ) );
        httpResponse.body( ) = boost::json::serialize( jsonResponse );

        doWrite( httpResponse, yield, errorCode );
        return;
    }

    const auto jsonRequest = jsonParser.release( ).as_object( );

    const auto & findId = jsonRequest.find( "id" );
    if ( findId == jsonRequest.end( ) )
    {
        // Send empty response ( observed behavior of Solana RPC API ).
        doWrite( httpResponse, yield, errorCode );
        return;
    }
    responseId = findId->value( );

    const auto & findMethod = jsonRequest.find( "method" );
    if ( findMethod == jsonRequest.end( ) || !findMethod->value( ).is_string( ) )
    {
        jsonResponse.emplace( "error", boost::json::value_from< ErrorResponse >( ErrorResponse{ "method missing" } ) );
        httpResponse.body( ) = boost::json::serialize( jsonResponse );

        doWrite( httpResponse, yield, errorCode );
        return;
    }

    auto paramsJson = jsonResponse[ "params" ].value( );

    const auto & methodName = findMethod->value( ).as_string( );
    if ( methodName == GetSynthfiInfoRequest::method_name( ) )
    {
        try
        {
            auto request = boost::json::value_to< GetSynthfiInfoRequest >( paramsJson );
            // TODO: build response.
            auto response = GetSynthfiInfoResponse{ };
            jsonResponse.emplace( "result", boost::json::value_from< GetSynthfiInfoResponse >( std::move( response ) ) );
        }
        catch ( std::exception & ex )
        {
            jsonResponse.emplace( "error", boost::json::value_from< ErrorResponse >( ErrorResponse{ "invalid request" } ) );
        }
    }
    else if ( methodName == GetSynthfiAccountInfoRequest::method_name( ) )
    {
        try
        {
            auto request = boost::json::value_to< GetSynthfiAccountInfoRequest >( params );
            // TODO: build response.
            auto response = GetSynthfiAccountInfoResponse{ };
            jsonResponse.emplace( "result", boost::json::value_from< GetSynthfiAccountInfoResponse >( std::move( response ) ) );
        }
        catch ( std::exception & ex )
        {
            jsonResponse.emplace( "error", boost::json::value_from< ErrorResponse >( ErrorResponse{ "invalid request" } ) );
        }
    }
    else
    {
        jsonResponse.emplace( "error", boost::json::value_from< ErrorResponse >( ErrorResponse{ "invalid method" } ) );
    }

    httpResponse.body( ) = boost::json::serialize( jsonResponse );
    doWrite( httpResponse, yield, errorCode );
}

} // namespace Synthfi

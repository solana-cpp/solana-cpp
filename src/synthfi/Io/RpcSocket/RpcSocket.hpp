#pragma once

#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

#include "synthfi/Util/Utils.hpp"
#include "synthfi/Util/JsonUtils.hpp"
#include "synthfi/Util/Logger.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <simdjson.h>

#include <chrono>
#include <unordered_map>

namespace Synthfi
{
namespace Io
{

template< class RequestType >
static boost::json::object build_rpc_request( uint64_t id, RequestType request )
{
    return boost::json::object
    {
        { "jsonrpc", "2.0" },
        { "id", id },
        { "method", RequestType::method_name( ) },
        { "params", boost::json::value_from( request ) }
    };
}

template< class NextLayer >
class RpcSocket : public NextLayer
{
public:
    using OnJsonMessageFn = NextLayer::OnJsonMessageFn;

    RpcSocket
    (
        boost::asio::strand< boost::asio::io_context::executor_type > * strand,
        Statistics::StatisticsPublisher * statisticsPublisher,
        std::string_view host,
        std::string_view service,
        std::string_view target,
        OnJsonMessageFn onJsonMessageFn
    )
        : NextLayer( strand, host, service, target, std::bind( &RpcSocket::on_message, this, std::placeholders::_1 ) )
        , _messagesSentCounter
        (
            "solana_rpc_api_sent",
            { { "source", "solana" } },
            "count",
            0UL,
            strand,
            statisticsPublisher
        )
        , _messagesReceivedCounter
        (
            "solana_rpc_api_received",
            { { "source", "solana" } },
            "count",
            0UL,
            strand,
            statisticsPublisher
        )
        , _onJsonMessageFn( onJsonMessageFn )
    { }

    template< class RequestType, class ResponseType >
    boost::asio::awaitable< ResponseType > do_send_request( RequestType request )
    {
        const auto id = _nextRequestId++;

        co_await NextLayer::do_send_message( build_rpc_request( id, std::move( request ) ) );
        _messagesSentCounter.increment( 1 );

        auto makeRequest = std::make_unique< PendingRequest< ResponseType > >( NextLayer::get_strand( ), std::chrono::seconds( 30 ) );
        auto * pendingRequest = _pendingRequests.emplace
        (
            id,
            std::move( makeRequest )
        ).first->second.get( );

        boost::system::error_code errorCode;
        // Wait for response to succeed or timeout.
        co_await pendingRequest->expiryTimer.async_wait( boost::asio::redirect_error( boost::asio::use_awaitable, errorCode ) );

        // Complete request.
        if ( errorCode == boost::asio::error::operation_aborted )
        {
            // Operation succeeded.
            if ( pendingRequest->error )
            {
                SYNTHFI_LOG_ERROR( NextLayer::get_logger( ) )
                    << fmt::format( "[{}] Completed request with error", name( ) );
                // Rethrow error.
                auto ex = std::move( pendingRequest->error );
                _pendingRequests.erase( id );
                throw ex;
            }

            auto response = std::move( reinterpret_cast< const PendingRequest< ResponseType > * >( pendingRequest )->response );
            _pendingRequests.erase( id );
            co_return response;
        }
        else if ( errorCode )
        {
            // Expiry timer error.
            SYNTHFI_LOG_ERROR( NextLayer::get_logger( ) )
                << fmt::format( "[{}] Request timer error: {}, id: {}", name( ), errorCode.what( ), id );
            _pendingRequests.erase( id );
            throw SynthfiError( errorCode.message( ) );
        }
        else
        {
            // Timed-out.
            _pendingRequests.erase( id );
            SYNTHFI_LOG_ERROR( NextLayer::get_logger( ) )
                << fmt::format( "[{}] Request timed out, id: {}", name( ), id );

            throw SynthfiError( "Timed out" );
        }
    }

    constexpr std::string name( ) const & { return "RpcSocket"; }

private:
    void on_message( simdjson::ondemand::document responseDocument )
    {
        _messagesReceivedCounter.increment( 1 );

        try
        {
            simdjson::ondemand::object responseObject( responseDocument );
            auto id = responseObject[ "id" ];
            if ( id.error( ) )
            {
                // JSON RPC control message.
                responseDocument.rewind( );
                _onJsonMessageFn( std::move( responseDocument) );
                return;
            }

            auto findPendingRequest = _pendingRequests.find( id.get_uint64( ) );
            if ( findPendingRequest == _pendingRequests.end( ) )
            {
                SYNTHFI_LOG_ERROR( NextLayer::get_logger( ) )
                    << fmt::format( "[{}] Invalid response id: {}", name( ), id.get_uint64( ).value( ) );
                return;
            }

            auto error = responseObject[ "error" ];
            if ( !error.error( ) )
            {
                SYNTHFI_LOG_ERROR( NextLayer::get_logger( ) )
                    << fmt::format( "[{}] Received error response: {}", name( ), error.error( ) );
                findPendingRequest->second->error = std::make_exception_ptr( simdjson::to_json_string( error ) );
            }
            else
            {
                findPendingRequest->second->set_result( responseObject[ "result" ].value( ) );
                SYNTHFI_LOG_TRACE( NextLayer::get_logger( ) )
                    << fmt::format( "[{}] Successfully read RPC response", name( ) );
            }

            // Notify writer of request that response is successful.
            findPendingRequest->second->expiryTimer.cancel( );
        }
        catch( const std::exception & ex )
        {
            SYNTHFI_LOG_ERROR( NextLayer::get_logger( ) )
                << fmt::format( "[{}] Error deserializing RPC message: {}", name( ), ex.what( ) );
        }
    }

    struct PendingRequestBase
    {
        PendingRequestBase( boost::asio::strand< boost::asio::io_context::executor_type > * strand, std::chrono::seconds timeout )
            : expiryTimer( *strand, timeout )
        { }

        virtual ~PendingRequestBase( ) = default;

        virtual void set_result( simdjson::ondemand::value jsonValue ) = 0;

        boost::asio::high_resolution_timer expiryTimer;
        std::exception_ptr error;
    };

    template< class ResponseType >
    struct PendingRequest : PendingRequestBase
    {
        PendingRequest( boost::asio::strand< boost::asio::io_context::executor_type > * strand, std::chrono::seconds timeout )
            : PendingRequestBase( strand, timeout )
        { }

        virtual ~PendingRequest( ) = default;

        void set_result( simdjson::ondemand::value jsonValue ) override
        {
            try
            {
                response = json_to< ResponseType >( jsonValue );
            }
            catch ( const std::exception & ex )
            {
                this->error = std::make_exception_ptr( ex );
            }
        };

        ResponseType response;
    };

    Statistics::Counter< uint64_t, Statistics::StatisticsPublisher > _messagesSentCounter;
    Statistics::Counter< uint64_t, Statistics::StatisticsPublisher > _messagesReceivedCounter;

    OnJsonMessageFn _onJsonMessageFn;

    uint32_t _nextRequestId = 0;
    std::unordered_map< uint64_t, std::unique_ptr< PendingRequestBase > > _pendingRequests;
};

} // namespace Io
} // namespace Synthfi

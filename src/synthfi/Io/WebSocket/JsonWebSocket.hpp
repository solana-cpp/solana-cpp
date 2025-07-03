#pragma once

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/strand.hpp>

#include <boost/json/serialize.hpp>
#include <boost/json/stream_parser.hpp>
#include <boost/json/value.hpp>

#include <simdjson.h>

#include <span>

namespace Synthfi
{
namespace Io
{

template< class NextLayer >
class JsonWebSocket : public NextLayer
{
public:
    using OnJsonMessageFn = std::function< void( simdjson::ondemand::document ) >;

    JsonWebSocket
    (
        boost::asio::strand< boost::asio::io_context::executor_type > * strand,
        std::string_view host,
        std::string_view service,
        std::string_view target,
        OnJsonMessageFn onJsonMessageFn
    )
        : NextLayer(strand, host, service, target, std::bind( &JsonWebSocket::on_message, this, std::placeholders::_1, std::placeholders::_2 ) )
        , _onJsonMessageFn( std::move( onJsonMessageFn ) )
    { }

    boost::asio::awaitable< void > do_send_message( boost::json::value message )
    {
        co_await NextLayer::do_send_message( boost::json::serialize( message ) );
        co_return;
    }

    constexpr std::string name( ) const & { return "JsonWebSocket"; }

protected:
    boost::asio::strand< boost::asio::io_context::executor_type > * get_strand( ) { return NextLayer::get_strand( ); }

private:
    void on_message( std::string_view message, size_t capacity )
    {
        try
        {
            _onJsonMessageFn( _parser.iterate( message, capacity ) );
        }
        catch ( const std::exception & ex )
        {
            SYNTHFI_LOG_ERROR( NextLayer::get_logger( ) ) <<
                fmt::format( "[{}] Error deserializing json message: {}", name( ), ex.what( ) );
        }
    }

    OnJsonMessageFn _onJsonMessageFn;
    simdjson::ondemand::parser _parser;
};

} // namespace Io
} // namespace Synthfi

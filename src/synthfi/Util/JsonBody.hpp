#pragma once

#include <boost/asio/buffer.hpp>
#include <boost/beast/core/buffers_range.hpp>
#include <boost/beast/http/message.hpp>

#include <simdjson.h>

namespace Synthfi
{

// https://www.boost.org/doc/libs/develop/libs/beast/doc/html/beast/concepts/Body.html

struct PaddedReader
{
    template< bool isRequest, class Fields >
    PaddedReader( boost::beast::http::header< isRequest, Fields > & header, std::vector< uint8_t > & body )
        : _body( body )
    { }

    void init( const boost::optional< std::uint64_t > & contentLength, boost::system::error_code & errorCode )
    {
        if ( contentLength )
        {
            _body.reserve( contentLength.value( ) + simdjson::SIMDJSON_PADDING );
        }
        else
        {
            _body.reserve( simdjson::SIMDJSON_PADDING );
        }
        // The specification requires this to indicate "no error"
        errorCode = { };
    }

    static std::uint64_t size( const std::vector< uint8_t > & body )
    {
        BOOST_ASSERT_MSG( body.size( ) > simdjson::SIMDJSON_PADDING, "Body missing padding" );

        return body.size( ) - simdjson::SIMDJSON_PADDING;
    }

    // This is called zero or more times with parsed body octets.
    template< class ConstBufferSequence >
    std::size_t put( const ConstBufferSequence & buffers, boost::system::error_code & errorCode )
    {

        auto const putBytes = boost::beast::buffer_bytes( buffers );
        auto const size = _body.size( );

        _body.resize( putBytes + size );

        auto it = _body.begin( ) + size;
        for ( auto putBuffer : boost::beast::buffers_range_ref( buffers ) )
        {
            it = std::copy
            (
                static_cast< const uint8_t * >( putBuffer.data( ) ),
                static_cast< const uint8_t * >( putBuffer.data( ) ) + putBuffer.size( ),
                it
            );
        }

        errorCode = { };

        return putBytes;
    }

    // Called when the body is complete.
    void finish( boost::system::error_code & errorCode )
    {
        _body.resize( _body.size( ) + simdjson::SIMDJSON_PADDING );

        errorCode = { };
    }

private:
    // The buffer must have at least SIMDJSON_PADDING extra allocated bytes.
    // It does not matter what those bytes are initialized to, as long as they are allocated.
    std::vector< uint8_t > & _body;
};

struct PaddedResponseBody
{
    // The type of message::body when used.
    using value_type = std::vector< uint8_t >;

    // The algorithm used during parsing
    using reader = PaddedReader;
};
static_assert( boost::beast::http::is_body_reader< PaddedResponseBody >::value, "JsonBody does not implement the Body concept." );


} // namespace Synthfi
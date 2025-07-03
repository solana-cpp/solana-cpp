#pragma once

#include "synthfi/Trading/PriceQuantity.hpp"
#include "synthfi/Trading/Side.hpp"

#include <boost/json/value_from.hpp>

#include <chrono>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/chrono.h>

namespace Synthfi
{
namespace Trading
{

class Book
{
public:
    using SideBook = std::vector< PriceQuantity >;
    using Timestamp = std::chrono::time_point< std::chrono::system_clock >;

    Book( ) = default;

    Book
    (
        std::chrono::high_resolution_clock::time_point exchangeTimestamp,
        std::chrono::high_resolution_clock::time_point receiveTimestamp
    )
        : _exchangeTimestamp( std::move( exchangeTimestamp ) )
        , _receiveTimestamp( std::move( receiveTimestamp ) )
    { }

    constexpr bool is_valid( ) const & { return !_sideBooks.get( Side::bid ).empty( ) && !_sideBooks.get( Side::ask ).empty( ); }

    constexpr auto & get( Side side ) & { return  _sideBooks.get( side ); }
    constexpr const auto & get( Side side ) const & { return _sideBooks.get( side ); }
    constexpr auto && get( Side side ) && { return std::move( _sideBooks.get( side ) ); }

    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const Book & book );

    const std::chrono::high_resolution_clock::time_point & exchange_timestamp( ) const & { return _exchangeTimestamp; }
    const std::chrono::high_resolution_clock::time_point & receive_timestamp( ) const & { return _receiveTimestamp; }

    std::chrono::high_resolution_clock::time_point & exchange_timestamp( ) & { return _exchangeTimestamp; }
    std::chrono::high_resolution_clock::time_point & receive_timestamp( ) & { return _receiveTimestamp; }

private:
    std::chrono::high_resolution_clock::time_point _exchangeTimestamp; // Timestamp provided by the exchange/estimated tx consensus time.
    std::chrono::high_resolution_clock::time_point _receiveTimestamp; // Timestamp book update was received by protocol.

    SidePair< SideBook > _sideBooks;
};

} // namespace Trading
} // namespace Synthfi

template <>
struct fmt::formatter< Synthfi::Trading::Book >
{
    constexpr auto parse( fmt::format_parse_context & ctx )
    {
        return ctx.begin( );
    }

    template < class FormatContext >
    auto format( const Synthfi::Trading::Book & book, FormatContext & ctx )
    {
        return fmt::format_to
        (
            ctx.out( ),
            "bids: {}, asks: {}, exchange timestamp: {}, received timestamp: {}",
            book.get( Synthfi::Trading::Side::bid ),
            book.get( Synthfi::Trading::Side::ask ),
            book.exchange_timestamp( ),
            book.receive_timestamp( )
        );
    }
};

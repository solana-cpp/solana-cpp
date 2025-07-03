#pragma once

#include <cstdint>
#include <limits>

#include "synthfi/Trading/Price.hpp"
#include "synthfi/Trading/Quantity.hpp"

#include "synthfi/Util/Logger.hpp"

#include <fmt/format.h>

namespace Synthfi
{
namespace Trading
{

struct PriceQuantity
{
    PriceQuantity( ) = default;

    PriceQuantity( Price price, Quantity quantity )
        : _price( price )
        , _quantity( quantity )
    { }

    PriceQuantity( int64_t price, int64_t quantity )
        : _price( price )
        , _quantity( quantity )
    { }

    PriceQuantity( uint64_t price, uint64_t quantity )
        : _price( price )
        , _quantity( quantity )
    { }

    auto operator<=>( const PriceQuantity & ) const = default;

    PriceQuantity & operator+=( const PriceQuantity & that )
    {
        _quantity += that.quantity( );
        return *this;
    }

    // Accessors to allow structured bindings.
    template< size_t N > requires( N == 0 || N == 1 )
    auto & get( ) &
    {
        return N == 0 ? _price : _quantity;
    }

    template< size_t N > requires( N == 0 || N == 1 )
    const auto & get( ) const &
    {
        return N == 0 ? _price : _quantity;
    }

    template< size_t N > requires( N == 0 || N == 1 )
    auto && get( ) &&
    {
        return N == 0 ? std::move( _price ) : std::move( _quantity );
    }

    const Price & price( ) const & { return _price; }
    const Quantity & quantity( ) const & { return _quantity; }

    Price & price( ) & { return _price; }
    Quantity & quantity( ) & { return _quantity; }

    Price && price( ) && { return std::move( _price ); }
    Quantity && quantity( ) && { return std::move( _quantity ); }

    bool is_valid( ) { return !( _price == synthfi_invalid_price( ) || _quantity == synthfi_invalid_quantity( ) ); }

private:
    Price _price = synthfi_invalid_price( );
    Quantity _quantity = synthfi_invalid_quantity( );
};

} // namespace Trading
} // namespace Synthfi

namespace std
{
    template< >
    struct tuple_size< Synthfi::Trading::PriceQuantity >
    {
        static constexpr size_t value = 2;
    };

    template < > struct tuple_element< 0, Synthfi::Trading::PriceQuantity > { using type = Synthfi::Trading::Price; };
    template < > struct tuple_element< 1, Synthfi::Trading::PriceQuantity > { using type = Synthfi::Trading::Quantity; };
}

template < >
struct fmt::formatter< Synthfi::Trading::PriceQuantity >
{
    constexpr auto parse( fmt::format_parse_context & ctx )
    {
        return ctx.begin( );
    }

    template < class FormatContext >
    auto format( const Synthfi::Trading::PriceQuantity & priceQuantity, FormatContext & ctx )
    {
        return fmt::format_to( ctx.out( ), "{}@{}", priceQuantity.quantity( ), priceQuantity.price( ) );
    }
};

#pragma once

#include <boost/assert.hpp>

#include <magic_enum/magic_enum.hpp>

namespace Synthfi
{
namespace Trading
{

enum class Side : uint8_t
{
    bid = 0,
    ask = 1,
    invalid = 2
};

inline constexpr Side side_flip( Side side )
{
    return side == Side::bid ? Side::ask : Side::bid;
}

template< class ItemType >
struct SidePair
{
    SidePair( ) = default;
    SidePair( ItemType bid, ItemType ask ) : _bid( bid ) , _ask ( ask ) { }

    // compile-time getters for structured bindings.
    template< uint8_t SideValue >
    constexpr auto & get( ) & requires ( SideValue == 0 || SideValue == 1 )
    {
        return magic_enum::enum_cast< Side >( SideValue ) == Side::bid ? _bid : _ask;
    }

    template< uint8_t SideValue >
    constexpr const auto & get( ) const & requires ( SideValue == 0 || SideValue == 1 )
    {
        return magic_enum::enum_cast< Side >( SideValue ) == Side::bid ? _bid : _ask;
    }

    template< uint8_t SideValue >
    constexpr const auto && get( ) && requires ( SideValue == 0 || SideValue == 1 )
    {
        return magic_enum::enum_cast< Side >( SideValue ) == Side::bid ? std::move( _bid ) : std::move( _ask );
    }

    // runtime gettings by enumerated enum.
    constexpr auto & get( Side side ) &
    {
        BOOST_ASSERT_MSG( side == Side::bid || side == Side::ask, "Invalid Side" );
        return side == Side::bid ? _bid : _ask;
    }

    constexpr const auto & get( Side side ) const &
    {
        BOOST_ASSERT_MSG( side == Side::bid || side == Side::ask, "Invalid Side" );
        return side == Side::bid ? _bid : _ask;
    }

    constexpr const auto && get( Side side ) &&
    {
        BOOST_ASSERT_MSG( side == Side::bid || side == Side::ask, "Invalid Side" );
        return side == Side::bid ? std::move( _bid ) : std::move( _ask );
    }

private:
    ItemType _bid;
    ItemType _ask;
};


} // namespace Trading
} // namespace Synthfi

namespace std
{
    template< class ItemType >
    struct tuple_size< Synthfi::Trading::SidePair< ItemType > >
    {
        static constexpr size_t value = 2;
    };

    template < class ItemType > struct tuple_element< 0, Synthfi::Trading::SidePair< ItemType > > { using type = ItemType; };
    template < class ItemType > struct tuple_element< 1, Synthfi::Trading::SidePair< ItemType > > { using type = ItemType; };
}

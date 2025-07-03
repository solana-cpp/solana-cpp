#pragma once

#include "synthfi/Util/Fixed.hpp"

#include <simdjson.h>

namespace Synthfi
{

// Customization point for deserializing solana accounts.
// See: boost::json::value_to
template< class T >
struct json_to_tag
{ };

template< class T >
T json_to( simdjson::ondemand::value value )
{
    static_assert( !std::is_reference_v< T > );

    return tag_invoke( json_to_tag< typename std::remove_cv_t< T > >( ), std::move( value ) );
}

inline FixedFloat tag_invoke( Synthfi::json_to_tag< FixedFloat >, simdjson::ondemand::value jsonValue )
{
    // The string_view will start at the beginning of the token, and include the entire toke
    // as well as all spaces until the next token (or EOF). This means, for example, that a
    // string token always begins with a " and is always terminated by the final ",
    // possibly followed by a number of spaces.
    // The string_view is not null-terminated. However, if this is a scalar
    //  (string, number, boolean, or null), the character after the end of the string_view is guaranteed to be a non-space token.
    size_t tokenSize = 0;
    std::string_view token = jsonValue.raw_json_token( );
    for ( auto character : token )
    {
        switch ( character )
        {
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
            case '.':
            case '-':
            case '+':
            case 'e':
            case 'E':
                ++tokenSize;
                continue;
            default:
                break;
        }
        break;
    }
    return FixedFloat( token.substr( 0, tokenSize ) );
}

} // namespace Synthfi

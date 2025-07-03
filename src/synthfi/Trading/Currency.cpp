#include "synthfi/Trading/Currency.hpp"

namespace Synthfi
{
namespace Trading
{

Currency tag_invoke( json_to_tag< Currency >, simdjson::ondemand::value jsonValue )
{
    Core::PublicKey mintAddress;
    mintAddress.init_from_base58( jsonValue[ "mintAddress" ].get_string( ).value( ) );

    return Currency
    {
        .currencyName = std::string( jsonValue[ "currencyName" ].get_string( ).value( ) ),
        .mintAddress = std::move( mintAddress )
    };
}

} // Synthfi
} // Trading

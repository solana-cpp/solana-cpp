#include "synthfi/Ftx/FtxTypes.hpp"

#include <magic_enum/magic_enum.hpp>

namespace Synthfi
{
namespace Ftx
{

FtxMarketInformation tag_invoke( json_to_tag< FtxMarketInformation >, simdjson::ondemand::value jsonValue )
{
    return FtxMarketInformation
    {
        .enabled = jsonValue[ "enabled" ].get_bool( ).value( ),
        .restricted = jsonValue[ "restricted" ].get_bool( ).value( ),
        .marketType = magic_enum::enum_cast< FtxMarketType >( jsonValue[ "type" ].get_string( ).value( ) ).value( ),
        .priceIncrement = json_to< Trading::Price >( jsonValue[ "priceIncrement" ].value( ) ),
        .quantityIncrement = json_to< Trading::Quantity >( jsonValue[ "priceIncrement" ].value( ) ),
        .baseCurrency = std::string( jsonValue[ "baseCurrency" ].get_string( ).value( ) ),
        .quoteCurrency = std::string( jsonValue[ "quoteCurrency" ].get_string( ).value( ) ),
        .name = std::string( jsonValue[ "name" ].get_string( ).value( ) )
    };
}

FtxAuthenticationConfig tag_invoke( json_to_tag< FtxAuthenticationConfig >, simdjson::ondemand::value jsonValue )
{
    return FtxAuthenticationConfig
    {
        .host = std::string( jsonValue[ "host" ].get_string( ).value( ) ),
        .apiKey = std::string( jsonValue[ "apiKey" ].get_string( ).value( ) ),
        .apiSecret = std::string( jsonValue[ "apiSecret" ].get_string( ).value( ) )
    };
}

} // namespace Ftx
} // namespace Synthfi

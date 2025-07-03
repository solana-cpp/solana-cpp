#include "synthfi/Trading/TradingPair.hpp"

#include <boost/json/object.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>

namespace Synthfi
{
namespace Trading
{

TradingPair tag_invoke( json_to_tag< TradingPair >, simdjson::ondemand::value jsonValue )
{
    Core::PublicKey serumMarketAddress;
    serumMarketAddress.init_from_base58( jsonValue[ "serumMarketAddress" ].get_string( ).value( ) );

    return TradingPair
    {
        .baseCurrency = std::string( jsonValue[ "baseCurrency" ].get_string( ).value( ) ),
        .quoteCurrency = std::string( jsonValue[ "quoteCurrency" ].get_string( ).value( ) ),
        .ftxMarketName = std::string( jsonValue[ "ftxMarketName" ].get_string( ).value( ) ),
        .serumMarketAddress = std::move( serumMarketAddress )
    };
}

} // Synthfi
} // Trading

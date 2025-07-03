#pragma once

#include "synthfi/Trading/Price.hpp"
#include "synthfi/Trading/Quantity.hpp"
#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Util/JsonUtils.hpp"
#include "synthfi/Util/Utils.hpp"

#include <string>

namespace Synthfi
{
namespace Trading
{

struct TradingPair
{
    std::string baseCurrency;
    std::string quoteCurrency;

    std::string ftxMarketName;
    Core::PublicKey serumMarketAddress;

    friend TradingPair tag_invoke( json_to_tag< TradingPair >, simdjson::ondemand::value jsonValue );
};

} // namespace Trading
} // namespace Synthfi

#pragma once

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Util/JsonUtils.hpp"
#include "synthfi/Util/Utils.hpp"

#include <simdjson.h>

namespace Synthfi
{
namespace Trading
{

enum struct CurrencyTypeFlags
{
    invalid = 0,
    token = 0x1,
    stable = 0x2
};

struct Currency
{
    friend Currency tag_invoke( json_to_tag< Currency >, simdjson::ondemand::value jsonValue );

    std::string currencyName;
    Core::PublicKey mintAddress;
};

} // namespace Trading
} // namespace Synthfi

#pragma once

#include "synthfi/Serum/SerumMarketDataClient/SerumMarketDataClientService.hpp"
#include "synthfi/Serum/SerumMarketDataClient/SerumMarketDataClientServiceProvider.hpp"

namespace Synthfi
{
namespace Serum
{
    using SerumMarketDataClient = SerumMarketDataClientServiceProvider< SerumMarketDataClientService >;
} // namespace Serum
} // namespace Synthfi

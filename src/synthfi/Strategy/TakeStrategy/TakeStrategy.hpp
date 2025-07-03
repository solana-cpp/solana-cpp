#pragma once

#include "synthfi/Strategy/TakeStrategy/TakeStrategyServiceProvider.hpp"
#include "synthfi/Strategy/TakeStrategy/TakeStrategyService.hpp"

namespace Synthfi
{
namespace Strategy
{
    using TakeStrategy = TakeStrategyServiceProvider< TakeStrategyService >;
} // namespace Strategy
} // namespace Synthfi

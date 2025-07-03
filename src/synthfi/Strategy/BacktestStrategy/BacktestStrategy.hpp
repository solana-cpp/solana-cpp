#pragma once

#include "synthfi/Strategy/BacktestStrategy/BacktestStrategyService.hpp"
#include "synthfi/Strategy/BacktestStrategy/BacktestStrategyServiceProvider.hpp"

namespace Synthfi
{
namespace Strategy
{
    using BacktestStrategy = BacktestStrategyServiceProvider< BacktestStrategyService >;
} // namespace Strategy
} // namespace Synthfi

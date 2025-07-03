#pragma once

#include "synthfi/Trading/Price.hpp"
#include "synthfi/Trading/Quantity.hpp"

#include <vector>

namespace Synthfi
{
namespace Trading
{

struct Wallet
{
    std::vector< Trading::Quantity > positions;
    std::vector< Trading::Quantity > marginAvailable; // Quote quantity available for each currency.
};

} // namespace Trading
} // namespace Synthfi

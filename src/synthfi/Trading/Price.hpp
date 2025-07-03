#pragma once

#include "synthfi/Util/Fixed.hpp"

namespace Synthfi
{
namespace Trading
{

using Price = FixedFloat;

inline Price synthfi_invalid_price( ) { return std::numeric_limits< Price >::max( ); }

} // namespace Trading
} // namespace Synthfi

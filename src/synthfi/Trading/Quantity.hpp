#pragma once

#include <synthfi/Util/Fixed.hpp>

namespace Synthfi
{
namespace Trading
{

using Quantity = FixedFloat;

inline Quantity synthfi_invalid_quantity( ) { return std::numeric_limits< Quantity >::max( ); }

} // namespace Trading
} // namespace Synthfi

#pragma once

#include <boost/multiprecision/cpp_dec_float.hpp>
#include <fmt/format.h>

namespace Synthfi
{

using FixedFloat = boost::multiprecision::number< boost::multiprecision::backends::cpp_dec_float< 32, int16_t > >;

} // namespace Synthfi

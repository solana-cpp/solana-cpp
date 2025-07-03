#pragma once

#include "synthfi/Mango/MangoOrderClient/MangoOrderClientService.hpp"
#include "synthfi/Mango/MangoOrderClient/MangoOrderClientServiceProvider.hpp"

namespace Synthfi
{
namespace Mango
{
    using MangoOrderClient = MangoOrderClientServiceProvider< MangoOrderClientService >;
} // namespace Mango
} // namespace Synthfi

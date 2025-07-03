#pragma once

#include "synthfi/Mango/MangoWalletClient/MangoWalletClientService.hpp"
#include "synthfi/Mango/MangoWalletClient/MangoWalletClientServiceProvider.hpp"

namespace Synthfi
{
namespace Mango
{
    using MangoWalletClient = MangoWalletClientServiceProvider< MangoWalletClientService >;
} // namespace Mango
} // namespace Synthfi

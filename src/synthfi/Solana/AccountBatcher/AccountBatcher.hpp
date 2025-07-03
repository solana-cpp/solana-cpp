#pragma once

#include "synthfi/Solana/AccountBatcher/AccountBatcherServiceProvider.hpp"
#include "synthfi/Solana/AccountBatcher/AccountBatcherService.hpp"

namespace Synthfi
{
namespace Solana
{
    using AccountBatcher = AccountBatcherServiceProvider< AccountBatcherService >;
} // namespace Solana
} // namespace Synthfi

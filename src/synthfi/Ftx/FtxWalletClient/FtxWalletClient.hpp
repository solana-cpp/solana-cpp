#pragma once

#include "synthfi/Ftx/FtxWalletClient/FtxWalletClientService.hpp"
#include "synthfi/Ftx/FtxWalletClient/FtxWalletClientServiceProvider.hpp"

namespace Synthfi
{
namespace Ftx
{
    using FtxWalletClient = FtxWalletClientServiceProvider< FtxWalletClientService >;
} // namespace Ftx
} // namespace Synthfi

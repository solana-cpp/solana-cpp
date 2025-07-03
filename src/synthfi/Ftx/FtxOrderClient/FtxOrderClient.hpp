#pragma once

#include "synthfi/Ftx/FtxOrderClient/FtxOrderClientService.hpp"
#include "synthfi/Ftx/FtxOrderClient/FtxOrderClientServiceProvider.hpp"

namespace Synthfi
{
namespace Ftx
{
    using FtxOrderClient = FtxOrderClientServiceProvider< FtxOrderClientService >;
} // namespace Ftx
} // namespace Synthfi

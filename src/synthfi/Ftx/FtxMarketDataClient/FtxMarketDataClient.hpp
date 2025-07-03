#pragma once

#include "synthfi/Ftx/FtxMarketDataClient/FtxMarketDataClientService.hpp"
#include "synthfi/Ftx/FtxMarketDataClient/FtxMarketDataClientServiceProvider.hpp"

namespace Synthfi
{
namespace Ftx
{
    using FtxMarketDataClient = FtxMarketDataClientServiceProvider< FtxMarketDataClientService >;
} // namespace Ftx
} // namespace Synthfi

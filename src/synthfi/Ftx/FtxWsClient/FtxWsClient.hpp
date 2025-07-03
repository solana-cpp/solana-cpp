#pragma once

#include "synthfi/Ftx/FtxWsClient/FtxWsClientService.hpp"
#include "synthfi/Ftx/FtxWsClient/FtxWsClientServiceProvider.hpp"

namespace Synthfi
{
namespace Ftx
{
    using FtxWsClient = FtxWsClientServiceProvider< FtxWsClientService >;
} // namespace Ftx
} // namespace Synthfi

#pragma once

#include "synthfi/Ftx/FtxRestClient/FtxRestClientService.hpp"
#include "synthfi/Ftx/FtxRestClient/FtxRestClientServiceProvider.hpp"

namespace Synthfi
{

namespace Ftx
{
    using FtxRestClient = FtxRestClientServiceProvider< FtxRestClientService >;
} // namespace Ftx

} // namespace Synthfi

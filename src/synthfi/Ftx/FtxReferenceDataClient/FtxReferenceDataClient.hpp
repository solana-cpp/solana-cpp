#pragma once

#include "synthfi/Ftx/FtxReferenceDataClient/FtxReferenceDataClientService.hpp"
#include "synthfi/Ftx/FtxReferenceDataClient/FtxReferenceDataClientServiceProvider.hpp"

namespace Synthfi
{
namespace Ftx
{
    using FtxReferenceDataClient = FtxReferenceDataClientServiceProvider< FtxReferenceDataClientService >;
} // namespace Ftx
} // namespace Synthfi

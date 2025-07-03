#pragma once

#include "synthfi/Mango/MangoReferenceDataClient/MangoReferenceDataClientService.hpp"
#include "synthfi/Mango/MangoReferenceDataClient/MangoReferenceDataClientServiceProvider.hpp"

namespace Synthfi
{
namespace Mango
{
    using MangoReferenceDataClient = MangoReferenceDataClientServiceProvider< MangoReferenceDataClientService >;
} // namespace Mango
} // namespace Synthfi

#pragma once

#include "synthfi/Serum/SerumReferenceDataClient/SerumReferenceDataClientService.hpp"
#include "synthfi/Serum/SerumReferenceDataClient/SerumReferenceDataClientServiceProvider.hpp"

namespace Synthfi
{
namespace Serum
{
    using SerumReferenceDataClient = SerumReferenceDataClientServiceProvider< SerumReferenceDataClientService >;
} // namespace Serum
} // namespace Synthfi

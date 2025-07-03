#pragma once

#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherService.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherServiceProvider.hpp"

namespace Synthfi
{
namespace Statistics
{
    using StatisticsPublisher = StatisticsPublisherServiceProvider< StatisticsPublisherService >;
} // namespace Statistics
} // namespace Synthfi

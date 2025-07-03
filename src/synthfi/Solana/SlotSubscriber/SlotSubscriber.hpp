#pragma once

#include "synthfi/Solana/SlotSubscriber/SlotSubscriberServiceProvider.hpp"
#include "synthfi/Solana/SlotSubscriber/SlotSubscriberService.hpp"

namespace Synthfi
{
namespace Solana
{
    using SlotSubscriber = SlotSubscriberServiceProvider< SlotSubscriberService >;
} // namespace Solana
} // namespace Synthfi

#pragma once

#include "synthfi/Solana/AccountSubscriber/AccountSubscriberServiceProvider.hpp"
#include "synthfi/Solana/AccountSubscriber/AccountSubscriberService.hpp"

namespace Synthfi
{
namespace Solana
{
    using AccountSubscriber = AccountSubscriberServiceProvider< AccountSubscriberService >;
} // namespace Solana
} // namespace Synthfi

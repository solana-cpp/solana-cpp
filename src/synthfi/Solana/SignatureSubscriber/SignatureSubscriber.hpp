#pragma once

#include "synthfi/Solana/SignatureSubscriber/SignatureSubscriberServiceProvider.hpp"
#include "synthfi/Solana/SignatureSubscriber/SignatureSubscriberService.hpp"

namespace Synthfi
{
namespace Solana
{
    using SignatureSubscriber = SignatureSubscriberServiceProvider< SignatureSubscriberService >;
} // namespace Solana
} // namespace Synthfi

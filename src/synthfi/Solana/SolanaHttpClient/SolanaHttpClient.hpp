#pragma once

#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClientServiceProvider.hpp"
#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClientService.hpp"

namespace Synthfi
{
namespace Solana
{
    using SolanaHttpClient = SolanaHttpClientServiceProvider< SolanaHttpClientService >;
} // namespace Solana
} // namespace Synthfi

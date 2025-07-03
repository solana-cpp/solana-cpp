#pragma once

#include "synthfi/Solana/SolanaWsClient/SolanaWsClientServiceProvider.hpp"
#include "synthfi/Solana/SolanaWsClient/SolanaWsClientService.hpp"

namespace Synthfi
{
namespace Solana
{
    using SolanaWsClient = SolanaWsClientServiceProvider< SolanaWsClientService >;
} // namespace Solana
} // namespace Synthfi

#pragma once

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Solana/SolanaTypes.hpp"

namespace Synthfi
{

namespace Solana
{

struct TokenMintAccount
{
    static constexpr size_t size( ) { return 82; }

    friend TokenMintAccount tag_invoke( Solana::account_to_tag< TokenMintAccount >, const AccountInfo & accountInfo );

    std::optional< Core::PublicKey > mintAuthority;
    uint64_t supply;
    uint8_t decimals;
    bool isInitialized;
    /// Optional authority to freeze token accounts.
    std::optional< Core::PublicKey > freezeAuthority;
};

} // namespace Solana

} // namespace Synthfi

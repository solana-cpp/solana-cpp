#pragma once

#include "synthfi/Solana/Transaction.hpp"

namespace
{

enum system_instruction : uint32_t
{
  e_create_account = 0,
  e_assign = 1,
  e_transfer = 2
};

} // namespace

namespace Synthfi
{
namespace Solana
{

template< uint16_t ProgramId, uint8_t ... AccountIndices >
struct CreateAccountInstruction
    : Instruction
    <
        ProgramId,
        AccountIndexArray< AccountIndices ... >,
        uint32_t, uint64_t, uint64_t, Core::PublicKey
    >
{
    using BaseType = Instruction
    <
        ProgramId,
        AccountIndexArray< AccountIndices ... >,
        uint32_t, uint64_t, uint64_t, Core::PublicKey
    >;

    CreateAccountInstruction
    (
        uint64_t lamports,
        uint64_t accountSpace,
        Core::PublicKeyRef newAccountPublicKey
    )
        : BaseType
        (
            system_instruction::e_create_account,
            lamports,
            accountSpace,
            *newAccountPublicKey
        )
    { }
};

static auto create_account_request
(
    Core::Hash recentBlockhash,
    Core::KeyPairRef publishKeyPair,
    Core::KeyPairRef accountKeyPair,
    Core::PublicKeyRef sysvarProgramPublicKey,
    Core::PublicKeyRef ownerPublicKey,
    uint64_t lamports,
    uint64_t space
)
{
    std::array< Core::KeyPairRef, 2 > signingWritableAccounts = { publishKeyPair, accountKeyPair };
    std::array< Core::KeyPairRef, 0 > signingReadOnlyAccounts = { };
    std::array< Core::PublicKeyRef, 0 > writableAccounts = { };
    std::array< Core::PublicKeyRef, 1 > readOnlyAccounts = { sysvarProgramPublicKey };

    CreateAccountInstruction< 2, 0, 1 > createAccountInstruction
    (
        lamports,
        space,
        ownerPublicKey
    );

    Transaction transaction
    (
            recentBlockhash,
            signingWritableAccounts,
            signingReadOnlyAccounts,
            writableAccounts,
            readOnlyAccounts,
            std::move( createAccountInstruction )
    );

    return SendTransactionRequest( std::move( transaction ) );
}

} // namespace Solana
} // namespace Synthfi

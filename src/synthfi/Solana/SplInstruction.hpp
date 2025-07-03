#pragma once

#include "synthfi/Solana/Transaction.hpp"

namespace
{

enum spl_instruction : uint32_t
{
  e_init_mint_account = 0,
  e_init_token_account = 1
};

} // namespace

namespace Synthfi
{
namespace Solana
{

template< uint16_t ProgramId, uint8_t ... AccountIndices >
struct InitMintInstruction
    : Instruction
    <
        ProgramId,
        AccountIndexArray< AccountIndices ... >,
        uint8_t, uint8_t, Core::PublicKey, uint8_t
    >
{
    using BaseType = Instruction
    <
        ProgramId,
        AccountIndexArray< AccountIndices ... >,
        uint8_t, uint8_t, Core::PublicKey, uint8_t
    >;

    InitMintInstruction( uint8_t mintDecimals, Core::PublicKeyRef mintAuthorityPublicKey )
        : BaseType
        (
            spl_instruction::e_init_mint_account,
            mintDecimals,
            *mintAuthorityPublicKey,
            0 // None optional
         )
    { }
};

template< uint16_t ProgramId, uint8_t ... AccountIndices >
struct InitTokenAccountInstruction
    : Instruction
    <
        ProgramId,
        AccountIndexArray< AccountIndices ... >,
        uint8_t
    >
{
    using BaseType = Instruction
    <
        ProgramId,
        AccountIndexArray< AccountIndices ... >,
        uint8_t
    >;

    InitTokenAccountInstruction( ) : BaseType ( spl_instruction::e_init_token_account ) { }
};

} // namespace Solana
} // namespace Synthfi

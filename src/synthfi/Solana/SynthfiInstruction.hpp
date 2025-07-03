#pragma once

#include "synthfi/Solana/Transaction.hpp"

namespace Synthfi
{
namespace Solana
{

template< uint16_t ProgramId, uint8_t ... AccountIndices >
struct InitSynthfiInstruction
    : Instruction
    <
        ProgramId,
        AccountIndexArray< AccountIndices ... >,
        uint32_t, uint32_t
    >
{
    using BaseType = Instruction
    <
        ProgramId,
        AccountIndexArray< AccountIndices ... >,
        uint32_t, uint32_t
    >;

    InitSynthfiInstruction( )
        : BaseType
        (
            SYNTHFI_VERSION,
            SYNTHFI_INIT
        )
        { }
};

template< uint16_t ProgramId, uint8_t ... AccountIndices >
struct InitSynthfiProductInstruction
    : Instruction
    <
        ProgramId,
        AccountIndexArray< AccountIndices ... >,
        uint32_t, uint32_t
    >
{
    using BaseType = Instruction
    <
        ProgramId,
        AccountIndexArray< AccountIndices ... >,
        uint32_t, uint32_t
    >;

    InitSynthfiProductInstruction( )
        : BaseType
        (
            SYNTHFI_VERSION,
            SYNTHFI_ADD_PRODUCT
        )
        { }
};

} // namespace Solana
} // namespace Synthfi

#pragma once

#include "synthfi/Mango/MangoTypes.hpp"
#include "synthfi/Solana/SolanaHttpMessage.hpp"
#include "synthfi/Solana/Transaction.hpp"
#include "synthfi/Trading/Order.hpp"

#include <magic_enum/magic_enum.hpp>

#include <array>
#include <bitset>
#include <variant>
#include <vector>

namespace Synthfi
{
namespace Mango
{

enum class MangoInstructionType : uint8_t
{
    InitMangoGroup = 0,
    InitMangoAccount = 1,
    Deposit = 2,
    Withdraw = 3,
    AddSpotMarket = 4,
    AddToBasket = 5,
    Borrow = 6,
    CachePrices = 7,
    CacheRootBanks = 8,
    PlaceSpotOrder = 9,
    AddOracle = 10,
    AddPerpMarket = 11,
    PlacePerpOrder = 12,
    CancelPerpOrderByClientId = 13,
    CancelPerpOrder = 14,
    ConsumeEvents = 15,
    CachePerpMarkets = 16,
    UpdateFunding = 17,
    SetOracle = 18,
    SettleFunds = 19,
    CancelSpotOrder = 20,
    UpdateRootBank = 21,
    SettlePnl = 22,
    SettleBorrow = 23,
    ForceCancelSpotOrders = 24,
    ForceCancelPerpOrders = 25,
    LiquidateTokenAndToken = 26,
    LiquidateTokenAndPerp = 27,
    LiquidatePerpMarket = 28,
    SettleFees = 29,
    ResolvePerpBankruptcy = 30,
    ResolveTokenBankruptcy = 31,
    InitSpotOpenOrders = 32,
    RedeemMngo = 33,
    AddMangoAccountInfo = 34,
    DepositMsrm = 35,
    WithdrawMsrm = 36,
    ChangePerpMarketParams = 37,
    SetGroupAdmin = 38,
    CancelAllPerpOrders = 39,
    // CacheRootBanks DEPRECATED 
    PlaceSpotOrder2 = 41,
    InitAdvancedOrders = 42,
    AddPerpTriggerOrder = 43,
    RemoveAdvancedOrder = 44,
    ExecutePerpTriggerOrder = 45,
    CreatePerpMarket = 46,
    ChangePerpMarketParams2 = 47,
    UpdateMarginBasket = 48,
    ChangeMaxMangoAccounts = 49,
    CloseMangoAccount = 50,
    CloseSpotOpenOrders = 51,
    CloseAdvancedOrders = 52,
    CreateDustAccount = 53,
    ResolveDust = 54,
    CreateMangoAccount = 55,
    UpgradeMangoAccountV0V1 = 56,
    CancelPerpOrdersSide = 57,
    SetDelegate = 58,
    ChangeSpotMarketParams = 59,
    CreateSpotOpenOrders = 60,
    ChangeReferralFeeParams = 61,
    SetReferrerMemory = 62,
    RegisterReferrerId = 63,
    PlacePerpOrder2 = 64,
    CancelAllSpotOrders = 65,
    Withdraw2 = 66,
    SetMarketMode = 67,
    RemovePerpMarket = 68,
    SwapSpotMarket = 69,
    RemoveSpotMarket = 70,
    RemoveOracle = 71,
    LiquidateDelistingToken = 72,
    ForceSettlePerpPosition = 73
};

/// Settle all funds from serum dex open orders
///
/// Accounts expected by this instruction (18):
///
/// 0. `[]` mango_group_ai - MangoGroup that this mango account is for
/// 1. `[]` mango_cache_ai - MangoCache for this MangoGroup
/// 2. `[signer]` owner_ai - MangoAccount owner
/// 3. `[writable]` mango_account_ai - MangoAccount
/// 4. `[]` dex_prog_ai - program id of serum dex
/// 5.  `[writable]` spot_market_ai - dex MarketState account
/// 6.  `[writable]` open_orders_ai - open orders for this market for this MangoAccount
/// 7. `[]` signer_ai - MangoGroup signer key
/// 8. `[writable]` dex_base_ai - base vault for dex MarketState
/// 9. `[writable]` dex_quote_ai - quote vault for dex MarketState
/// 10. `[]` base_root_bank_ai - MangoGroup base vault acc
/// 11. `[writable]` base_node_bank_ai - MangoGroup quote vault acc
/// 12. `[]` quote_root_bank_ai - MangoGroup quote vault acc
/// 13. `[writable]` quote_node_bank_ai - MangoGroup quote vault acc
/// 14. `[writable]` base_vault_ai - MangoGroup base vault acc
/// 15. `[writable]` quote_vault_ai - MangoGroup quote vault acc
/// 16. `[]` dex_signer_ai - dex Market signer account
/// 17. `[]` spl token program
template< uint16_t ProgramId, uint8_t ... AccountIndices >
struct SettleFundsInstruction
    : Solana::Instruction
    <
        ProgramId,
        Solana::AccountIndexArray< AccountIndices ... >,
        uint32_t // instruction type
    >
{
    using BaseType = Solana::Instruction
    <
        ProgramId,
        Solana::AccountIndexArray< AccountIndices ... >,
        uint32_t // instruction type
    >;

    SettleFundsInstruction( )
        : BaseType
        (
            magic_enum::enum_integer( MangoInstructionType::SettleFunds )
        )
        { }
};

/// Place an order on the Serum Dex using Mango account
///
/// Accounts expected by this instruction (23 + MAX_PAIRS):
/// 0. `[]` mango_group_ai - MangoGroup
/// 1. `[writable]` mango_account_ai - the MangoAccount of owner
/// 2. `[signer]` owner_ai - owner of MangoAccount
/// 3. `[]` mango_cache_ai - MangoCache for this MangoGroup
/// 4. `[]` dex_prog_ai - serum dex program id
/// 5. `[writable]` spot_market_ai - serum dex MarketState account
/// 6. `[writable]` bids_ai - bids account for serum dex market
/// 7. `[writable]` asks_ai - asks account for serum dex market
/// 8. `[writable]` dex_request_queue_ai - request queue for serum dex market
/// 9. `[writable]` dex_event_queue_ai - event queue for serum dex market
/// 10. `[writable]` dex_base_ai - base currency serum dex market vault
/// 11. `[writable]` dex_quote_ai - quote currency serum dex market vault
/// 12. `[]` base_root_bank_ai - root bank of base currency
/// 13. `[writable]` base_node_bank_ai - node bank of base currency
/// 14. `[writable]` base_vault_ai - vault of the basenode bank
/// 15. `[]` quote_root_bank_ai - root bank of quote currency
/// 16. `[writable]` quote_node_bank_ai - node bank of quote currency
/// 17. `[writable]` quote_vault_ai - vault of the quote node bank
/// 18. `[]` token_prog_ai - SPL token program id
/// 19. `[]` signer_ai - signer key for this MangoGroup
/// 20. `[]` rent_ai - rent sysvar var // TODO: I think this is not a field anymore.
/// 21. `[]` dex_signer_key - signer for serum dex
/// 22. `[]` msrm_or_srm_vault_ai - the msrm or srm vault in this MangoGroup. Can be zero key
/// 23+ `[writable]` open_orders_ais - An array of MAX_PAIRS. Only OpenOrders of current market
///         index needs to be writable. Only OpenOrders in_margin_basket needs to be correct;
///         remaining open orders can just be Pubkey::default() (the zero key)
struct PlaceSpotOrderInstruction
    : Solana::DynamicInstruction
    <
        uint32_t, // instruction type
        uint32_t, // side
        uint64_t, // limit_price
        uint64_t, // max_coin_qty
        uint64_t, // max_native_pc_qty_including_fees
        uint32_t, // self_trade_behavior
        uint32_t, // order_type
        uint64_t, // clientOrderId
        uint16_t // limit
    >
{
    using BaseType = Solana::DynamicInstruction
    <
        uint32_t,
        uint32_t,
        uint64_t,
        uint64_t,
        uint64_t,
        uint32_t,
        uint32_t,
        uint64_t,
        uint16_t
    >;

    PlaceSpotOrderInstruction
    (
        uint16_t programId,
        Solana::DynamicAccountIndexArray accountIndices,
        Trading::Side side,
        uint64_t price,
        uint64_t quantity,
        Trading::OrderType orderType,
        uint64_t maxQuoteQuantity,
        uint64_t clientOrderId
    );
};

using SendPlaceSpotOrderTransaction = Solana::DynamicTransaction< PlaceSpotOrderInstruction >;
using SendPlaceSpotOrderTransactionRequest  = Solana::SendTransactionRequest< SendPlaceSpotOrderTransaction >;

/// 0. `[writable]` market
/// 1. `[writable]` bids
/// 2. `[writable]` asks
/// 3. `[writable]` OpenOrders
/// 4. `[signer]` the OpenOrders owner
/// 5. `[writable]` event_q
template< uint16_t ProgramId, uint8_t ... AccountIndices >
struct CancelSpotOrderInstruction
    : Solana::Instruction
    <
        ProgramId,
        Solana::AccountIndexArray< AccountIndices ... >,
        uint32_t, // instruction type
        uint32_t, // side
        uint64_t, // sequence number ( upper 64 bits order id )
        uint64_t // limit price ( lower 64 bits order id )
    >
{
    using BaseType = Solana::Instruction
    <
        ProgramId,
        Solana::AccountIndexArray< AccountIndices ... >,
        uint32_t,
        uint32_t,
        uint64_t,
        uint64_t
    >;

    CancelSpotOrderInstruction
    (
        Trading::Side side,
        uint64_t sequenceNumber,
        uint64_t price
    ) : BaseType
    (
        magic_enum::enum_integer( MangoInstructionType::CancelSpotOrder ),
        magic_enum::enum_integer( side ),
        sequenceNumber,
        price
    )
    { }
};

} // namespace Mango
} // namespace Synthfi

#pragma once

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Solana/SolanaTypes.hpp"
#include "synthfi/Util/JsonUtils.hpp"

#include <boost/json/value_from.hpp>

#include <simdjson.h>

#include <vector>

// Include after system headers.
#include "synthesizer/synthesizer.h"

namespace Synthfi
{
namespace Solana
{

// getHealth
struct GetHealthRequest
{
    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetHealthRequest & request );

    static constexpr std::string_view method_name( ) { return "getHealth"; }
};

struct GetHealthResponse
{
    friend GetHealthResponse tag_invoke( json_to_tag< GetHealthResponse >, simdjson::ondemand::value jsonValue );

    std::string health;
};

// getSlot
struct GetSlotRequest
{
    static constexpr std::string_view method_name( ) { return "getSlot"; }

    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetSlotRequest & request );
};

struct GetSlotResponse
{
    uint64_t slot;

    friend GetSlotResponse tag_invoke( json_to_tag< GetSlotResponse >, simdjson::ondemand::value jsonValue );
};

// getRecentBlockhash
struct GetRecentBlockhashRequest
{
    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetRecentBlockhashRequest & request );

    static constexpr std::string_view method_name( ) { return "getRecentBlockhash"; }
};

struct GetRecentBlockhashResponse
{
    friend GetRecentBlockhashResponse tag_invoke( json_to_tag< GetRecentBlockhashResponse >, simdjson::ondemand::value jsonValue );

    Core::Hash blockhash;
};

// getLatestBlockhash
struct GetLatestBlockhashRequest
{
    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetLatestBlockhashRequest & request );

    static constexpr std::string_view method_name( ) { return "getLatestBlockhash"; }

    Solana::Commitment commitment;
};

struct GetLatestBlockhashResponse
{
    Core::Hash blockhash;
    uint64_t lastValidBlockHeight;

    friend GetLatestBlockhashResponse tag_invoke( json_to_tag< GetLatestBlockhashResponse >, simdjson::ondemand::value jsonValue );
};

// getBlockHeight
struct GetBlockHeightRequest
{
    static constexpr std::string_view method_name( ) { return "getBlockHeight"; }

    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetBlockHeightRequest & request );

    Solana::Commitment commitment;
};

struct GetBlockHeightResponse
{
    uint64_t blockHeight;

    friend GetBlockHeightResponse tag_invoke( json_to_tag< GetBlockHeightResponse >, simdjson::ondemand::value jsonValue );
};

// getMinimumBalanceForRentExempt
struct GetMinBalanceForRentExemptionRequest
{
    static constexpr std::string_view method_name( ) { return "getMinimumBalanceForRentExemption"; }

    uint64_t space;

    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetMinBalanceForRentExemptionRequest & request );
};

struct GetMinBalanceForRentExemptionResponse
{
    uint64_t lamports;

    friend GetMinBalanceForRentExemptionResponse tag_invoke
    (
        json_to_tag< GetMinBalanceForRentExemptionResponse >,
        simdjson::ondemand::value jsonValue
    );
};

// getAccountInfo
struct GetAccountInfoRequest
{
    static constexpr std::string_view method_name( ) { return "getAccountInfo"; }

    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetAccountInfoRequest & request );

    Core::PublicKey accountPublicKey; // Pubkey of account to query, as base-58 encoded string.
};

struct GetAccountInfoResponse
{
    friend GetAccountInfoResponse tag_invoke( json_to_tag< GetAccountInfoResponse >, simdjson::ondemand::value jsonValue );

    AccountInfo accountInfo;
};

// getMultipleAccounts
struct GetMultipleAccountsRequest
{
    static constexpr std::string_view method_name( ) { return "getMultipleAccounts"; }

    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetMultipleAccountsRequest & request );

    std::vector< Core::PublicKey > accountPublicKeys;
};

struct GetMultipleAccountsResponse
{
    friend GetMultipleAccountsResponse tag_invoke( json_to_tag< GetMultipleAccountsResponse >, simdjson::ondemand::value jsonValue );

    std::vector< AccountInfo > accountInfos;
};

// sendTransactionRequest
template< class TransactionType >
struct SendTransactionRequest
{
    static constexpr std::string_view method_name( ) { return "sendTransaction"; }

    friend void tag_invoke
    (
        boost::json::value_from_tag,
        boost::json::value & jsonValue,
        const SendTransactionRequest< TransactionType > & request
    )
    {
        jsonValue = boost::json::value_from( request.transaction );
    }

    TransactionType transaction;
};

struct SendTransactionResponse
{
    friend SendTransactionResponse tag_invoke( json_to_tag< SendTransactionResponse >, simdjson::ondemand::value jsonValue );

    Core::Signature transactionSignature;
};

} // namespace Solana
} // namespace Synthfi

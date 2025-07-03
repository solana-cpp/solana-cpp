#pragma once

#include "synthfi/Core/KeyPair.hpp"

#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>

#include <vector>

// Include after system headers.
#include "synthesizer/synthesizer.h"

namespace Synthfi
{

struct SynthfiProductInfo
{
    std::string name;
    std::string assetType;
    std::string description;

    std::string underlyingToken;

    uint8_t synthfiTokenExponent;
    uint8_t underlyingTokenExponent;

    uint64_t synthfiTokenBalance;
    uint64_t underlyingTokenDepositBalance;
    uint64_t collateralizationRatio;
};

struct UnderlyingTokenInfo
{
    std::string name;
    std::string assetType;
    std::string description;

    uint8_t underlyingTokenExponent;
    uint64_t underlyingTokenBalance;
};

struct SynthfiAccountInfo
{
    uint64_t lamports;
    std::vector< SynthfiProductInfo > synthfiTokenInfo;
    std::vector< UnderlyingTokenInfo > underlyingTokenInfo;
};

// SState of ynthfi accounts
struct SynthfiState
{
    synthfi_product_table_t productTable;
    std::vector< synthfi_product_t > products;
    std::vector< synthfi_deposit_account_t > depositAccounts;
};

// method: getSynrhfiInfo
// params: [ ]
struct GetSynthfiInfoRequest
{
    static std::string_view method_name( ) { return "getSynthfiInfo"; }
};
GetSynthfiInfoRequest tag_invoke( const boost::json::value_to_tag< GetSynthfiInfoRequest > &, const boost::json::value & jsonValue );

struct GetSynthfiInfoResponse
{
    SynthfiState synthfiState;
};
void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetSynthfiInfoResponse & response );

// method: getSynthfiAccountInfo
// params: [ publicKey ]
struct GetSynthfiAccountInfoRequest
{
    static std::string_view method_name( ) { return "getSynthfiAccountInfo"; }

    Core::PublicKey accountPublicKey;
};
GetSynthfiAccountInfoRequest tag_invoke( const boost::json::value_to_tag< GetSynthfiAccountInfoRequest > &, const boost::json::value & jsonValue );

struct GetSynthfiAccountInfoResponse
{
    uint64_t lamports;
};
void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetSynthfiAccountInfoResponse & response );

struct ErrorResponse
{
    std::string message;
};
void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const ErrorResponse & response );

} // namespace Synthfi

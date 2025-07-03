#include "synthfi/Solana/SolanaHttpMessage.hpp"
#include "synthfi/Solana/SplInstruction.hpp"
#include "synthfi/Solana/SynthfiInstruction.hpp"
#include "synthfi/Solana/SystemInstruction.hpp"

#include "synthfi/Util/StringEncode.hpp"
#include "synthfi/Util/Utils.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/string.hpp>
#include <boost/json/value.hpp>

#include <magic_enum/magic_enum.hpp>

#include <array>

namespace Synthfi
{
namespace Solana
{

// getHealth
void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetHealthRequest & request )
{
    jsonValue = boost::json::value{ };
}

GetHealthResponse tag_invoke( json_to_tag< GetHealthResponse >, simdjson::ondemand::value jsonValue )
{
    return GetHealthResponse
    {
        .health = std::string( jsonValue.get_string( ).value( ) )
    };
}

// getSlot
void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetSlotRequest & request )
{
    jsonValue = boost::json::value{ };
}

GetSlotResponse tag_invoke( json_to_tag< GetSlotResponse >, simdjson::ondemand::value jsonValue )
{
    return GetSlotResponse
    {
        .slot = jsonValue.get_uint64( ).value( )
    };
}

// getRecentBlockhash
void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetRecentBlockhashRequest & request )
{
    jsonValue = boost::json::value{ };
}

GetRecentBlockhashResponse tag_invoke( json_to_tag< GetRecentBlockhashResponse >, simdjson::ondemand::value jsonValue )
{
    GetRecentBlockhashResponse response;
    response.blockhash.init_from_base58
    (
        jsonValue[ "value" ][ "blockhash" ].get_string( ).value( )
    );
    return response;
}

// getLatestBlockhash
void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetLatestBlockhashRequest & request )
{
    jsonValue = boost::json::array
    {
        { { "commitment", magic_enum::enum_name( request.commitment ) } }
    };
}

GetLatestBlockhashResponse tag_invoke( json_to_tag< GetLatestBlockhashResponse >, simdjson::ondemand::value jsonValue )
{
    GetLatestBlockhashResponse latestBlockhashResponse;

    latestBlockhashResponse.blockhash.init_from_base58
    (
        jsonValue[ "blockhash" ].get_string( ).value( )
    );
    latestBlockhashResponse.lastValidBlockHeight = jsonValue[ "lastValidBlockHeight" ].get_uint64( );

    return latestBlockhashResponse;
}

// getBlockHeight
void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetBlockHeightRequest & request )
{
    jsonValue = boost::json::array
    {
        { { "commitment", magic_enum::enum_name( request.commitment ) } }
    };
}

GetBlockHeightResponse tag_invoke( json_to_tag< GetBlockHeightResponse >, simdjson::ondemand::value jsonValue )
{
    return GetBlockHeightResponse
    {
        .blockHeight = jsonValue.get_uint64( )
    };
}

// getMinBalanceForRentExemption
void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetMinBalanceForRentExemptionRequest & request )
{
    jsonValue = boost::json::array{ request.space };
}

GetMinBalanceForRentExemptionResponse tag_invoke
(
    json_to_tag< GetMinBalanceForRentExemptionResponse >,
    simdjson::ondemand::value jsonValue
)
{
    return GetMinBalanceForRentExemptionResponse
    {
        .lamports = jsonValue.get_uint64( ).value( )
    };
}


// getAccountInfo
void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetAccountInfoRequest & request )
{
    jsonValue = boost::json::array
    {
            request.accountPublicKey.enc_base58_text( ), // base-58 encoded account.
            { { "encoding", "base64" } }
    };
}

GetAccountInfoResponse tag_invoke( json_to_tag< GetAccountInfoResponse >, simdjson::ondemand::value jsonValue )
{
    return GetAccountInfoResponse
    {
        .accountInfo = json_to< AccountInfo >( jsonValue[ "value" ].value( ) )
    };
}

// getMultipleAccounts
void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetMultipleAccountsRequest & request )
{
    boost::json::array publicKeyArray( request.accountPublicKeys.size( ) );
    std::transform
    (
        request.accountPublicKeys.begin( ),
        request.accountPublicKeys.end( ),
        publicKeyArray.begin( ),
        [ ] ( const Core::PublicKey & publicKey ) -> boost::json::string
        {
            return { publicKey.enc_base58_text( ) }; // base-58 encoded account.
        }
    );

    jsonValue = boost::json::array
    {
        std::move( publicKeyArray),
        { { "encoding", "base64" } }
    };
}

GetMultipleAccountsResponse tag_invoke( json_to_tag< GetMultipleAccountsResponse >, simdjson::ondemand::value jsonValue )
{
    std::vector< AccountInfo > accounts;
    for ( simdjson::ondemand::value account : jsonValue[ "value" ].get_array( ) )
    {
        accounts.push_back( json_to< AccountInfo >( account ) );
    }

    return GetMultipleAccountsResponse
    {
        .accountInfos = std::move( accounts )
    };
}

SendTransactionResponse tag_invoke( json_to_tag< SendTransactionResponse >, simdjson::ondemand::value jsonValue )
{
    Core::Signature transactionSignature;
    if ( !transactionSignature.init_from_base58( jsonValue.get_string( ).value( ) ) )
    {
        throw SynthfiError( "base58 deserialization error" );
    }

    return SendTransactionResponse
    {
        .transactionSignature = std::move( transactionSignature )
    };
}

} // namespace Solana
} // namespace Synthfi

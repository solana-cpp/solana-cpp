#include "synthfi/Util/Logger.hpp"
#include "synthfi/Solana/SolanaWsMessage.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/string.hpp>
#include <boost/json/value.hpp>
#include <boost/json/serialize.hpp>

#include <magic_enum/magic_enum.hpp>

namespace Synthfi
{
namespace Solana
{

SubscribeResult tag_invoke( json_to_tag< SubscribeResult >, simdjson::ondemand::value jsonValue )
{
    return SubscribeResult
    {
        .subscriptionId = jsonValue.get_uint64( )
    };
}

UnsubscribeResult tag_invoke( json_to_tag< UnsubscribeResult >, simdjson::ondemand::value jsonValue )
{
    return UnsubscribeResult
    {
        .success = jsonValue.get_bool( )
    };
}

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const SignatureSubscribe & request )
{
    jsonValue = boost::json::array
    {
        request.transactionSignature.enc_base58_text( ),
        { { "commitment", magic_enum::enum_name( request.commitment ) } }
    };
}

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const SignatureUnsubscribe & request )
{
    jsonValue = boost::json::array{ request.subscriptionId };
}

SignatureNotification tag_invoke( json_to_tag< SignatureNotification >, simdjson::ondemand::value jsonValue )
{
    auto error = jsonValue[ "value"][ "err" ];
    if ( error.type( ).value( ) == simdjson::ondemand::json_type::null )
    {
        return { };
    }

    return SignatureNotification
    {
        .error = std::optional( std::string( error.get_string( ).value( ) ) )
    };
}

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const AccountSubscribe & request )
{
    jsonValue = boost::json::array
    {
        request.accountPublicKey.enc_base58_text( ),
        {
            { "commitment", magic_enum::enum_name( request.commitment ) },
            { "encoding", "base64" }
        }
    };
}

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const AccountUnsubscribe & request )
{
    jsonValue = boost::json::array{ request.subscriptionId };
}

AccountNotification tag_invoke( json_to_tag< AccountNotification >, simdjson::ondemand::value jsonValue )
{
    return AccountNotification
    {
        .accountInfo = json_to< AccountInfo >( jsonValue[ "value" ].value( ) )
    };
}

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const SlotSubscribe & request )
{
    jsonValue = boost::json::value{ }; // no params.
}

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const SlotUnsubscribe & request )
{
    jsonValue = boost::json::array{ request.subscriptionId };
}

SlotNotification tag_invoke( json_to_tag< SlotNotification >, simdjson::ondemand::value jsonValue )
{
    return SlotNotification
    {
        .parent = jsonValue[ "parent" ].get_uint64( ),
        .root = jsonValue[ "root" ].get_uint64( ),
        .slot = jsonValue[ "slot" ].get_uint64( )
    };
}

} // namespace Solana
} // namespace Synthfi

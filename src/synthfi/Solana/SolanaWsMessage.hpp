#pragma once

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Solana/SolanaTypes.hpp"
#include "synthfi/Util/JsonUtils.hpp"

#include <boost/json/value_from.hpp>

#include <optional>

namespace Synthfi
{
namespace Solana
{

struct SubscribeResult
{
    uint64_t subscription_id( ) const { return subscriptionId; }
    friend SubscribeResult tag_invoke( json_to_tag< SubscribeResult >, simdjson::ondemand::value jsonValue );

    uint64_t subscriptionId; // [integer] Needed to unsubscribe.
};

struct UnsubscribeResult
{
    friend UnsubscribeResult tag_invoke( json_to_tag< UnsubscribeResult >, simdjson::ondemand::value jsonValue );

    bool success;
};

// signatureSubscribe

struct SignatureSubscribe
{
    static constexpr std::string_view method_name( ) { return "signatureSubscribe"; }
    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const SignatureSubscribe & request );

    Core::Signature transactionSignature;
    Commitment commitment;
};

struct SignatureUnsubscribe
{
    static constexpr std::string_view method_name( ) { return "signatureUnsubscribe"; }
    uint64_t subscription_id( ) const { return subscriptionId; }
    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const SignatureUnsubscribe & request );

    uint64_t subscriptionId;
};

struct SignatureNotification
{
    static constexpr std::string_view method_name( ) { return "signatureNotification"; }
    friend SignatureNotification tag_invoke( json_to_tag< SignatureNotification >, simdjson::ondemand::value jsonValue );

    std::optional< std::string > error; // Error if transaction failed, does not contain a value if transaction succeeded.
};

// accountSubscribe

struct AccountSubscribe
{
    static constexpr std::string_view method_name( ) { return "accountSubscribe"; }
    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const AccountSubscribe & request );

    Core::PublicKey accountPublicKey; // Transaction Core::Signature, as base-58 encoded string.
    Commitment commitment;
};

struct AccountUnsubscribe
{
    static constexpr std::string_view method_name( ) { return "accountUnsubscribe"; }
    uint64_t subscription_id( ) const { return subscriptionId; }
    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const AccountUnsubscribe & request );

    uint64_t subscriptionId;
};

struct AccountNotification
{
    static constexpr std::string_view method_name( ) { return "accountNotification"; }
    friend AccountNotification tag_invoke( json_to_tag< AccountNotification >, simdjson::ondemand::value jsonValue );

    AccountInfo accountInfo;
};

// slotSubscribe

struct SlotSubscribe
{
    static constexpr std::string_view method_name( ) { return "slotSubscribe"; }
    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const SlotSubscribe & request );
};

struct SlotUnsubscribe
{
    static constexpr std::string_view method_name( ) { return "slotUnsubscribe"; }
    uint64_t subscription_id( ) const { return subscriptionId; }
    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const SlotUnsubscribe & request );

    uint64_t subscriptionId;
};

struct SlotNotification
{
    static constexpr std::string_view method_name( ) { return "slotNotification"; }
    friend SlotNotification tag_invoke( json_to_tag< SlotNotification >, simdjson::ondemand::value jsonValue );

    uint64_t parent;
    uint64_t root;
    uint64_t slot;
};

} // namespace Solana
} // namespace Synthfi

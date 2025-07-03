#pragma once

#include "synthfi/Core/KeyPair.hpp"

#include "synthfi/Util/JsonUtils.hpp"

namespace Synthfi
{
namespace Solana
{

enum class Commitment : uint8_t
{
    invalid = 0,
    finalized = 1,
    confirmed = 2,
    processed = 3
};

struct AccountInfo
{
    friend AccountInfo tag_invoke( json_to_tag< AccountInfo >, simdjson::ondemand::value jsonValue );

    bool executable;
    uint64_t lamports;
    Core::PublicKey owner;
    std::vector< std::byte > data;
};

// Customization point for deserializing solana accounts.
// See: boost::json::value_to
template< class T >
struct account_to_tag
{ };

template< class T >
T account_to( AccountInfo accountInfo )
{
    static_assert( !std::is_reference_v< T > );

    return tag_invoke( account_to_tag< typename std::remove_cv_t< T > >( ), std::move( accountInfo ) );
}

struct SolanaEndpointConfig
{
    friend SolanaEndpointConfig tag_invoke( json_to_tag< SolanaEndpointConfig >, simdjson::ondemand::value jsonValue );

    std::string host;
    std::string service;
    std::string target;
};

} // namespace Solana
} // namespace Synthfi

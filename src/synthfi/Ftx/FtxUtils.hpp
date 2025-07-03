#pragma once

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Trading/Price.hpp"
#include "synthfi/Trading/Quantity.hpp"
#include "synthfi/Trading/Book.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/json/value_from.hpp>

#include <chrono>

namespace Synthfi
{
namespace Ftx
{

struct FtxAuthenticationMessage
{
    std::string apiKey;
    std::string apiSecret;
    std::chrono::high_resolution_clock::time_point requestTime;

    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const FtxAuthenticationMessage & message );
};

template< bool isPostRequest, class RequestType >
void authenticate_request
(
    RequestType & request,
    std::string_view apiKey,
    std::string_view apiSecret,
    std::chrono::high_resolution_clock::time_point requestTime
)
{
    // FTX-SIGN: SHA256 HMAC of the following three strings, using your API secret, as a hex string:
    // Request timestamp (e.g. 1528394229375)
    // HTTP method in uppercase (e.g. GET or POST)
    // Request path, including leading slash and any URL parameters but not including the hostname (e.g. /account)
    // (POST only) Request body (JSON-encoded)

    request.set( "FTXUS-KEY", apiKey.data( ) ); // API key.
    auto timeString = std::to_string( std::chrono::duration_cast< std::chrono::milliseconds >( requestTime.time_since_epoch( ) ).count( ) );
    request.set( "FTXUS-TS", timeString ); // Number of milliseconds since Unix epoch.

    auto authMessage = timeString
        .append( std::string( request.method_string( ) ) )
        .append( std::string( request.target( ) ) );

    if constexpr ( isPostRequest )
    {
        authMessage.append( request.body( ) );
    }

   Core::Signature signature;
    signature.sign_hmac( authMessage, apiSecret );

    // FTX API requires first 32 bytes of hmac signature.
    request.set( "FTXUS-SIGN", signature.enc_hex< 32 >( ) );
}

uint32_t ftx_orderbook_checksum( const Trading::Book & book );

} // namespace Ftx
} // namespace Synthfi

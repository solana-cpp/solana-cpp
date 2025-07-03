#pragma once

#include "synthfi/Trading/TradingPair.hpp"

#include <boost/describe/enum.hpp>

#include "synthfi/Util/JsonUtils.hpp"

#include <vector>

namespace Synthfi
{
namespace Ftx
{

enum class FtxMarketType : uint8_t
{
    invalid = 0,
    spot = 1,
    future = 2
};

enum class FtxChannelType : uint8_t
{
    invalid = 0,
    orderbook = 1,
    trades = 2,
    orders = 3,
    fills = 4,
    ticker = 5
};

enum class FtxOrderType : uint8_t
{
    invalid = 0,
    limit = 1,
    market = 2
};

enum class FtxLiquidityType :uint8_t
{
    invalid = 0,
    maker = 1,
    taker = 2
};

enum class FtxOp : uint8_t
{
    invalid = 0,
    subscribe = 1,
    unsubscribe = 2,
    login = 3
};

enum class FtxResponseType : uint8_t
{
    invalid = 0,
    error = 1,
    subscribed = 2,
    unsubscribed = 3,
    info = 4,
    partial = 5,
    update = 6
};

enum class FtxWithdrawlStatus : uint8_t
{
    invalid = 0,
    requested = 1,
    processing = 2,
    complete = 3,
    cancelled = 4
};

enum class FtxOrderStatus : uint8_t
{
    INVALID = 0,
    NEW = 1, // keyword, sigh.
    OPEN = 2,
    CLOSED = 3
};

inline Trading::Quantity ftx_taker_fee( ) { return Trading::Quantity{ 0.002 }; } // 20 bps

inline FtxOrderStatus ftx_order_status_from_string( std::string_view orderStatusString )
{
    if ( orderStatusString == "new" )
    {
        return FtxOrderStatus::NEW;
    }
    if ( orderStatusString == "open" )
    {
        return FtxOrderStatus::OPEN;
    }
    if ( orderStatusString == "closed" )
    {
        return FtxOrderStatus::CLOSED;
    }

    BOOST_ASSERT_MSG( false, "Invalid order status string" );
    return FtxOrderStatus::INVALID;
}

struct FtxMarketInformation
{
    bool enabled; // If the market is enabled.
    bool restricted; // If the market has nonstandard restrictions on which jurisdictions can trade it.

    FtxMarketType marketType; // 'future' or 'spot'.

    Trading::Price priceIncrement; // Price tick size.
    Trading::Quantity quantityIncrement; // Min size step.

    std::string baseCurrency; // Base currency if spot, else null.
    std::string quoteCurrency; // Quote currency if spot, else null.

    std::string name; // Name of the market.

    friend FtxMarketInformation tag_invoke( json_to_tag< FtxMarketInformation >, simdjson::ondemand::value jsonValue );
};

struct FtxTradingPair
{
    FtxMarketInformation marketInformation;

    uint64_t baseCurrencyIndex;
    uint64_t quoteCurrencyIndex;
};

struct FtxCurrency
{
    std::string currencyName;
};

struct FtxReferenceData
{
    std::vector< FtxTradingPair > tradingPairs;
    std::vector< FtxCurrency > currencies;
};

struct FtxAuthenticationConfig
{
    std::string host;
    std::string apiKey;
    std::string apiSecret;
 
    friend FtxAuthenticationConfig tag_invoke( json_to_tag< FtxAuthenticationConfig >, simdjson::ondemand::value jsonValue );
};

} // namespace Ftx
} // namespace Synthfi

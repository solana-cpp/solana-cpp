#pragma once

#include "synthfi/Ftx/FtxTypes.hpp"
#include "synthfi/Trading/Side.hpp"
#include "synthfi/Trading/PriceQuantity.hpp"
#include "synthfi/Util/JsonUtils.hpp"

#include <boost/beast/http/status.hpp>
#include <boost/json/value_from.hpp>

#include <optional>
#include <string>
#include <vector>

namespace Synthfi
{
namespace Ftx
{


struct FtxSubscribeFillsMessage
{
    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const FtxSubscribeFillsMessage & message );
};

struct FtxSubscribeOrdersMessage
{
    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const FtxSubscribeOrdersMessage & message );
};

struct SubscriptionRequest
{
    FtxChannelType channel;
    std::string market;

    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const SubscriptionRequest & request );
};

// Every message contains a signed 32-bit integer checksum of the orderbook.
// You can run the same checksum on your client orderbook state
// and compare it to checksum field. If they are the same, your client's state is correct.
// If not, you have likely lost or mishandled a packet and should re-subscribe to receive the initial snapshot.
struct FtxOrderbookMessage
{
    static constexpr std::string_view channel( ) { return "orderbook"; }

    friend FtxOrderbookMessage tag_invoke( json_to_tag< FtxOrderbookMessage >, simdjson::ondemand::value jsonValue );

    uint32_t checksum;
    std::chrono::high_resolution_clock::time_point time;
    Trading::SidePair< std::vector< Trading::PriceQuantity > > levels;
};

struct FtxFillMessage
{
    friend FtxFillMessage tag_invoke( json_to_tag< FtxFillMessage >, simdjson::ondemand::value jsonValue );

    Trading::Side side;
    Trading::Price price;
    Trading::Quantity quantity;

    Trading::Price fee;
    double feeRate;
    FtxLiquidityType liquidityType;

    uint64_t fillId;
    uint64_t orderId;
    uint64_t tradeId;

    std::chrono::high_resolution_clock::time_point time;
    std::string market;
};

struct FtxOrderMessage
{
    friend FtxOrderMessage tag_invoke( json_to_tag< FtxOrderMessage >, simdjson::ondemand::value jsonValue );

    Trading::Side side;
    Trading::Price price;
    Trading::Quantity quantity;

    Trading::Quantity fillQuantity;
    Trading::Quantity remainingQuantity;
    Trading::Price averageFillPrice;

    FtxOrderType orderType;
    FtxOrderStatus orderStatus;

    bool reduceOnly;
    bool ioc;
    bool postOnly;

    uint64_t orderId;
    uint64_t clientOrderId;

    std::chrono::high_resolution_clock::time_point time;
    std::string market;
};

} // namespace Ftx
} // namespace Synthfi

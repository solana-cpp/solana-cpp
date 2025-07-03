#pragma once

#include "synthfi/Ftx/FtxTypes.hpp"

#include "synthfi/Trading/Price.hpp"
#include "synthfi/Trading/Quantity.hpp"
#include "synthfi/Trading/Side.hpp"
#include "synthfi/Util/JsonUtils.hpp"

#include <boost/beast/http/verb.hpp>

#include <boost/json/value_from.hpp>

#include <chrono>
#include <string>
#include <vector>

namespace Synthfi
{
namespace Ftx
{

struct GetBalancesRequest
{
    static constexpr boost::beast::http::verb request_verb( ) { return boost::beast::http::verb::get; }
    constexpr std::string target( ) const & { return std::string( "/api/wallet/balances" ); }
};

struct GetBalancesResponse
{
    struct Balance
    {
        std::string coin; // Coin id.
        Trading::Quantity total; // Total amount.
        Trading::Quantity free; // Free amount.
        Trading::Quantity spotBorrow; // Amount borrowed using spot margin.
        Trading::Quantity availableWithoutBorrow; // Amount available without borrowing.
        Trading::Quantity usdValue; // Approximate total amount in USD.
    };

    friend GetBalancesResponse tag_invoke( json_to_tag< GetBalancesResponse >, simdjson::ondemand::value jsonValue );

    std::vector< Balance > balances;
};

struct GetDepositAddressRequest
{
    static constexpr boost::beast::http::verb request_verb( ) { return boost::beast::http::verb::get; }

    std::string target( ) const & { return fmt::format( "/api/wallet/deposit_address/{}", coin ); }

    std::string coin;
};

struct GetDepositAddressResponse
{
    friend GetDepositAddressResponse tag_invoke( json_to_tag< GetDepositAddressResponse >, simdjson::ondemand::value jsonValue );

    std::string address;
};

struct PostWithdrawlRequest
{
    static constexpr boost::beast::http::verb request_verb( ) { return boost::beast::http::verb::post; }

    constexpr std::string target( ) const & { return "/api/wallet/withdrawls"; }

    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const PostWithdrawlRequest & request );

    std::string coin; // Coin to withdraw.
    Trading::Quantity size; // Amount to withdraw.
    std::string address; // Address to send to.
};

struct PostWithdrawlResponse
{
    std::string coin; // Coin id.
    std::string address; // Deposit address the withdrawal was sent to.
    Trading::Quantity fee; // Fee, not included in size.
    uint64_t id; // Withdrawal id.
    Trading::Quantity size;
    FtxWithdrawlStatus status; // One of "requested", "processing", "complete", or "cancelled".
    std::chrono::high_resolution_clock::time_point time;

    friend PostWithdrawlResponse tag_invoke( json_to_tag< PostWithdrawlResponse >, simdjson::ondemand::value jsonValue );
};


struct GetMarketsRequest
{
    static constexpr boost::beast::http::verb request_verb( ) { return boost::beast::http::verb::get; }

    std::string target( ) const { return "/api/markets"; }
};

struct GetMarketsResponse
{
    friend GetMarketsResponse tag_invoke( json_to_tag< GetMarketsResponse >, simdjson::ondemand::value jsonValue );

    std::vector< FtxMarketInformation > markets;
};

struct FtxOrderRequest
{
    static constexpr boost::beast::http::verb request_verb( ) { return boost::beast::http::verb::post; }

    constexpr std::string target( ) const & { return "/api/orders"; }

    friend void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const FtxOrderRequest & request );

    bool reduceOnly;
    bool ioc;
    bool postOnly;

    FtxOrderType orderType;

    Trading::Side side;
    Trading::Price price;
    Trading::Quantity quantity;

    std::string marketName;
    uint64_t clientOrderId; // json encoded as string.
};

struct FtxOrderResponse
{
    friend FtxOrderResponse tag_invoke( json_to_tag< FtxOrderResponse >, simdjson::ondemand::value jsonValue );

    bool reduceOnly;
    bool ioc;
    bool postOnly;

    FtxOrderStatus orderStatus = FtxOrderStatus::INVALID;
    FtxOrderType orderType = FtxOrderType::invalid;
    Trading::Side side = Trading::Side::invalid;

    uint64_t orderId;

    Trading::Price price;

    Trading::Quantity orderSize;
    Trading::Quantity fillQuantity;
    Trading::Quantity remainingQuantity;

    std::chrono::high_resolution_clock::time_point createdTime;

    std::string marketName;
    uint64_t clientOrderId;
};

} // namespace Ftx
} // namespace Synthfi

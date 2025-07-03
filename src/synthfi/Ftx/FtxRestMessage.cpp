#include "synthfi/Ftx/FtxRestMessage.hpp"

#include <magic_enum/magic_enum.hpp>

namespace Synthfi
{
namespace Ftx
{

GetBalancesResponse tag_invoke( json_to_tag< GetBalancesResponse >, simdjson::ondemand::value jsonValue )
{
    GetBalancesResponse balancesResponse;

    for ( simdjson::ondemand::object balanceObject : jsonValue.get_array( ) )
    {
        GetBalancesResponse::Balance balance
        {
            .coin = std::string( balanceObject[ "coin" ].get_string( ).value( ) ),
            .total = json_to< Trading::Quantity >( balanceObject[ "total" ].value( ) ),
            .free = json_to< Trading::Quantity >( balanceObject[ "free" ].value( ) ),
            .spotBorrow = json_to< Trading::Quantity >( balanceObject[ "spotBorrow" ].value( ) ),
            .availableWithoutBorrow = json_to< Trading::Quantity >( balanceObject[ "availableWithoutBorrow" ].value( ) ),
            .usdValue = json_to< Trading::Quantity >( balanceObject[ "usdValue" ].value( ) )
        };
        balancesResponse.balances.push_back( std::move( balance ) );
    }

    return balancesResponse;
}

GetDepositAddressResponse tag_invoke( json_to_tag< GetDepositAddressResponse >, simdjson::ondemand::value jsonValue )
{
    return GetDepositAddressResponse
    {
        .address = std::string( jsonValue[ "address" ].get_string( ).value( ) )
    };
}

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const PostWithdrawlRequest & request )
{
    jsonValue = boost::json::object
    {
        { "coin", request.coin },
        { "size", request.size.convert_to< double >( ) },
        { "address", request.address }
    };
}

PostWithdrawlResponse tag_invoke( json_to_tag< PostWithdrawlResponse >, simdjson::ondemand::value jsonValue )
{
    return PostWithdrawlResponse
    {
        .coin = std::string( jsonValue[ "coin" ].get_string( ).value( ) ),
        .address = std::string( jsonValue[ "address" ].get_string( ).value( ) ),
        .fee = json_to< Trading::Quantity >( jsonValue[ "fee" ].value( ) ),
        .id = jsonValue[ "id" ].get_uint64( ).value( ),
        .size = json_to< Trading::Quantity >( jsonValue[ "size" ].value( ) ),
        .status = magic_enum::enum_cast< FtxWithdrawlStatus >( jsonValue[ "status" ].get_string( ).value( ) ).value( ),
        .time = std::chrono::high_resolution_clock::time_point
        (
            std::chrono::duration_cast< std::chrono::high_resolution_clock::duration >
            (
                std::chrono::duration< double >( jsonValue[ "time" ].get_double( ).value( ) )
            )
        )
    };
}

GetMarketsResponse tag_invoke( json_to_tag< GetMarketsResponse >, simdjson::ondemand::value jsonValue )
{
    GetMarketsResponse marketsResponse;
    for ( simdjson::ondemand::value marketValue : jsonValue.get_array( ) )
    {
        marketsResponse.markets.push_back( json_to< FtxMarketInformation >( marketValue ) );
    }

    return marketsResponse;
}

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const FtxOrderRequest & request )
{
    boost::json::object jsonRequest
    {
        { "type", magic_enum::enum_name( request.orderType ) },
        { "market", request.marketName },
        { "size", static_cast< double >( request.quantity ) },
        { "price", static_cast< double >( request.price ) },
        { "clientId", std::to_string( request.clientOrderId ) },
        { "side", ( request.side == Trading::Side::bid ) ? "buy" : "sell" }
    };
    if ( request.reduceOnly )
    {
        jsonRequest[ "reduceOnly" ] =  true;
    }
    if ( request.ioc )
    {
        jsonRequest[ "ioc" ] =  true;
    }
    if ( request.postOnly )
    {
        jsonRequest[ "postOnly" ] =  true;
    }

    jsonValue = jsonRequest;
}

FtxOrderResponse tag_invoke( json_to_tag< FtxOrderResponse >, simdjson::ondemand::value jsonValue )
{
    return FtxOrderResponse
    {
        .reduceOnly = jsonValue[ "reduceOnly" ].get_bool( ).value( ),
        .ioc = jsonValue[ "ioc" ].get_bool( ).value( ),
        .postOnly = jsonValue[ "postOnly" ].get_bool( ).value( ),
        .orderStatus = ftx_order_status_from_string( jsonValue[ "status" ].get_string( ).value( ) ),
        .orderType = magic_enum::enum_cast< FtxOrderType >( jsonValue[ "type" ].get_string( ).value( ) ).value( ),
        .side = ( jsonValue[ "side" ].get_string( ).value( ) == "buy" ) ? Trading::Side::bid : Trading::Side::ask,
        .orderId = jsonValue[ "id" ].get_uint64( ).value( ),
        .price = json_to< Trading::Price >( jsonValue[ "price" ].value( ) ),
        .orderSize = json_to< Trading::Quantity >( jsonValue[ "size" ].value( ) ),
        .fillQuantity = json_to< Trading::Quantity >( jsonValue[ "filledSize" ].value( ) ),
        .remainingQuantity = json_to< Trading::Quantity >( jsonValue[ "remainingSize" ].value( ) ),
        .createdTime = std::chrono::high_resolution_clock::time_point
        (
            std::chrono::duration_cast< std::chrono::high_resolution_clock::duration >
            (
                std::chrono::duration< double >( jsonValue[ "createdAt" ].get_double( ).value( ) )
            )
        ),
        .marketName = std::string( jsonValue[ "market" ].get_string( ).value( ) ),
        .clientOrderId = jsonValue[ "clientId" ].get_uint64_in_string( ).value( )
    };
}

} // namespace Ftx

} // namespace Synthfi

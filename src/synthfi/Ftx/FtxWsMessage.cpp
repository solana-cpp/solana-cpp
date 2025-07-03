#include "synthfi/Ftx/FtxWsMessage.hpp"

#include <boost/json/object.hpp>

#include <magic_enum/magic_enum.hpp>

namespace Synthfi
{
namespace Ftx
{

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const FtxSubscribeFillsMessage & request )
{
    jsonValue = boost::json::object
    {
        { "op", magic_enum::enum_name( FtxOp::subscribe ) },
        { "channel", magic_enum::enum_name( FtxChannelType::fills ) },
    };
}

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const FtxSubscribeOrdersMessage & request )
{
    jsonValue = boost::json::object
    {
        { "op", magic_enum::enum_name( FtxOp::subscribe ) },
        { "channel", magic_enum::enum_name( FtxChannelType::orders ) },
    };
}

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const SubscriptionRequest & request )
{
    jsonValue = boost::json::object
    {
        { "channel", magic_enum::enum_name( request.channel ) },
        { "market", request.market },
        { "op", magic_enum::enum_name( FtxOp::subscribe ) }
    };
}

FtxOrderbookMessage tag_invoke( json_to_tag< FtxOrderbookMessage >, simdjson::ondemand::value jsonValue )
{
    FtxOrderbookMessage orderbookMessage;

    // Seconds since epoch as a double.
    // Docs are wrong.
    orderbookMessage.time = std::chrono::high_resolution_clock::time_point
    (
        std::chrono::duration_cast< std::chrono::high_resolution_clock::duration >
        (
            std::chrono::duration< double >( jsonValue[ "time" ].get_double( ).value( ) )
        )
    );

    orderbookMessage.checksum = jsonValue[ "checksum" ].get_uint64( ).value( );

    auto & [ bids, asks ] = orderbookMessage.levels;

    auto bidsArray = jsonValue[ "bids" ].get_array( );
    bids.reserve( 100 ); // FTX sends 100 best levels.
    for ( simdjson::ondemand::array level : bidsArray )
    {
        auto levelArrayIt = level.begin( );
        auto price = *levelArrayIt;
        ++levelArrayIt;
        auto quantity = *levelArrayIt;

        bids.emplace_back
        (
            json_to< Trading::Price >( price.value( ) ),
            json_to< Trading::Quantity >( quantity.value( ) )
        );
    }

    auto asksArray = jsonValue[ "asks" ].get_array( );
    asks.reserve( 100 ); // FTX sends 100 best levels.
    for ( simdjson::ondemand::array level : asksArray )
    {
        auto levelArrayIt = level.begin( );
        auto price = *levelArrayIt;
        ++levelArrayIt;
        auto quantity = *levelArrayIt;

        asks.emplace_back
        (
            json_to< Trading::Price >( price.value( ) ),
            json_to< Trading::Quantity >( quantity.value( ) )
        );
    }

    return orderbookMessage;
}

FtxFillMessage tag_invoke( json_to_tag< FtxFillMessage >, simdjson::ondemand::value jsonValue )
{
    return FtxFillMessage
    {
        .side = magic_enum::enum_cast< Trading::Side >( jsonValue[ "side" ].get_string( ).value( ) ).value( ),
        .price = json_to< Trading::Price >( jsonValue[ "price" ].value( ) ),
        .quantity = json_to< Trading::Quantity >( jsonValue[ "quantity" ].value( ) ),
        .fee = json_to< Trading::Price >( jsonValue[ "fee" ].value( ) ),
        .feeRate = jsonValue[ "feeRate" ].get_double( ).value( ),
        .liquidityType = magic_enum::enum_cast< FtxLiquidityType >( jsonValue[ "liquidity" ].get_string( ).value( ) ).value( ),
        .fillId = jsonValue[ "id" ].get_uint64( ).value( ),
        .orderId = jsonValue[ "orderId" ].get_uint64_in_string( ).value( ),
        .tradeId = jsonValue[ "tradeId" ].get_uint64( ).value( ),
        .time = std::chrono::high_resolution_clock::time_point
        (
            std::chrono::duration_cast< std::chrono::high_resolution_clock::duration >
            (
                std::chrono::duration< double >( jsonValue[ "time" ].get_double( ).value( ) )
            )
        ),
        .market = std::string( jsonValue[ "market" ].get_string( ).value( ) )
    };
}

FtxOrderMessage tag_invoke( json_to_tag< FtxOrderMessage >, simdjson::ondemand::value jsonValue )
{
    return FtxOrderMessage
    {
        .side = magic_enum::enum_cast< Trading::Side >( jsonValue[ "side" ].get_string( ).value( ) ).value( ),
        .price = json_to< Trading::Price >( jsonValue[ "price" ].value( ) ),
        .quantity = json_to< Trading::Quantity >( jsonValue[ "quantity" ].value( ) ),
        .fillQuantity = json_to< Trading::Quantity >( jsonValue[ "fillQuantity" ].value( ) ),
        .remainingQuantity = json_to< Trading::Quantity >( jsonValue[ "remainingSize" ].value( ) ),
        .averageFillPrice = json_to< Trading::Price >( jsonValue[ "averageFillPrice" ].value( ) ),
        .orderType = magic_enum::enum_cast< FtxOrderType >( jsonValue[ "type" ].get_string( ).value( ) ).value( ),
        .orderStatus = ftx_order_status_from_string( jsonValue[ "status" ].get_string( ).value( ) ),
        .reduceOnly = jsonValue[ "reduceOnly" ].get_bool( ).value( ),
        .ioc = jsonValue[ "ioc" ].get_bool( ).value( ),
        .postOnly = jsonValue[ "postOnly" ].get_bool( ).value( ),
        .orderId = jsonValue[ "id" ].get_uint64( ).value( ),
        .clientOrderId = jsonValue[ "orderId" ].get_uint64_in_string( ).value( ),
        .time = std::chrono::high_resolution_clock::time_point
        (
            std::chrono::duration_cast< std::chrono::high_resolution_clock::duration >
            (
                std::chrono::duration< double >( jsonValue[ "time" ].get_double( ).value( ) )
            )
        ),
        .market = std::string( jsonValue[ "market" ].get_string( ).value( ) )
    };
}

} // namesapce Ftx
} // namespace Synthfi

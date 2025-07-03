#pragma once

#include "synthfi/Trading/Price.hpp"
#include "synthfi/Trading/Quantity.hpp"
#include "synthfi/Trading/Side.hpp"

#include <boost/algorithm/string.hpp>

#include <magic_enum/magic_enum.hpp>

namespace Synthfi
{
namespace Trading
{

using OrderId = uint64_t;
using ClientOrderId = uint64_t;

inline constexpr uint64_t invalid_order_id( ) { return std::numeric_limits< OrderId >::max( ); }
inline constexpr uint64_t invalid_client_order_id( ) { return std::numeric_limits< ClientOrderId >::max( ); }

enum class OrderState
{
    INVALID = 0,
    NEW = 1,
    OPEN = 2,
    CLOSED = 3
};

enum class OrderType
{
    invalid = 0,
    limit = 1,
    immediateOrCancel = 2
};

inline std::string order_state_to_lower( OrderState orderState )
{
    std::string result( magic_enum::enum_name( orderState ) );
    boost::algorithm::to_lower( result );
    return result;
}

class Order
{
public:
    Order( ) = default;

    Order
    (
        Price price,
        Quantity quantity,
        Side side,
        OrderType orderType,
        uint64_t tradingPairIndex,
        std::string exchangeName
    )
        : _side( side )
        , _price( std::move( price ) )
        , _remainingFillQuantity( std::move( quantity ) )
        , _orderType( orderType )
        , _tradingPairIndex( tradingPairIndex )
        , _exchangeName( std::move( exchangeName ) )
    { }

    const Side & side( ) const & { return _side; }
    Side & side( ) & { return _side; }

    const Price & price( ) const & { return _price; }
    Price & price( ) & { return _price; }

    Price quantity( ) const & { return fill_quantity( ) + remaining_fill_quantity( ); }

    OrderId & order_id( ) & { return _orderId; }
    const OrderId & order_id( ) const & { return _orderId; }

    OrderId & client_order_id( ) & { return _clientOrderId; }
    const OrderId & client_order_id( ) const & { return _clientOrderId; }

    const Price & average_fill_price( ) const & { return _averageFillPrice; }
    Price & average_fill_price( ) & { return _averageFillPrice; }

    const Quantity & fill_quantity( ) const & { return _fillQuantity; }
    Quantity & fill_quantity( ) & { return _fillQuantity; }

    const Quantity & remaining_fill_quantity( ) const & { return _remainingFillQuantity; }
    Quantity & remaining_fill_quantity( ) & { return _remainingFillQuantity; }

    const OrderState & order_state( ) const & { return _orderState; }
    OrderState & order_state( ) & { return _orderState; }

    const OrderType & order_type( ) const & { return _orderType; }

    uint8_t trading_pair_index( ) const & { return _tradingPairIndex; }

    const std::string & exchange_name( ) const & { return _exchangeName; }


private:
    Side _side = Trading::Side::invalid;
    Price _price;
    Price _averageFillPrice = std::numeric_limits< Price >::quiet_NaN( );
    Quantity _fillQuantity = 0;
    Quantity _remainingFillQuantity;

    OrderId _orderId = invalid_order_id( );
    OrderId _clientOrderId = invalid_client_order_id( );

    OrderType _orderType;
    OrderState _orderState = OrderState::NEW;

    uint64_t _tradingPairIndex;
    std::string _exchangeName;
};

} // namespace Trading
} // namespace Synthfi

#include "synthfi/Serum/SerumTypes.hpp"

#include "synthfi/Util/Utils.hpp"
#include "synthfi/Util/Logger.hpp"

namespace Synthfi
{
namespace Serum
{

// Serum adds padding to beginning of account data which ruins alignment. 
[[ nodiscard ]]
static constexpr std::vector< std::byte > align_serum_data( std::vector< std::byte > data )
{
    return std::vector< std::byte >
    {
        std::make_move_iterator( data.begin( ) + serum_begin_padding_size( ) ),
        std::make_move_iterator( data.end( ) - serum_end_padding_size( ) )
    };
};

Trading::Price serum_to_synthfi_price( uint64_t serumPrice, uint8_t tokenDecimals )
{
    return serum_to_synthfi_price( static_cast< Trading::Price >( serumPrice ), tokenDecimals );
}

Trading::Quantity serum_to_synthfi_quantity( uint64_t serumQuantity, uint8_t tokenDecimals )
{
    return serum_to_synthfi_quantity( static_cast< Trading::Quantity >( serumQuantity ), tokenDecimals );
}

Trading::Price serum_to_synthfi_price( Trading::Price serumPrice, uint8_t tokenDecimals )
{
    BOOST_ASSERT_MSG( tokenDecimals < std::numeric_limits< Trading::Price >::digits10, "Insufficient precision to represent token price" );

    return serumPrice / boost::multiprecision::pow( Trading::Price( 10 ), static_cast< Trading::Price >( tokenDecimals ) );
}

Trading::Quantity serum_to_synthfi_quantity( Trading::Quantity serumQuantity, uint8_t tokenDecimals )
{
    BOOST_ASSERT_MSG( tokenDecimals < std::numeric_limits< Trading::Quantity >::digits10, "Insufficient precision to represent token quantity" );

    return serumQuantity / boost::multiprecision::pow( Trading::Quantity( 10 ), static_cast< Trading::Quantity >( tokenDecimals ) );
}

SerumMarketAccount tag_invoke( Solana::account_to_tag< SerumMarketAccount >, Solana::AccountInfo accountInfo )
{
    if ( accountInfo.data.size( ) != SerumMarketAccount::size( ) )
    {
        throw SynthfiError( "Invalid SerumMarketAccount size" );
    }

    auto accountData = align_serum_data( std::move( accountInfo.data ) );
    return *reinterpret_cast< const SerumMarketAccount * >( &accountData[ 0 ] );
}

SerumOpenOrdersAccount tag_invoke( Solana::account_to_tag< SerumOpenOrdersAccount >, Solana::AccountInfo accountInfo )
{
    if ( accountInfo.data.size( ) != SerumOpenOrdersAccount::size( ) )
    {
        throw SynthfiError( "Invalid SerumOpenOrdersAccount size" );
    }

    auto accountData = align_serum_data( std::move( accountInfo.data ) );
    return *reinterpret_cast< const SerumOpenOrdersAccount * >( &accountData[ 0 ] );
}

SerumOrderbookAccount tag_invoke( Solana::account_to_tag< SerumOrderbookAccount >, Solana::AccountInfo accountInfo )
{
    if ( accountInfo.data.size( ) != SerumOrderbookAccount::size( ) )
    {
        throw SynthfiError( "Invalid SerumOrderbookAccount size" );
    }

    SYNTHFI_LOG_ERROR_GLOBAL( ) << "Deserialized orderbook acct";

    // Construct a slice containing aligned slab.
    auto accountData = align_serum_data( std::move( accountInfo.data ) );
    const auto * slabHeader = reinterpret_cast< const SerumSlabHeader * >( &accountData[ 0 ] );

    bool isBid = slabHeader->serumAccountFlags.test( magic_enum::enum_integer( SerumAccountFlag::Bids ) );
    bool isAsk = slabHeader->serumAccountFlags.test( magic_enum::enum_integer( SerumAccountFlag::Asks ) );
    BOOST_ASSERT_MSG( isBid ^ isAsk, "Invalid SerumOrderbookAccount: account must have bids OR asks account flags set." );

    Trading::Side side = isBid ? Trading::Side::bid : Trading::Side::ask;
    SerumOrderbookAccount orderbook{ side, slabHeader->get_leaf_nodes( ) };

    SYNTHFI_LOG_ERROR_GLOBAL( ) << "Done deserializing orderbook acct";

    return orderbook;
};

SerumRequestQueueAccount tag_invoke( Solana::account_to_tag< SerumRequestQueueAccount >, Solana::AccountInfo accountInfo )
{
    if ( accountInfo.data.size( ) < SerumRequestQueue::size( ) )
    {
        throw SynthfiError( "Invalid SerumRequestQueue size" );
    }

    const auto * requestQueue = reinterpret_cast< const SerumRequestQueue * >( &accountInfo.data[ sizeof( serum_begin_padding( ) ) ] );

    size_t requestQueueDataSize = accountInfo.data.size( ) - ( sizeof( serum_begin_padding( ) ) + sizeof( serum_end_padding( ) )  + sizeof( SerumRequestQueue) );
    auto maxItemCount = requestQueueDataSize % sizeof( SerumRequest );

    const auto * data = requestQueue->data( );
    uint64_t head = requestQueue->head;
    uint64_t itemCount = requestQueue->count;

    SerumRequestQueueAccount requestQueueAccount
    {
        .nextSequenceNumber = requestQueue->sequenceNumber,
        .requests = std::vector< SerumRequest >( itemCount )
    };

    for ( size_t i = 0; i < itemCount; ++i )
    {
        std::copy
        (
            &data[ ( head + i ) % maxItemCount ],
            &data[ ( head + i ) % maxItemCount ] + 1,
            &requestQueueAccount.requests[ i ]
        );
    }

    return requestQueueAccount;
}

SerumEventQueueAccount tag_invoke( Solana::account_to_tag< SerumEventQueueAccount >, Solana::AccountInfo accountInfo )
{
    if ( accountInfo.data.size( ) < sizeof( SerumEventQueue ) )
    {
        throw SynthfiError( "Invalid SerumEventQueue size" );
    }

    // Construct a slice containing aligned slab.
    auto accountData = align_serum_data( std::move( accountInfo.data ) );
    const auto * eventQueue = reinterpret_cast< const SerumEventQueue * >( &accountData[ 0 ] );

    size_t eventQueueDataSize = accountData.size( ) - sizeof( SerumEventQueue );
    auto maxItemCount = eventQueueDataSize % sizeof( SerumEvent );

    uint64_t head = eventQueue->head;
    uint64_t itemCount = eventQueue->count;

    const auto * data = reinterpret_cast< const SerumEvent * >( &accountData[ sizeof( SerumEventQueue ) ] );

    SerumEventQueueAccount eventQueueAccount
    {
        .sequenceNumber = eventQueue->sequenceNumber,
        .events = std::vector< SerumEvent >( itemCount )
    };

    SYNTHFI_LOG_ERROR_GLOBAL( ) << "Head: " << head << ", item count: " << itemCount << ", size: " << accountData.size( ) << ", sequence: " << eventQueue->sequenceNumber;

    for ( size_t i = 0; i < itemCount; ++i )
    {
        std::copy
        (
            &data[ ( head + i ) % maxItemCount ],
            &data[ ( head + i ) % maxItemCount ] + 1,
            &eventQueueAccount.events[ i ]
        );
    }

    return eventQueueAccount;
}

} // namespace Serum
} // namespace Synthfi

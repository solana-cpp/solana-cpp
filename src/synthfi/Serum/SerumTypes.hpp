#pragma once

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Solana/SolanaTypes.hpp"
#include "synthfi/Solana/Token.hpp"
#include "synthfi/Solana/Transaction.hpp"
#include "synthfi/Trading/PriceQuantity.hpp"
#include "synthfi/Trading/Side.hpp"
#include "synthfi/Trading/TradingPair.hpp"
#include "synthfi/Util/Logger.hpp"
#include "synthfi/Util/Utils.hpp"

#include <magic_enum/magic_enum.hpp>

#include <array>
#include <bitset>
#include <variant>
#include <vector>

namespace Synthfi
{
namespace Serum
{

Trading::Price serum_to_synthfi_price( uint64_t serumPrice, uint8_t tokenDecimals );
Trading::Quantity serum_to_synthfi_quantity( uint64_t serumQuantity, uint8_t tokenDecimals );

Trading::Price serum_to_synthfi_price( Trading::Price serumPrice, uint8_t tokenDecimals );
Trading::Quantity serum_to_synthfi_quantity( Trading::Quantity serumQuantity, uint8_t tokenDecimals );

inline Trading::Quantity serum_taker_fee( ) { return Trading::Quantity{ 0.0003 }; } // 3 bps

template< class AlignType >
size_t serum_align( size_t bytes )
{
    return bytes % sizeof( AlignType );
}

enum class SerumAccountFlag : uint8_t
{
    Initialized = 0,
    Market = 1,
    OpenOrders = 2,
    RequestQueue = 3,
    EventQueue = 4,
    Bids = 5,
    Asks = 6,
    Disabled = 7,
    Closed = 8,
    Permissioned = 9,
    CrankAuthorityRequired = 10
};

enum class SerumOrderType : uint8_t
{
    Limit = 0,
    ImmediateOrCancel = 1,
    PostOnly = 2
};

static constexpr size_t serum_begin_padding_size( ) { return 5; }
static constexpr size_t serum_end_padding_size( ) { return 7; }

static constexpr std::array< char, serum_begin_padding_size( ) > serum_begin_padding( ) { return { 's', 'e', 'r', 'u', 'm' }; }
static constexpr std::array< char, serum_end_padding_size( ) > serum_end_padding( ) { return { 'p', 'a', 'd', 'd', 'i', 'n', 'g' }; }

struct SerumMarketAccount
{
    static constexpr size_t size( ) { return 388; };

    std::bitset< 64 > SerumAccountFlags;
    Core::PublicKey owner;
    uint64_t vaultSignerNonce;
    Core::PublicKey baseMint;
    Core::PublicKey quoteMint;
    Core::PublicKey baseVault;
    uint64_t baseDepositsTotal;
    uint64_t baseFeesAccrued;
    Core::PublicKey quoteVault;
    uint64_t quoteDepositsTotal;
    uint64_t quoteFeesAccrued;
    uint64_t quoteDustThreshold;
    Core::PublicKey requestQueue;
    Core::PublicKey eventQueue;
    Core::PublicKey bids;
    Core::PublicKey asks;
    uint64_t baseLotSize;
    uint64_t quoteLotSize;
    uint64_t feeRateBasisPoints;
    uint64_t referrerRebatesAccrued;

    friend SerumMarketAccount tag_invoke( Solana::account_to_tag< SerumMarketAccount >, Solana::AccountInfo accountInfo );
};
static_assert
(
    ( sizeof( SerumMarketAccount ) + serum_begin_padding_size( ) + serum_end_padding_size( ) ) == SerumMarketAccount::size( ),
    "Unexpected SerumMarketAccount size"
);

struct SerumNodeKey
{
    constexpr Trading::Side side( ) const &
    {
        return sequenceNumber > static_cast< uint64_t>( std::numeric_limits< int64_t >::max( ) )
            ? Trading::Side::bid
            : Trading::Side::ask;
    }

    constexpr uint64_t sequence_number( ) const & { return side( ) == Trading::Side::bid ? ~sequenceNumber : sequenceNumber; }

    constexpr uint64_t limit_price( ) const & { return limitPrice; }

    SerumNodeKey operator & ( const SerumNodeKey & other ) const
    {
        return SerumNodeKey{ limitPrice & other.limitPrice, sequenceNumber & other.sequenceNumber };
    }

    auto operator<=>( const SerumNodeKey& ) const & = default;

    friend std::ostream & operator <<( std::ostream & os, const SerumNodeKey & key )
    {
        return os <<
            fmt::format
            (
                "[{}|{}:{}]",
                key.sequence_number( ),
                key.limit_price( ),
                magic_enum::enum_name( key.side( ) )
            );
    }

    uint64_t sequenceNumber;
    uint64_t limitPrice;
};
using SerumNodeIndex = uint32_t; // uint32 key.

struct SerumOpenOrdersAccount
{
    static constexpr size_t size( ) { return 3228; };

    std::bitset< 64 > serumAccountFlags;

    Core::PublicKey market;
    Core::PublicKey owner;

    uint64_t nativeBaseFree;
    uint64_t nativeBaseTotal;

    uint64_t nativeQuoteFree;
    uint64_t nativeQuoteTotal;

    std::bitset< 128 > freeSlotBits;
    std::bitset< 128 > isBidBits;

    std::array< SerumNodeKey, 128 > orders;
    std::array< uint64_t, 128 > clientOrderIds;
    uint64_t referrerRebatesAccrued;

    friend SerumOpenOrdersAccount tag_invoke( Solana::account_to_tag< SerumOpenOrdersAccount >, Solana::AccountInfo accountInfo );
};
static_assert
(
    sizeof( SerumOpenOrdersAccount ) + sizeof( serum_begin_padding( ) ) + sizeof( serum_end_padding( ) ) == SerumOpenOrdersAccount::size( ),
    "Unexpected SerumOpenOrdersAccount size"
);

static constexpr size_t serum_node_size( ) { return 72; }

enum class SerumNodeTag : uint32_t
{
    Uninitialized = 0,
    InnerNode = 1,
    LeafNode = 2,
    FreeNode = 3,
    LastFreeNode = 4
};

struct SerumInnerNode
{
    /*
    SerumNodeIndex walk_down( SerumNodeKey searchKey )
    {
        SerumNodeKey critBitMask = ( SerumNodeKey{ 0x1 } <<= 127 ) >>= prefixLength;
        bool critBit = ( searchKey & critBitMask ).any( );
        return children[ critBit ? 1 : 0 ];
    }
    */

    friend std::ostream & operator <<( std::ostream & os, const SerumInnerNode & node )
    {
        return os
            << fmt::format
            (
                "Tag: {}, prefix length: {}, key: {}, children: [{},{}]",
                magic_enum::enum_name( node.tag ),
                node.prefixLength,
                node.key,
                node.children[ 0 ],
                node.children[ 1 ]
            );
    }

    SerumNodeTag tag;
    uint32_t prefixLength;
    SerumNodeKey key;
    std::array< SerumNodeIndex, 2 > children;
    std::array< uint64_t, 5 > _padding;
};
static_assert( sizeof( SerumInnerNode ) == serum_node_size( ), "Unexpected SerumInnerNode size" );

struct SerumLeafNode
{
    auto operator<=>( const SerumLeafNode& ) const & = default;

    friend std::ostream & operator <<( std::ostream & os, const SerumLeafNode & node )
    {
        return os
            << fmt::format
            (
                "Tag: {}, owner slot: {:d}, fee tier: {:d}, owner id(key): {}, owner: {}, quantity: {}, client order id: {}",
                magic_enum::enum_name( node.tag ),
                node.ownerSlot,
                node.fee_tier,
                node.key,
                node.owner,
                node.quantity,
                node.clientOrderId
            );
    }

    SerumNodeTag tag;
    uint8_t ownerSlot;
    uint8_t fee_tier;
    std::array< uint8_t, 2 > _padding;
    SerumNodeKey key;
    Core::PublicKey owner;
    uint64_t quantity;
    uint64_t clientOrderId;
};
static_assert( sizeof( SerumLeafNode ) == serum_node_size( ), "Unexpected SerumLeafNode size" );

struct SerumFreeNode
{
    friend std::ostream & operator <<( std::ostream & os, SerumFreeNode const & node )
    {
        return os
            << fmt::format
            (
                "Tag: {}, next: {}",
                magic_enum::enum_name( node.tag ),
                node.next
            );
    }

    SerumNodeTag tag;
    SerumNodeIndex next;
    std::array< uint64_t, 8 > _padding;
};
static_assert( sizeof( SerumFreeNode ) == serum_node_size( ), "Unexpected SerumFreeNode size" );

struct SerumSlabHeader
{
    using SerumNodeVariant = std::variant< const SerumInnerNode *, const SerumLeafNode *, const SerumFreeNode * >;

    SerumNodeVariant get_node( SerumNodeIndex index ) const
    {
        const auto * nodeTag = reinterpret_cast< const SerumNodeTag * >( reinterpret_cast< const char * >( this + 1 ) + ( serum_node_size( ) * index ) );
        switch ( *nodeTag )
        {
            case SerumNodeTag::InnerNode:
            {
                return reinterpret_cast< const SerumInnerNode * >( nodeTag );
            }
            case SerumNodeTag::LeafNode:
            {
                return reinterpret_cast< const SerumLeafNode * >( nodeTag );

            }
            case SerumNodeTag::FreeNode:
            {
                return reinterpret_cast< const SerumFreeNode * >( nodeTag );
            }
            case SerumNodeTag::LastFreeNode:
            {
                return reinterpret_cast< const SerumFreeNode * >( nodeTag );
            }
            case SerumNodeTag::Uninitialized:
            {
                BOOST_ASSERT_MSG( false, "Uninitialized SerumNode");
                return reinterpret_cast< const SerumFreeNode * >( nodeTag );
            }
            default:
            {
                BOOST_ASSERT_MSG( false, "Invalid SerumNode");
                return reinterpret_cast< const SerumFreeNode * >( nodeTag );
            }
        }
    }

    // TODO: could probably do this with a view, no allocations.
    std::vector< SerumLeafNode > get_leaf_nodes( ) const
    {
        std::vector< SerumLeafNode > nodes;
        get_leaf_nodes( nodes, get_node( rootNode ) );
        return nodes;
    }

    std::ostream & print_tree( std::ostream & os ) const
    {
        return print_tree( os, get_node( rootNode ) );
    }

    friend std::ostream & operator << ( std::ostream & os, const SerumSlabHeader & header )
    {
        return os
            << fmt::format
            (
                "Account flags: {}, bump index: {}, free list length: {}, free list head: {}, root node: {}, leaf count: {}",
                header.serumAccountFlags,
                header.bumpIndex,
                header.freeListLength,
                header.freeListHead,
                header.rootNode,
                header.leafCount
            );
    }

    std::bitset< 64 > serumAccountFlags;
    uint64_t bumpIndex;
    uint64_t freeListLength;
    SerumNodeIndex freeListHead;
    SerumNodeIndex rootNode;
    uint64_t leafCount;

private:
    void get_leaf_nodes( std::vector< SerumLeafNode > & nodes, SerumNodeVariant node ) const
    {
        std::visit
        (
            [ this, &nodes ]< class NodeType >( const NodeType * node )
            {
                if constexpr ( std::is_same_v< NodeType, SerumInnerNode > )
                {
                    get_leaf_nodes( nodes, get_node( node->children[ 0 ] ) );
                    get_leaf_nodes( nodes, get_node( node->children[ 1 ] ) );
                    return;
                }
                if constexpr ( std::is_same_v< NodeType, SerumLeafNode > )
                {
                    nodes.emplace_back( *node );
                    return;
                }
                if constexpr ( std::is_same_v< NodeType, SerumFreeNode > )
                {
                    return;
                }
            },
            node
        );
    }

    std::ostream & print_tree( std::ostream & os, SerumNodeVariant node ) const
    {
        std::visit
        (
            [ this, &os ]< class NodeType >( const NodeType * node )
            {
                if constexpr ( std::is_same_v< NodeType, SerumInnerNode > )
                {
                    os << *node << "\n";
                    print_tree( os, get_node( node->children[ 0 ] ) );
                    print_tree( os, get_node( node->children[ 1 ] ) );
                }
                if constexpr ( std::is_same_v< NodeType, SerumLeafNode > )
                {
                    os << *node << "\n";
                }
                if constexpr ( std::is_same_v< NodeType, SerumFreeNode > )
                {
                    os << *node << "\n";
                }
            },
            node
        );
        return os;
    }
};

static_assert( sizeof( SerumSlabHeader ) == 40, "Unexpected SerumSlabHeader size" );

struct SerumOrderbookAccount
{
    static constexpr size_t size( ) { return 65548; }

    Trading::Side side;
    std::vector< SerumLeafNode > orders;

    friend SerumOrderbookAccount tag_invoke( Solana::account_to_tag< SerumOrderbookAccount >, Solana::AccountInfo accountInfo );
};

template< class ItemType >
struct SerumQueueHeader
{
    static constexpr size_t size( ) { return 32; }

    std::bitset< 64 > SerumAccountFlags;
    uint64_t head;
    uint64_t count;
    uint64_t sequenceNumber;

    friend std::ostream & operator << ( std::ostream & os, const SerumQueueHeader< ItemType > & header )
    {
        return os <<
            fmt::format
            (
                "Account flags: {}, head: {}, count: {}, sequence number: {}",
                header.SerumAccountFlags,
                header.head,
                header.count,
                header.sequenceNumber
            );
    }

    const ItemType * data( ) const
    {
        return reinterpret_cast< const ItemType * >( reinterpret_cast< const uint8_t * >( this + 1 ) );
    }
};

enum class RequestMasks : uint8_t
{
    NewOrder = 0x01,
    CancelOrder = 0x02,
    Bid = 0x04,
    PostOnly = 0x08,
    ImmediateOrCancel = 0x10,
    DecrementTakeOnSelfTrade = 0x20
};

enum class SerumSelfTradeBehavior : uint8_t
{
    DecrementTake = 0,
    CancelProvide = 1,
    AbortTransaction = 2
};

struct SerumRequest
{
    static constexpr size_t size( ) { return 80; }

    uint8_t requestFlags; // using byte representation, bitset is 8 bytes.
    uint8_t ownerSlot;
    uint8_t feeTier;
    uint8_t selfTradeBehavior;
    std::array< uint8_t, 4 > _padding;
    uint64_t maxCoinQtyOrCancelId;
    uint64_t nativeQuoteQtyLocked;
    SerumNodeKey orderId;
    Core::PublicKey owner;
    uint64_t clientOrderId;

    friend std::ostream & operator <<( std::ostream & os, const SerumRequest & request )
    {
        return os
            << fmt::format
            (
                "Request flags: {:d}, owner slot: {:d}, fee tier: {:d}, max coin quantity or cancel id: {}"
                " max coin quote quantity locked: {} order id: {}, owner: {}, client order id: {}",
                request.requestFlags,
                request.ownerSlot,
                request.feeTier,
                request.maxCoinQtyOrCancelId,
                request.nativeQuoteQtyLocked,
                request.orderId,
                request.owner,
                request.clientOrderId
            );
    }
};
static_assert( sizeof( SerumRequest ) == SerumRequest::size( ), "Unexpected SerumRequest size" );

using SerumRequestQueue = SerumQueueHeader< SerumRequest >;
static_assert( sizeof( SerumRequestQueue ) == SerumRequestQueue::size( ), "Unexpected SerumRequestQueue size" );

struct SerumRequestQueueAccount
{
    friend SerumRequestQueueAccount tag_invoke( Solana::account_to_tag< SerumRequestQueueAccount >, Solana::AccountInfo accountInfo );

    uint64_t nextSequenceNumber;
    std::vector< SerumRequest > requests;
};

enum class EventMasks : uint8_t
{
    Fill = 0x01,
    Out = 0x02,
    Bid = 0x04,
    Maker = 0x08,
    ReleaseFunds = 0x10
};

struct SerumEvent
{
    static constexpr size_t size( ) { return 88; }

    friend std::ostream & operator <<( std::ostream & os, const SerumEvent & event )
    {
        return os
            << fmt::format
            (
                "Event flags: {:d}, owner slot: {:d}, fee tier: {:d}, native quantity released: {}, native quantity paid: {}"
                " native fee or rebate: {}, order id: {}, owner: {}, client order id: {}",
                event.eventFlags,
                event.ownerSlot,
                event.feeTier,
                event.nativeQtyReleased,
                event.nativeQtyPaid,
                event.nativeFeeOrRebate,
                event.orderId,
                event.owner,
                event.clientOrderId
            );
    }

    uint8_t eventFlags; // using byte representation, bitset is 8 bytes.
    uint8_t ownerSlot;
    uint8_t feeTier;
    std::array< uint8_t, 5 > _padding;
    uint64_t nativeQtyReleased;
    uint64_t nativeQtyPaid;
    uint64_t nativeFeeOrRebate;
    SerumNodeKey orderId;
    Core::PublicKey owner;
    uint64_t clientOrderId;
};
static_assert( sizeof( SerumEvent ) == SerumEvent::size( ), "Unexpected Event size" );

using SerumEventQueue = SerumQueueHeader< SerumEvent >;
static_assert( sizeof( SerumEventQueue ) == SerumEventQueue::size( ), "Unexpected SerumEventQueue size" );

struct SerumEventQueueAccount
{
    friend SerumEventQueueAccount tag_invoke( Solana::account_to_tag< SerumEventQueueAccount >, Solana::AccountInfo accountInfo );

    uint64_t sequenceNumber;
    std::vector< SerumEvent > events;
};

// Serum Transactions
template< uint16_t ProgramId, uint8_t ... AccountIndices >
struct SendTakeInstruction
    : Solana::Instruction
    <
        ProgramId,
        Solana::AccountIndexArray< AccountIndices ... >,
        uint32_t,
        uint64_t,
        uint64_t,
        uint64_t,
        uint64_t,
        uint64_t,
        uint16_t
    >
{
    using BaseType = Solana::Instruction
    <
        ProgramId,
        Solana::AccountIndexArray< AccountIndices ... >,
        uint32_t,
        uint64_t,
        uint64_t,
        uint64_t,
        uint64_t,
        uint64_t,
        uint16_t
    >;

    SendTakeInstruction
    (
        Trading::Side side,
        Trading::Price price,
        Trading::Quantity maxBaseQuantity,
        Trading::Quantity maxQuoteQuantity,
        Trading::Quantity minCoinQuantity,
        Trading::Quantity minQuoteQuantity
    )
    : BaseType( ) { }
};

// Serum reference data types.
struct SerumTradingPair
{
    Core::PublicKey serumMarketAddress;
    SerumMarketAccount serumMarketAccount;

    Trading::Price priceIncrement;
    Trading::Quantity quantityIncrement;

    uint64_t baseCurrencyIndex;
    uint64_t quoteCurrencyIndex;
};

struct SerumCurrency
{
    Core::PublicKey mintAddress;
    Solana::TokenMintAccount mintAccount;
};

struct SerumReferenceData
{
    Core::PublicKey serumProgramId;
    std::vector< SerumTradingPair > tradingPairs;
    std::vector< SerumCurrency > currencies;
};

} // namespace Serum
} // namespace Synthfi

#pragma once

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Serum/SerumTypes.hpp"
#include "synthfi/Solana/SolanaTypes.hpp"
#include "synthfi/Solana/Token.hpp"
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

#include <boost/multiprecision/cpp_bin_float.hpp>

#include <fmt/core.h>
#include <fmt/ostream.h>

namespace Synthfi
{
namespace Mango
{

__extension__ typedef __int128 int128_t;
__extension__ typedef unsigned __int128 uint128_t;

// 64 bit aligned u128.
// Rust ABI uses 64 bit alignment for 128 bit integral types.
struct Aligned128
{
    constexpr operator int128_t( ) const { return static_cast< int128_t>( ( static_cast< uint128_t >( upper ) << 64 )  + lower ); }

    uint64_t lower;
    uint64_t upper;
};

uint8_t constexpr mango_max_tokens( ) { return 16; }
uint8_t constexpr mango_max_pairs( ) { return mango_max_tokens( ) - 1; }
uint8_t constexpr mango_quote_currency_index( ) { return mango_max_tokens( ) - 1; }

uint8_t constexpr mango_max_perp_open_orders( ) { return 64; }
uint8_t constexpr mango_info_length( ) { return 32; }
uint8_t constexpr mango_max_node_banks( ) { return 8; }
uint8_t constexpr max_num_in_margin_basket( ) { return 9; }

uint64_t constexpr mango_scale_factor( ) { return 1L << 48; }

inline Trading::Price mango_int128_to_price( int128_t price )
{
    return Trading::Price( price ) / mango_scale_factor( );
}

inline Trading::Quantity mango_int128_to_quantity( int128_t quantity )
{
    return Trading::Quantity( quantity ) / mango_scale_factor( );
}

inline Trading::Price mango_int128_to_price( Aligned128 price )
{
    return Trading::Price( static_cast< int128_t >( price ) ) / mango_scale_factor( );
}

inline Trading::Quantity mango_int128_to_quantity( Aligned128 quantity )
{
    return Trading::Quantity( static_cast< int128_t >( quantity ) ) / mango_scale_factor( );
}

enum class MangoSide : uint8_t
{
    bid = 0,
    ask = 1
};

enum class MangoDataType : uint8_t
{
    MangoGroup = 0,
    MangoAccount = 1,
    RootBank = 2,
    NodeBank = 3,
    PerpMarket = 4,
    Bids = 5,
    Asks = 6,
    MangoCache = 7,
    EventQueue = 8,
    AdvancedOrder = 9,
    ReferrerMemory = 10,
    ReferrerIdRecord = 11
};

enum class DataType
{
    Maint = 0,
    Init = 1,
    Equity = 2
};

enum class AssetType : uint8_t
{
    Token = 0,
    Perp = 1
};

enum class MangoOrderType : uint8_t
{
    Limit = 0,

    /// Take existing orders up to price, max_base_quantity and max_quote_quantity.
    /// Never place an order on the book.
    ImmediateOrCancel = 1,

    /// Never take any existing orders, post the order on the book if possible.
    /// If existing orders can match with this order, do nothing.
    PostOnly = 2,

    /// Ignore price and take orders up to max_base_quantity and max_quote_quantity.
    /// Never place an order on the book.
    ///
    /// Equivalent to ImmediateOrCancel with price=i64::MAX.
    Market = 3,

    /// If existing orders match with this order, adjust the price to just barely
    /// not match. Always places an order on the book.
    PostOnlySlide = 4
};

struct MangoMetadata
{
    friend std::ostream & operator <<( std::ostream & os, const MangoMetadata & metadata );

    MangoDataType dataType;
    uint8_t version;
    uint8_t isInitialized;
    std::array< uint8_t, 5 > extraInfo;
};
static_assert( sizeof( MangoMetadata ) == 8, "Unexpected MangoMetadata size" );

struct RootBank
{
    static constexpr size_t size( ) { return 424; };

    friend RootBank tag_invoke( Solana::account_to_tag< RootBank >, Solana::AccountInfo accountInfo );
    friend std::ostream & operator <<( std::ostream & os, const RootBank & rootBank );

    MangoMetadata metadata;

    Aligned128 optimalUtilization;
    Aligned128 optimalRate;
    Aligned128 maximumRate;

    uint64_t numNodeBanks;
    std::array< Core::PublicKey, mango_max_node_banks( ) > nodeBanks;

    Aligned128 depositIndex;
    Aligned128 borrowIndex;

    uint64_t lastUpdated;

    std::array< uint8_t, 64 > _padding;

    Trading::Price optimal_utilization( ) const &;
    Trading::Price optimal_rate( ) const &;
    Trading::Price maximum_rate( ) const &;
};
static_assert( sizeof( RootBank ) == RootBank::size( ), "Unexpected RootBank size" );

struct PriceCache
{
    Aligned128 price; // Unit is interpreted as how many quote native tokens for 1 base native token.
    uint64_t lastUpdate;
};

struct RootBankCache
{
    Aligned128 depositIndex;
    Aligned128 borrowIndex;
    uint64_t lastUpdate;
};

struct PerpMarketCache
{
    Aligned128 longFunding;
    Aligned128 shortFunding;
    uint64_t lastUpdate;
};

struct MangoCacheAccount
{
    static constexpr size_t size( ) { return 1608; };

    friend MangoCacheAccount tag_invoke( Solana::account_to_tag< MangoCacheAccount >, Solana::AccountInfo accountInfo );

    MangoMetadata metadata;

    std::array< PriceCache, mango_max_pairs( ) > priceCaches;
    std::array< RootBankCache, mango_max_tokens( ) > rootBankCaches;
    std::array< PerpMarketCache, mango_max_pairs( ) > perpMarketCaches;
}
;
static_assert( sizeof( MangoCacheAccount ) == MangoCacheAccount::size( ), "Unexpected MangoCache size" );

struct NodeBank
{
    MangoMetadata metadata;

    Aligned128 deposits;
    Aligned128 borrows;
    Core::PublicKey vaultAddress;

    static constexpr size_t size( ) { return 72; };

    friend NodeBank tag_invoke( Solana::account_to_tag< NodeBank >, Solana::AccountInfo accountInfo );
};
static_assert( sizeof( NodeBank ) == NodeBank::size( ), "Unexpected NodeBank size" );

struct MangoTokenInfo
{
    Core::PublicKey mintAddress;
    Core::PublicKey rootBankAddress;
    uint8_t decimals;
    std::array< uint8_t, 7 > _padding;

    bool empty( ) const { return mintAddress.is_zero( ); }

    friend std::ostream & operator <<( std::ostream & os, const MangoTokenInfo & tokenInfo )
    {
        return os
            << fmt::format
            (
                "Mint address: {}, root bank address: {}, decimals: {:d}",
                tokenInfo.mintAddress,
                tokenInfo.rootBankAddress,
                tokenInfo.decimals
            );
    }
};
static_assert( sizeof( MangoTokenInfo ) == 72 , "Unexpected MangoTokenInfo size" );

struct SpotMarketInfo
{
    Core::PublicKey spotMarketAddress;
    int128_t maintenanceAssetWeight;
    int128_t initialAssetWeight;
    int128_t maintenanceLiabilityWeight;
    int128_t initialLiabilityWeight;
    int128_t liquidationFee;

    Trading::Price maintenance_asset_weight( ) const &;
    Trading::Price initial_asset_weight( ) const &;
    Trading::Price maintenance_liability_weight( ) const &;
    Trading::Price initial_liability_weight( ) const &;
    Trading::Price liquidation_fee( ) const &;

    friend std::ostream & operator <<( std::ostream & os, const SpotMarketInfo & marketInfo )
    {
        return os
            << fmt::format
            (
                "Spot market address: {}",
                marketInfo.spotMarketAddress
            );
    }
};
static_assert( sizeof( SpotMarketInfo ) == 112 , "Unexpected MangoTokenInfo size" );

struct PerpMarketInfo
{
    Core::PublicKey perpMarketAddress;

    int128_t maintenanceAssetWeight;
    int128_t initialAssetWeight;
    int128_t maintenanceLiabilityWeight;
    int128_t initialLiabilityWeight;
    int128_t liquidationFee;
    int128_t makerFee;
    int128_t takerFee;
    uint64_t baseLotSize;
    uint64_t quoteLotSize;

    friend std::ostream & operator <<( std::ostream & os, const PerpMarketInfo & marketInfo )
    {
        return os
            << fmt::format
            (
                "Perp market address: {}, base lot size: {}, quote lot size: {}",
                marketInfo.perpMarketAddress,
                marketInfo.baseLotSize,
                marketInfo.quoteLotSize
            );
    }
};
static_assert( sizeof( PerpMarketInfo ) == 160, "Unexpected MangoTokenInfo size" );

struct MangoGroupAccount
{
    MangoMetadata metadata;
    uint64_t numOracles;

    std::array< MangoTokenInfo, mango_max_tokens( ) > tokens;
    std::array< SpotMarketInfo, mango_max_pairs( ) > spotMarkets;
    std::array< PerpMarketInfo, mango_max_pairs( ) > perpMarkets;

    std::array< Core::PublicKey, mango_max_pairs( ) > oracles;

    uint64_t signerNonce;
    Core::PublicKey signerKey;
    Core::PublicKey admin;
    Core::PublicKey dexProgramId;
    Core::PublicKey mangoCache;
    uint64_t validInterval;

    Core::PublicKey insuranceVault;
    Core::PublicKey serumVault;
    Core::PublicKey msrmVault;
    Core::PublicKey feesVault;

    uint32_t maxMangoAccounts;
    uint32_t numMangoAccounts;

    uint32_t refRurchargeCentibps;
    uint32_t refShareCentibps;
    uint64_t refMngoRequired;
    std::array< uint8_t, 8 > _padding;

    static constexpr size_t size( ) { return 6032; };

    friend std::ostream & operator <<( std::ostream & os, const MangoGroupAccount & marketInfo )
    {
        return os
            << fmt::format
            (
                "Metadata: {}, num oracles: {:d}, tokens: {}, spot markets: {}",
                marketInfo.metadata,
                marketInfo.numOracles,
                fmt::join( marketInfo.tokens, "," ),
                fmt::join( marketInfo.spotMarkets, "," )
            );
    }

    friend MangoGroupAccount tag_invoke( Solana::account_to_tag< MangoGroupAccount >, Solana::AccountInfo accountInfo );
};
static_assert( sizeof( MangoGroupAccount ) == 6032, "Unexpected MangoGroup size" );

struct MangoPerpAccount
{
    int64_t basePosition;
    Aligned128 quotePosition;
    Aligned128 longSettledFunding;
    Aligned128 shortSettledFunding;

    int64_t bidsQuantity;
    int64_t asksQuantity;

    int64_t takerBase;
    int64_t takerQuote;
    int64_t mangoAccrued;

    friend std::ostream & operator <<( std::ostream & os, MangoPerpAccount perpAccount )
    {
        return os
            << fmt::format
            (
                "Base position: {}, bids quantity: {}, asks quantity: {}, taker base: {}, quote base: {}, mango accrued: {}",
                perpAccount.basePosition,
                perpAccount.bidsQuantity,
                perpAccount.asksQuantity,
                perpAccount.takerBase,
                perpAccount.takerQuote,
                perpAccount.mangoAccrued
            );
    }
};

struct MangoAccount
{
    MangoMetadata metadata;

    Core::PublicKey mangoGroupAddress;
    Core::PublicKey owner;

    std::array< bool, mango_max_pairs( ) > inMarginBasket;
    uint8_t numInMarginBasket;

    std::array< Aligned128, mango_max_tokens( ) > deposits; // bitsets for alignment.
    std::array< Aligned128, mango_max_tokens( ) > borrows;
    std::array< Core::PublicKey, mango_max_pairs( ) > openOrdersAddresses;

    std::array< MangoPerpAccount, mango_max_pairs( ) > perpAccounts;
    std::array< uint8_t, mango_max_perp_open_orders( ) > orderMarket;
    std::array< MangoSide, mango_max_perp_open_orders( ) > perpSide;
    std::array< Aligned128, mango_max_perp_open_orders( ) > orders;
    std::array< uint64_t, mango_max_perp_open_orders( ) > clientOrderIds;

    uint64_t msrmAmount;
    bool beingLiquidated;
    bool isBankrupt;
    std::array< uint8_t, mango_info_length( ) > info;
    Core::PublicKey advancedOrdersKey;
    bool notUpgradable;
    Core::PublicKey delegate;
    std::array< uint8_t, 5 > _padding;

    static constexpr size_t size( ) { return 4296; };

    friend std::ostream & operator <<( std::ostream & os, const MangoAccount & mangoAccount )
    {
        return os
            << fmt::format
            (
                "Metadata: {}, mango group address: {}, owner: {}",
                mangoAccount.metadata,
                mangoAccount.mangoGroupAddress,
                mangoAccount.owner
            );
    }

    friend MangoAccount tag_invoke( Solana::account_to_tag< MangoAccount >, Solana::AccountInfo accountInfo );
};
static_assert
(
    sizeof( MangoAccount ) == MangoAccount::size( ),
    "Unexpected MangoAccount size"
);


struct MangoTradingPair
{
    uint64_t mangoTradingPairIndex;
    uint64_t mangoBaseCurrencyIndex;
    uint64_t mangoQuoteCurrencyIndex;

    Serum::SerumOpenOrdersAccount openOrdersAccount;
};

struct MangoCurrency
{
    uint64_t mangoCurrencyIndex;

    Core::PublicKey nodeBankAddress;

    NodeBank nodeBank;
    RootBank rootBank;
};

struct MangoReferenceData
{
    MangoReferenceData( ) = default;

    Core::PublicKey mangoAccountAddress;
    Core::PublicKey mangoProgramId;

    MangoGroupAccount mangoGroup;
    MangoAccount mangoAccount;
    MangoCacheAccount mangoCache;

    std::vector< MangoTradingPair > tradingPairs;
    std::vector< MangoCurrency > currencies;

    void get_position_component( std::vector< Trading::Quantity > & positionComponent ) const &;

    void get_health_components
    (
        Trading::Quantity & quoteComponent,
        std::vector< Trading::Quantity > & spotComponent,
        std::vector< Trading::Price > & priceComponent,
        std::vector< Trading::Price > & weightComponent,
        const std::vector< Trading::Quantity > & positions
    ) const &;

    Trading::Quantity get_health_from_components
    (
        const Trading::Quantity & quoteComponent,
        const std::vector< Trading::Quantity > & spotComponent,
        const std::vector< Trading::Price > & priceComponent,
        const std::vector< Trading::Price > & weightComponent
    ) const &;
};

} // namespace Mango
} // namespace Synthfi

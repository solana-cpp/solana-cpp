#include "synthfi/Mango/MangoTypes.hpp"

#include "synthfi/Util/Utils.hpp"

namespace Synthfi
{
namespace Mango
{

std::ostream & operator <<( std::ostream & os, const MangoMetadata & metadata )
{
    return os
        << fmt::format
        (
            "Data type: {}, version: {:d}, initialized: {:d}",
            magic_enum::enum_name( metadata.dataType ),
            metadata.version,
            metadata.isInitialized
        );
}

MangoCacheAccount tag_invoke( Solana::account_to_tag< MangoCacheAccount >, Solana::AccountInfo accountInfo )
{
    if ( accountInfo.data.size( ) < MangoCacheAccount::size( ) )
    {
        throw SynthfiError( "Invalid MangoCacheAccount size" );
    }
    return *( reinterpret_cast< const MangoCacheAccount * >( &accountInfo.data[ 0 ] ) );
}

RootBank tag_invoke( Solana::account_to_tag< RootBank >, Solana::AccountInfo accountInfo )
{
    if ( accountInfo.data.size( ) < RootBank::size( ) )
    {
        throw SynthfiError( "Invalid RootBank size" );
    }
    return *( reinterpret_cast< const RootBank * >( &accountInfo.data[ 0 ] ) );
}

std::ostream & operator <<( std::ostream & os, const RootBank & rootBank )
{
    return os
        << fmt::format
        (
            "Metadata: {}, deposit index: {}, borrow index: {}",
            rootBank.metadata,
            mango_int128_to_quantity( rootBank.borrowIndex ),
            mango_int128_to_quantity( rootBank.depositIndex )
        ); 
}

NodeBank tag_invoke( Solana::account_to_tag< NodeBank >, Solana::AccountInfo accountInfo )
{
    if ( accountInfo.data.size( ) < NodeBank::size( ) )
    {
        throw SynthfiError( "Invalid RootBank size" );
    }
    return *( reinterpret_cast< const NodeBank * >( &accountInfo.data[ 0 ] ) );
}

MangoGroupAccount tag_invoke( Solana::account_to_tag< MangoGroupAccount >, Solana::AccountInfo accountInfo )
{
    if ( accountInfo.data.size( ) < MangoGroupAccount::size( ) )
    {
        throw SynthfiError( "Invalid MangoGroupAccount size" );
    }
    return *( reinterpret_cast< const MangoGroupAccount * >( &accountInfo.data[ 0 ] ) );
};

MangoAccount tag_invoke( Solana::account_to_tag< MangoAccount >, Solana::AccountInfo accountInfo )
{
    if ( accountInfo.data.size( ) < MangoAccount::size( ) )
    {
        throw SynthfiError( "Invalid MangoAccount size" );
    }
    return *( reinterpret_cast< const MangoAccount * >( &accountInfo.data[ 0 ] ) );
};

Trading::Price RootBank::optimal_utilization( ) const &
{
    return mango_int128_to_price( optimalUtilization );
}

Trading::Price RootBank::optimal_rate( ) const &
{
    return mango_int128_to_price( optimalRate );
}

Trading::Price RootBank::maximum_rate( ) const &
{
    return mango_int128_to_price( maximumRate );
}

Trading::Price SpotMarketInfo::maintenance_asset_weight( ) const &
{
    return mango_int128_to_price( maintenanceAssetWeight );
}

Trading::Price SpotMarketInfo::initial_asset_weight( ) const &
{
    return mango_int128_to_price( initialAssetWeight );
}

Trading::Price SpotMarketInfo::maintenance_liability_weight( ) const &
{
    return mango_int128_to_price( maintenanceLiabilityWeight );
}

Trading::Price SpotMarketInfo::initial_liability_weight( ) const &
{
    return mango_int128_to_price( initialLiabilityWeight );
}

Trading::Price SpotMarketInfo::liquidation_fee( ) const &
{
    return mango_int128_to_price( liquidationFee );
}

void MangoReferenceData::get_position_component( std::vector< Trading::Quantity > & positionComponent ) const &
{
    for ( size_t currencyIndex = 0; currencyIndex < currencies.size( ); ++currencyIndex )
    {
        const auto & mangoCurrency = currencies[ currencyIndex ];
        auto mangoCurrencyIndex = mangoCurrency.mangoCurrencyIndex;

        Trading::Quantity depositIndex = mango_int128_to_price( mangoCache.rootBankCaches[ mangoCurrencyIndex ].depositIndex );
        Trading::Quantity borrowIndex = mango_int128_to_price( mangoCache.rootBankCaches[ mangoCurrencyIndex ].borrowIndex );

        Trading::Quantity deposits = mango_int128_to_price( mangoAccount.deposits[ mangoCurrencyIndex ] );
        Trading::Quantity borrows = mango_int128_to_price( mangoAccount.borrows[ mangoCurrencyIndex ] );

        positionComponent[ currencyIndex ] = Serum::serum_to_synthfi_quantity
        (
            ( deposits * depositIndex ) - ( borrows * borrowIndex ),
            mangoGroup.tokens[ mangoCurrencyIndex ].decimals
        );
    }
}

void MangoReferenceData::get_health_components
(
    Trading::Quantity & quoteComponent,
    std::vector< Trading::Quantity > & spotComponent,
    std::vector< Trading::Price > & priceComponent,
    std::vector< Trading::Price > & weightComponent,
    const std::vector< Trading::Quantity > & positions
) const &
{
    quoteComponent = Trading::Quantity( 0 );

    for ( size_t currencyIndex = 0; currencyIndex < currencies.size( ); ++currencyIndex )
    {
        const auto & position = positions[ currencyIndex ];
        const auto mangoCurrencyIndex = currencies[ currencyIndex ].mangoCurrencyIndex;

        // Value of position is: ( scaled weight ) * ( cached oracle price ).
        if ( mangoCurrencyIndex < mangoGroup.numOracles )
        {
            // Mango trading pair index is mango currency index of the base currency.
            // TODO: search unneccesary.
            const auto & findTradingPairIndex = std::find_if
            (
                tradingPairs.begin( ),
                tradingPairs.end( ),
                [ mangoCurrencyIndex ]( const auto & mangoTradingPair )
                {
                    return mangoTradingPair.mangoBaseCurrencyIndex == mangoCurrencyIndex;
                }
            );

            const auto tradingPairIndex = std::distance( tradingPairs.begin( ), findTradingPairIndex );
            const auto mangoTradingPairIndex = tradingPairs[ tradingPairIndex ].mangoTradingPairIndex;

            BOOST_ASSERT_MSG( mangoTradingPairIndex == mangoCurrencyIndex, "Invalid mango trading pair index" );

            priceComponent[ tradingPairIndex ] = mango_int128_to_quantity
            (
                mangoCache.priceCaches[ mangoTradingPairIndex ].price
            );

            if ( mangoAccount.inMarginBasket[ mangoTradingPairIndex ] )
            {
                // Account for open orders.
                const auto & openOrdersAccount = tradingPairs[ tradingPairIndex ].openOrdersAccount;

                const auto decimals = mangoGroup.tokens[ mangoCurrencyIndex ].decimals;

                const auto quoteFree = Serum::serum_to_synthfi_quantity( openOrdersAccount.nativeQuoteFree + openOrdersAccount.referrerRebatesAccrued, decimals );
                const auto quoteLocked = Serum::serum_to_synthfi_quantity( openOrdersAccount.nativeQuoteTotal - openOrdersAccount.nativeQuoteFree, decimals );
                const auto baseFree = Serum::serum_to_synthfi_quantity( openOrdersAccount.nativeBaseFree, decimals );
                const auto baseLocked = Serum::serum_to_synthfi_quantity( openOrdersAccount.nativeBaseTotal - openOrdersAccount.nativeBaseFree, decimals );

                // Base total if all bids were executed.
                const auto bidsBasePosition = ( ( position + quoteLocked ) / priceComponent[ tradingPairIndex ] ) + baseFree + baseLocked;
                const auto asksBasePosition = position + baseFree;

                // Bids case worse if it has a higher absolute position.
                if ( boost::multiprecision::abs( bidsBasePosition ) > boost::multiprecision::abs( asksBasePosition ) )
                {
                    spotComponent[ tradingPairIndex ] = bidsBasePosition;
                    quoteComponent += quoteFree;
                }
                else
                {
                    spotComponent[ tradingPairIndex ] = asksBasePosition;
                    quoteComponent += ( baseLocked * priceComponent[ tradingPairIndex ] ) + quoteFree + quoteLocked;
                }
            }
            else
            {
                // Account for open orders.
                spotComponent[ tradingPairIndex ] = position;
            }

            weightComponent[ tradingPairIndex ] = ( spotComponent[ tradingPairIndex ] > 0 )
                ? mangoGroup.spotMarkets[ mangoTradingPairIndex ].initial_asset_weight( )
                : mangoGroup.spotMarkets[ mangoTradingPairIndex ].initial_liability_weight( );
        }
        // Quote currency is 1:1 with itself.
        else if ( mangoCurrencyIndex == mango_quote_currency_index( ) )
        {
            quoteComponent += position;
        }
    }
}

Trading::Quantity MangoReferenceData::get_health_from_components
(
    const Trading::Quantity & quoteComponent,
    const std::vector< Trading::Quantity > & spotComponent,
    const std::vector< Trading::Price > & priceComponent,
    const std::vector< Trading::Price > & weightComponent
) const &
{
    Trading::Quantity health = quoteComponent;

    for ( size_t currencyIndex = 0; currencyIndex < currencies.size( ); ++currencyIndex )
    {
        const auto mangoCurrencyIndex = currencies[ currencyIndex ].mangoCurrencyIndex;

        if ( mangoCurrencyIndex < mangoGroup.numOracles )
        {
            // Mango trading pair index is mango currency index of the base currency.
            // TODO: search unneccesary.
            const auto & findTradingPairIndex = std::find_if
            (
                tradingPairs.begin( ),
                tradingPairs.end( ),
                [ mangoCurrencyIndex ]( const auto & mangoTradingPair )
                {
                    return mangoTradingPair.mangoBaseCurrencyIndex == mangoCurrencyIndex;
                }
            );

            const auto tradingPairIndex = std::distance( tradingPairs.begin( ), findTradingPairIndex );

            health +=
                spotComponent[ tradingPairIndex ]
                * priceComponent[ tradingPairIndex ]
                * weightComponent[ tradingPairIndex ];
        }
    }

    return health; 
}

} // namespace Mango
} // namespace Synthfi

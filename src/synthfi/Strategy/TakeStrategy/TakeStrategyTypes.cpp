#include "synthfi/Strategy/TakeStrategy/TakeStrategyTypes.hpp"

#include "synthfi/Util/Logger.hpp"

namespace Synthfi
{
namespace Strategy
{

TakeStrategyOptionsConfig tag_invoke( json_to_tag< TakeStrategyOptionsConfig >, simdjson::ondemand::value jsonValue )
{
    auto maxUsdTradeSize = jsonValue[ "maxUsdTradeSize" ].value( );
    auto minUsdTradeProfit = jsonValue[ "minUsdTradeProfit" ].value( );

    return TakeStrategyOptionsConfig
    {
        .maxUsdTradeSize = json_to< Trading::Quantity >( maxUsdTradeSize ),
        .minUsdTradeProfit = json_to< Trading::Price >( minUsdTradeProfit )
    };
}

TakeStrategyConfig tag_invoke( json_to_tag< TakeStrategyConfig >, simdjson::ondemand::value jsonValue )
{
    TakeStrategyConfig config;

    config.keyStoreDirectory = std::string( jsonValue[ "keyStoreDirectory" ].get_string( ).value( ) );
    config.solanaMaxMultipleAccounts = jsonValue[ "solanaMaxMultipleAccounts" ].get_uint64( );

    config.solanaHttpEndpointConfig = json_to< Solana::SolanaEndpointConfig >( jsonValue[ "solanaHttpEndpointConfig" ].value( ) );
    config.solanaWsEndpointConfig = json_to< Solana::SolanaEndpointConfig >( jsonValue[ "solanaWsEndpointConfig" ].value( ) );
    config.ftxAuthenticationConfig = json_to< Ftx::FtxAuthenticationConfig >( jsonValue[ "ftxAuthenticationConfig" ].value( ) );
    config.statisticsPublisherConfig = json_to< Statistics::StatisticsPublisherConfig >( jsonValue[ "statisticsPublisherConfig" ].value( ) );
    config.takeStrategyOptionsConfig = json_to< TakeStrategyOptionsConfig >( jsonValue[ "takeStrategyOptionsConfig" ].value( ) );

    config.mangoAccountAddress.init_from_base58( jsonValue[ "mangoAccountAddress" ].get_string( ).value( ) );

    for ( simdjson::ondemand::value tradingPair : jsonValue[ "tradingPairs" ].get_array( ) )
    {
        config.tradingPairs.push_back( json_to< Trading::TradingPair >( tradingPair ) );
    }

    for ( simdjson::ondemand::value currency : jsonValue[ "currencies" ].get_array( ) )
    {
        config.currencies.push_back( json_to< Trading::Currency >( currency ) );
    }

    return config;
}

} // namespace Strategy
} // namespace Synthfi

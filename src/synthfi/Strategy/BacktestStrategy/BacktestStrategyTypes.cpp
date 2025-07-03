#include "synthfi/Strategy/BacktestStrategy/BacktestStrategyTypes.hpp"

#include "synthfi/Util/Logger.hpp"

namespace Synthfi
{
namespace Strategy
{

BacktestStrategyConfig tag_invoke( json_to_tag< BacktestStrategyConfig >, simdjson::ondemand::value jsonValue )
{
    BacktestStrategyConfig config;

    config.keyStoreDirectory = std::filesystem::path( jsonValue[ "keyStoreDirectory" ].get_string( ).value( ) );

    config.solanaMaxMultipleAccounts = jsonValue[ "solanaMaxMultipleAccounts" ].get_uint64( );
    config.solanaHttpEndpointConfig = json_to< Solana::SolanaEndpointConfig >( jsonValue[ "solanaHttpEndpointConfig" ].value( ) );
    config.solanaWsEndpointConfig = json_to< Solana::SolanaEndpointConfig >( jsonValue[ "solanaWsEndpointConfig" ].value( ) );
    config.ftxAuthenticationConfig = json_to< Ftx::FtxAuthenticationConfig >( jsonValue[ "ftxAuthenticationConfig" ].value( ) );
    config.statisticsPublisherConfig = json_to< Statistics::StatisticsPublisherConfig >( jsonValue[ "statisticsPublisherConfig" ].value( ) );

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


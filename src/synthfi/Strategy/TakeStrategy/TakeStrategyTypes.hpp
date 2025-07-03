#pragma once

#include "synthfi/Ftx/FtxTypes.hpp"
#include "synthfi/Solana/SolanaTypes.hpp"
#include "synthfi/Statistics/StatisticsTypes.hpp"

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Trading/Currency.hpp"
#include "synthfi/Trading/Quantity.hpp"
#include "synthfi/Trading/TradingPair.hpp"
#include "synthfi/Util/Utils.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace Synthfi
{
namespace Strategy
{

struct TakeStrategyOptionsConfig
{
    friend TakeStrategyOptionsConfig tag_invoke( json_to_tag< TakeStrategyOptionsConfig >, simdjson::ondemand::value jsonValue );

    Trading::Quantity maxUsdTradeSize;
    Trading::Price minUsdTradeProfit;
};

struct TakeStrategyConfig
{
    friend TakeStrategyConfig tag_invoke( json_to_tag< TakeStrategyConfig >, simdjson::ondemand::value jsonValue );

    std::filesystem::path keyStoreDirectory;

    uint64_t solanaMaxMultipleAccounts;

    Solana::SolanaEndpointConfig solanaHttpEndpointConfig;
    Solana::SolanaEndpointConfig solanaWsEndpointConfig;
    Ftx::FtxAuthenticationConfig ftxAuthenticationConfig;
    Statistics::StatisticsPublisherConfig statisticsPublisherConfig;
    TakeStrategyOptionsConfig takeStrategyOptionsConfig;

    Core::PublicKey mangoAccountAddress;

    std::vector< Trading::TradingPair > tradingPairs;
    std::vector< Trading::Currency > currencies;
};

} // namespace Synthfi

} // namespace Strategy

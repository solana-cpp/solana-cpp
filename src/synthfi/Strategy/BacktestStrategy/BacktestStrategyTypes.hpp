#pragma once

#include "synthfi/Ftx/FtxTypes.hpp"
#include "synthfi/Solana/SolanaTypes.hpp"
#include "synthfi/Statistics/StatisticsTypes.hpp"

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Trading/Currency.hpp"
#include "synthfi/Trading/TradingPair.hpp"
#include "synthfi/Util/Utils.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace Synthfi
{
namespace Strategy
{

struct BacktestStrategyConfig
{
    std::filesystem::path keyStoreDirectory;

    uint64_t solanaMaxMultipleAccounts;

    Solana::SolanaEndpointConfig solanaHttpEndpointConfig;
    Solana::SolanaEndpointConfig solanaWsEndpointConfig;

    Ftx::FtxAuthenticationConfig ftxAuthenticationConfig;
    Statistics::StatisticsPublisherConfig statisticsPublisherConfig;

    std::vector< Trading::TradingPair > tradingPairs;
    std::vector< Trading::Currency > currencies;

    friend BacktestStrategyConfig tag_invoke( json_to_tag< BacktestStrategyConfig >, simdjson::ondemand::value jsonValue );
};

} // namespace Synthfi

} // namespace Strategy

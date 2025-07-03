#pragma once

#include "synthfi/Serum/SerumReferenceDataClient/SerumReferenceDataClient.hpp"
#include "synthfi/Solana/AccountBatcher/AccountBatcher.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

#include "synthfi/Mango/MangoTypes.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace Synthfi
{
namespace Mango
{

class MangoReferenceDataClientImpl
{
public:
    MangoReferenceDataClientImpl
    (
        boost::asio::io_context & ioContext,
        Solana::AccountBatcherService & solanaAccountBatcherService,
        Serum::SerumReferenceDataClientService & serumReferenceDataClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        Core::PublicKey mangoAccountAddress
    );

    template< class CompletionHandlerType >
    void get_reference_data( CompletionHandlerType completionHandler )
    {
        co_spawn( _strand, do_get_reference_data( ), completionHandler );
    }

    constexpr std::string name( ) const & { return "MangoReferenceDataClient"; }

private:
    boost::asio::awaitable< void > do_load_reference_data( Core::PublicKey mangoMarketAddress );

    boost::asio::awaitable< MangoReferenceData > do_get_reference_data( );

    boost::asio::awaitable< void > do_publish_statistics( );

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;
    boost::asio::high_resolution_timer _expiryTimer;

    Solana::AccountBatcher _accountBatcher;
    Serum::SerumReferenceDataClient _serumReferenceDataClient;
    Statistics::StatisticsPublisher _statisticsPublisher;

    Serum::SerumReferenceData _serumReferenceData;

    bool _isInitialized = false;
    MangoReferenceData _mangoReferenceData;

    std::vector< Statistics::InfluxMeasurement > _currencyMeasurements;
    std::vector< Statistics::InfluxMeasurement > _tradingPairMeasurements;

    mutable SynthfiLogger _logger;
};

} // namespace Mango
} // namespace Synthfi

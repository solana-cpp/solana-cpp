#pragma once

#include "synthfi/Serum/SerumTypes.hpp"
#include "synthfi/Solana/AccountBatcher/AccountBatcher.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace Synthfi
{
namespace Serum
{

class SerumReferenceDataClientImpl
{
public:
    SerumReferenceDataClientImpl
    (
        boost::asio::io_context & ioContext,
        Solana::AccountBatcherService & solanaAccountBatcherService,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        std::vector< Core::PublicKey > serumMarketAddresses,
        std::vector< Core::PublicKey > mintAddresses
    );

    template< class CompletionHandlerType >
    void get_reference_data( CompletionHandlerType completionHandler )
    {
        co_spawn( _strand, do_get_reference_data( ), completionHandler );
    }

    constexpr std::string name( ) const & { return "SerumReferenceDataClient"; }

private:
    boost::asio::awaitable< void > do_load_reference_data
    (
        std::vector< Core::PublicKey > serumMarketAddresses,
        std::vector< Core::PublicKey > mintAddresses
    );

    boost::asio::awaitable< SerumReferenceData > do_get_reference_data( );

    boost::asio::awaitable< void > do_publish_statistics( );

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;
    boost::asio::high_resolution_timer _expiryTimer;
    Solana::AccountBatcher _accountBatcher;

    Statistics::StatisticsPublisher _statisticsPublisher;

    std::vector< Statistics::InfluxMeasurement > _currencyMeasurements;
    std::vector< Statistics::InfluxMeasurement > _tradingPairMeasurements;

    bool _isInitialized = false;
    SerumReferenceData _referenceData;

    mutable SynthfiLogger _logger;
};

} // namespace Serum
} // namespace Synthfi

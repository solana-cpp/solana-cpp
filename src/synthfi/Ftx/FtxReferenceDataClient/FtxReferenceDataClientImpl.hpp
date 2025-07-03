#pragma once

#include "synthfi/Ftx/FtxTypes.hpp"
#include "synthfi/Ftx/FtxRestClient/FtxRestClient.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace Synthfi
{
namespace Ftx
{

class FtxReferenceDataClientImpl
{
public:
    FtxReferenceDataClientImpl
    (
        boost::asio::io_context & ioContext,
        FtxRestClientService & ftxRestClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        std::vector< std::string > ftxMarketNames,
        std::vector< std::string > ftxCurrencyNames
    );

    template< class CompletionHandlerType >
    void get_reference_data( CompletionHandlerType completionHandler )
    {
        co_spawn( _strand, do_get_reference_data( ), completionHandler );
    }

    constexpr std::string name( ) const & { return "FtxReferenceDataClient"; }

private:
    boost::asio::awaitable< FtxReferenceData > do_get_reference_data( );
    boost::asio::awaitable< void > do_load_reference_data
    (
        std::vector< std::string > ftxMarketNames,
        std::vector< std::string > ftxCurrencyNames
    );

    boost::asio::awaitable< void > do_publish_statistics( );

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;
    boost::asio::high_resolution_timer _expiryTimer;

    FtxRestClient _ftxRestClient;
    Statistics::StatisticsPublisher _statisticsPublisher;

    bool _isInitialized = false;
    FtxReferenceData _referenceData;

    std::vector< Statistics::InfluxMeasurement > _currencyMeasurements;
    std::vector< Statistics::InfluxMeasurement > _tradingPairMeasurements;

    mutable SynthfiLogger _logger;
};

} // namespace Ftx
} // namespace Synthfi

#pragma once

#include "synthfi/Ftx/FtxTypes.hpp"
#include "synthfi/Trading/Order.hpp"
#include "synthfi/Trading/Wallet.hpp"

#include "synthfi/Ftx/FtxRestClient/FtxRestClient.hpp"
#include "synthfi/Ftx/FtxReferenceDataClient/FtxReferenceDataClient.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/high_resolution_timer.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>
#include <functional>
#include <string>

namespace Synthfi
{
namespace Ftx
{

class FtxWalletClientImpl
{
public:
    FtxWalletClientImpl
    (
        boost::asio::io_context & ioContext,
        Ftx::FtxRestClientService & ftxRestClientService,
        Ftx::FtxReferenceDataClientService & ftxReferenceDataClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService
    );

    template< class CompletionHandlerType >
    void subscribe_wallet
    (
        std::function< void( Trading::Wallet ) > onWallet,
        CompletionHandlerType completionHandler
    )
    {
        co_spawn( _strand, do_subscribe_wallet( std::move( onWallet ) ), completionHandler );
    }

    constexpr std::string name( ) const & { return "FtxWalletClient"; }

private:
    boost::asio::awaitable< void > do_subscribe_wallet( std::function< void( Trading::Wallet ) > onWallet );

    boost::asio::awaitable< void > do_update_positions( );

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;

    Ftx::FtxRestClient _ftxRestClient;
    Ftx::FtxReferenceDataClient _ftxReferenceDataClient;
    Statistics::StatisticsPublisher _statisticsPublisher;

    boost::asio::high_resolution_timer _updatePositionsTimer;

    Ftx::FtxReferenceData _referenceData;

    uint64_t _quoteCurrencyIndex = std::numeric_limits< uint64_t >::max( );
    Trading::Wallet _wallet;

    std::vector< Statistics::InfluxMeasurement > _positionMeasurements;
    std::vector< Statistics::InfluxMeasurement > _marginAvailableMeasurements;

    std::function< void( Trading::Wallet ) > _onWallet;

    mutable SynthfiLogger _logger;
};

} // namespace Ftx
} // namespace Synthfi

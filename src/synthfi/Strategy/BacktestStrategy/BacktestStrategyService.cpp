#include "synthfi/Strategy/BacktestStrategy/BacktestStrategyService.hpp"

#include "synthfi/Ftx/FtxMarketDataClient/FtxMarketDataClientService.hpp"
#include "synthfi/Ftx/FtxOrderClient/FtxOrderClientService.hpp"
#include "synthfi/Ftx/FtxRestClient/FtxRestClientService.hpp"
#include "synthfi/Ftx/FtxWsClient/FtxWsClientService.hpp"

#include "synthfi/Solana/AccountSubscriber/AccountSubscriberService.hpp"
#include "synthfi/Solana/SignatureSubscriber/SignatureSubscriberService.hpp"
#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClientService.hpp"
#include "synthfi/Solana/SolanaWsClient/SolanaWsClientService.hpp"
namespace Synthfi
{
namespace Strategy
{

BacktestStrategyService::BacktestStrategyService( boost::asio::execution_context & executionContext, BacktestStrategyConfig config )
    : boost::asio::execution_context::service( executionContext )
    , _ioContext( )
    , _work
    (
        boost::asio::require( _ioContext.get_executor( ),
        boost::asio::execution::outstanding_work.tracked )
    )
    , _workThread( [ this ]( ){ return this->_ioContext.run( ); } )
{
    // Core services.
    auto & keyStoreService = boost::asio::make_service< Core::KeyStoreService >( _ioContext, config.keyStoreDirectory );
    auto & statisticsPublisherService = boost::asio::make_service< Statistics::StatisticsPublisherService >( _ioContext, config.statisticsPublisherConfig );

    // Make required Solana services.
    auto & solanaHttpClientService = boost::asio::make_service< Solana::SolanaHttpClientService >
    (
        _ioContext,
        statisticsPublisherService,
        config.solanaHttpEndpointConfig
   );
    auto & solanaWsClientService = boost::asio::make_service< Solana::SolanaWsClientService >
    (
        _ioContext,
        statisticsPublisherService,
        config.solanaWsEndpointConfig
   );
    auto & solanaAccountBatcherService = boost::asio::make_service< Solana::AccountBatcherService >( _ioContext, solanaHttpClientService, config.solanaMaxMultipleAccounts );
    auto & solanaAccountSubscriberService = asio::make_service< Solana::AccountSubscriberService >( _ioContext, solanaWsClientService );
    auto & solanaSignatureSubscriberService = boost::asio::make_service< Solana::SignatureSubscriberService >( _ioContext, solanaWsClientService );

    std::vector< std::string > ftxMarketNames;
    std::vector< Core::PublicKey > serumMarketAddresses;
    for ( const auto & tradingPair : config.tradingPairs )
    {
        ftxMarketNames.emplace_back( tradingPair.ftxMarketName );
        serumMarketAddresses.emplace_back( tradingPair.serumMarketAddress );
    }

    std::vector< std::string > ftxCurrencyNames;
    std::vector< Core::PublicKey > mintAddresses;
    for ( const auto & currency : config.currencies )
    {
        ftxCurrencyNames.emplace_back( currency.currencyName );
        mintAddresses.emplace_back( currency.mintAddress );
    }

    auto & serumReferenceDataClientService = boost::asio::make_service< Serum::SerumReferenceDataClientService >
    (
        _ioContext,
        solanaAccountBatcherService,
        statisticsPublisherService,
        std::move( serumMarketAddresses ),
        std::move( mintAddresses )
    );

    auto & serumMarketDataClientService = boost::asio::make_service< Serum::SerumMarketDataClientService >
    (
        _ioContext,
        solanaAccountBatcherService,
        solanaAccountSubscriberService,
        serumReferenceDataClientService,
        statisticsPublisherService
    );

    // Make required Ftx services.
    auto & ftxRestClientService = boost::asio::make_service< Ftx::FtxRestClientService >
    (
        _ioContext,
        statisticsPublisherService,
        config.ftxAuthenticationConfig
    );
    auto & ftxWsClientService = boost::asio::make_service< Ftx::FtxWsClientService >
    (
        _ioContext,
        statisticsPublisherService,
        config.ftxAuthenticationConfig.host
    );
    auto & ftxReferenceDataClientService = boost::asio::make_service< Ftx::FtxReferenceDataClientService >
    (
        _ioContext,
        ftxRestClientService,
        statisticsPublisherService,
        std::move( ftxMarketNames ),
        std::move( ftxCurrencyNames )
    );
    auto & ftxMarketDataClientService = boost::asio::make_service< Ftx::FtxMarketDataClientService >
    (
        _ioContext,
        ftxReferenceDataClientService,
        ftxWsClientService,
        statisticsPublisherService
    );

    _impl = std::make_unique< BacktestStrategyImpl >
    (
        _ioContext,
        keyStoreService,
        serumReferenceDataClientService,
        serumMarketDataClientService,
        ftxReferenceDataClientService,
        ftxMarketDataClientService,
        statisticsPublisherService,
        config
    );
}

} // namespace Strategy
} // namespace Synthfi

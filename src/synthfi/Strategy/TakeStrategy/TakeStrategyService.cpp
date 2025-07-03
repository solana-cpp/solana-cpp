#include "synthfi/Strategy/TakeStrategy/TakeStrategyService.hpp"

#include "synthfi/Ftx/FtxOrderClient/FtxOrderClientService.hpp"
#include "synthfi/Ftx/FtxRestClient/FtxRestClientService.hpp"
#include "synthfi/Ftx/FtxWsClient/FtxWsClientService.hpp"
#include "synthfi/Ftx/FtxWalletClient/FtxWalletClientService.hpp"

#include "synthfi/Mango/MangoOrderClient/MangoOrderClientService.hpp"
#include "synthfi/Mango/MangoWalletClient/MangoWalletClientService.hpp"

#include "synthfi/Solana/AccountSubscriber/AccountSubscriberService.hpp"
#include "synthfi/Solana/SignatureSubscriber/SignatureSubscriberService.hpp"
#include "synthfi/Solana/SlotSubscriber/SlotSubscriberService.hpp"
#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClientService.hpp"
#include "synthfi/Solana/SolanaWsClient/SolanaWsClientService.hpp"

#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherService.hpp"

namespace asio = boost::asio;

namespace Synthfi
{
namespace Strategy
{

TakeStrategyService::TakeStrategyService( asio::execution_context & executionContext, TakeStrategyConfig config )
    : asio::execution_context::service( executionContext )
    , _ioContext( )
    , _work( asio::require( _ioContext.get_executor( ), asio::execution::outstanding_work.tracked ) )
    , _workThread( [ this ]( ){ return this->_ioContext.run( ); } )
{
    // core services.
    auto & keyStoreService = asio::make_service< Core::KeyStoreService >( _ioContext, config.keyStoreDirectory );
    auto & statisticsPublisherService = asio::make_service< Statistics::StatisticsPublisherService >( _ioContext, config.statisticsPublisherConfig );

    // Make required Solana services.
    auto & solanaHttpClientService = asio::make_service< Solana::SolanaHttpClientService >
    (
        _ioContext,
        statisticsPublisherService,
        config.solanaHttpEndpointConfig
    );
    auto & solanaWsClientService = asio::make_service< Solana::SolanaWsClientService >
    (
        _ioContext,
        statisticsPublisherService,
        config.solanaWsEndpointConfig
    );

    auto & solanaAccountBatcherService = asio::make_service< Solana::AccountBatcherService >( _ioContext, solanaHttpClientService, config.solanaMaxMultipleAccounts );
    auto & solanaAccountSubscriberService = asio::make_service< Solana::AccountSubscriberService >( _ioContext, solanaWsClientService );
    auto & solanaSignatureSubscriberService = asio::make_service< Solana::SignatureSubscriberService >( _ioContext, solanaWsClientService );
    auto & solanaSlotSubscriberService = asio::make_service< Solana::SlotSubscriberService >( _ioContext, solanaHttpClientService, solanaWsClientService, statisticsPublisherService );

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

    auto & serumReferenceDataClientService = asio::make_service< Serum::SerumReferenceDataClientService >
    (
        _ioContext,
        solanaAccountBatcherService,
        statisticsPublisherService,
        std::move( serumMarketAddresses ),
        std::move( mintAddresses )
    );
    auto & serumMarketDataClientService = asio::make_service< Serum::SerumMarketDataClientService >
    (
        _ioContext,
        solanaAccountBatcherService,
        solanaAccountSubscriberService,
        serumReferenceDataClientService,
        statisticsPublisherService
    );

    auto & mangoReferenceDataClientService = asio::make_service< Mango::MangoReferenceDataClientService >
    (
        _ioContext,
        solanaAccountBatcherService,
        serumReferenceDataClientService,
        statisticsPublisherService,
        config.mangoAccountAddress
    );

    auto & mangoOrderClientService = asio::make_service< Mango::MangoOrderClientService >
    (
        _ioContext,
        keyStoreService,
        solanaHttpClientService,
        solanaAccountSubscriberService,
        solanaSignatureSubscriberService,
        solanaSlotSubscriberService,
        serumReferenceDataClientService,
        mangoReferenceDataClientService,
        statisticsPublisherService
    );

    auto & mangoWalletClientService = asio::make_service< Mango::MangoWalletClientService >
    (
        _ioContext,
        keyStoreService,
        solanaHttpClientService,
        solanaAccountSubscriberService,
        solanaSignatureSubscriberService,
        serumReferenceDataClientService,
        mangoReferenceDataClientService,
        statisticsPublisherService
    );

    // Make required Ftx services.
    auto & ftxRestClientService = asio::make_service< Ftx::FtxRestClientService >
    (
        _ioContext,
        statisticsPublisherService,
        config.ftxAuthenticationConfig
    );
    auto & ftxWsClientService = asio::make_service< Ftx::FtxWsClientService >
    (
        _ioContext,
        statisticsPublisherService,
        config.ftxAuthenticationConfig.host
    );

    auto & ftxReferenceDataClientService = asio::make_service< Ftx::FtxReferenceDataClientService >
    (
        _ioContext,
        ftxRestClientService,
        statisticsPublisherService,
        std::move( ftxMarketNames ),
        std::move( ftxCurrencyNames )
    );

    auto & ftxOrderClientService = asio::make_service< Ftx::FtxOrderClientService >
    (
        _ioContext,
        ftxRestClientService,
        ftxReferenceDataClientService,
        statisticsPublisherService,
        config.ftxAuthenticationConfig
    );
    auto & ftxMarketDataClientService = asio::make_service< Ftx::FtxMarketDataClientService >
    (
        _ioContext,
        ftxReferenceDataClientService,
        ftxWsClientService,
        statisticsPublisherService
    );

    auto & ftxWalletClientService = asio::make_service< Ftx::FtxWalletClientService >
    (
        _ioContext,
        ftxRestClientService,
        ftxReferenceDataClientService,
        statisticsPublisherService
    );

    _impl = std::make_unique< TakeStrategyImpl >
    (
        _ioContext,
        ftxReferenceDataClientService,
        serumReferenceDataClientService,
        ftxMarketDataClientService,
        serumMarketDataClientService,
        ftxWalletClientService,
        mangoWalletClientService,
        ftxOrderClientService,
        mangoOrderClientService,
        statisticsPublisherService,
        config.takeStrategyOptionsConfig
    );
}


} // namespace Strategy

} // namespace Synthfi

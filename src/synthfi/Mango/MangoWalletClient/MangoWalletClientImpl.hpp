#pragma once

#include "synthfi/Trading/Wallet.hpp"
#include "synthfi/Mango/MangoTypes.hpp"

#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClient.hpp"
#include "synthfi/Solana/AccountSubscriber/AccountSubscriber.hpp"
#include "synthfi/Solana/SignatureSubscriber/SignatureSubscriber.hpp"
#include "synthfi/Serum/SerumReferenceDataClient/SerumReferenceDataClient.hpp"
#include "synthfi/Mango/MangoReferenceDataClient/MangoReferenceDataClient.hpp"
#include "synthfi/Core/KeyStore/KeyStore.hpp"

#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

#include "synthfi/Trading/Order.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>

#include <chrono>
#include <functional>
#include <optional>
#include <string>

namespace Synthfi
{
namespace Mango
{

class MangoWalletClientImpl
{
public:
    MangoWalletClientImpl
    (
        boost::asio::io_context & ioContext,
        Core::KeyStoreService & keyStoreService,
        Solana::SolanaHttpClientService & solanaHttpClientService,
        Solana::AccountSubscriberService & accountSubscriberService,
        Solana::SignatureSubscriberService & signatureSubscriberService,
        Serum::SerumReferenceDataClientService & serumReferenceDataClientService,
        MangoReferenceDataClientService & mangoReferenceDataClientService,
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

    constexpr std::string name( ) const & { return "MangoWalletClient"; }

private:
    boost::asio::awaitable< void > do_subscribe_wallet( std::function< void( Trading::Wallet ) > onWallet );

    boost::asio::awaitable< void > do_update_positions( );

    // On mango account update, update mango wallet.
    void on_mango_account_notification( uint64_t subscriptionId, Solana::AccountNotification accountNotification );
    boost::asio::awaitable< void > do_on_mango_account_notification
    (
        uint64_t subscriptionId, 
        Solana::AccountNotification accountNotification
    );

    // Mango cache determines available margin, deposit and borrows.
    void on_mango_cache_account_notification( uint64_t subscriptionId, Solana::AccountNotification accountNotification );
    boost::asio::awaitable< void > do_on_mango_cache_notification
    (
        uint64_t subscriptionId, 
        Solana::AccountNotification accountNotification
    );

    // Open orders consume available margin.
    void on_open_orders_account_notification
    (
        uint64_t subscriptionId,
        Solana::AccountNotification accountNotification,
        uint64_t tradingPairIndex
    );
    boost::asio::awaitable< void > do_on_open_orders_account_notification
    (
        uint64_t subscriptionId,
        Solana::AccountNotification accountNotification,
        uint64_t tradingPairIndex
    );

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;

    Core::KeyStore _keyStore;

    Solana::SolanaHttpClient _solanaHttpClient;
    Solana::AccountSubscriber _accountSubscriber;
    Solana::SignatureSubscriber _signatureSubscriber;

    Serum::SerumReferenceDataClient _serumReferenceDataClient;
    MangoReferenceDataClient _mangoReferenceDataClient;

    std::vector< Serum::SerumOpenOrdersAccount > _openOrdersAccounts;

    Statistics::StatisticsPublisher _statisticsPublisher;

    Serum::SerumReferenceData _serumReferenceData;
    MangoReferenceData _mangoReferenceData;

    Trading::Wallet _wallet;
    std::vector< Statistics::InfluxMeasurement > _positionMeasurements;
    std::vector< Statistics::InfluxMeasurement > _marginAvailableMeasurements;

    std::function< void( Trading::Wallet ) > _onWallet;

    mutable SynthfiLogger _logger;
};

} // namespace Mango
} // namespace Synthfi

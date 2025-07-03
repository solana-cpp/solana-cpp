#pragma once

#include "synthfi/Mango/MangoTypes.hpp"

#include "synthfi/Mango/MangoReferenceDataClient/MangoReferenceDataClient.hpp"
#include "synthfi/Serum/SerumReferenceDataClient/SerumReferenceDataClient.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisher.hpp"

#include "synthfi/Solana/AccountSubscriber/AccountSubscriber.hpp"
#include "synthfi/Solana/SignatureSubscriber/SignatureSubscriber.hpp"
#include "synthfi/Solana/SlotSubscriber/SlotSubscriber.hpp"
#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClient.hpp"

#include "synthfi/Core/KeyStore/KeyStore.hpp"

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

class MangoOrderClientImpl
{
public:
    MangoOrderClientImpl
    (
        boost::asio::io_context & ioContext,
        Core::KeyStoreService & keyStoreService,
        Solana::SolanaHttpClientService & solanaHttpClientService,
        Solana::AccountSubscriberService & accountSubscriberService,
        Solana::SignatureSubscriberService & signatureSubscriberService,
        Solana::SlotSubscriberService & slotSubscriberService,
        Serum::SerumReferenceDataClientService & serumReferenceDataClientService,
        Mango::MangoReferenceDataClientService & mangoReferenceDataClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService
    );

    template< class CompletionHandlerType >
    void load_mango_account( CompletionHandlerType completionHandler )
    {
        co_spawn( _strand, do_load_mango_account( ), completionHandler );
    }

    template< class CompletionHandlerType >
    void send_order( Trading::Order order, CompletionHandlerType completionHandler )
    {
        co_spawn
        (
            _strand,
            do_send_order( std::move( order ) ),
            completionHandler
        );
    }

    template< class CompletionHandlerType >
    void cancel_order( Trading::Order order, CompletionHandlerType completionHandler )
    {
        co_spawn
        (
            _strand,
            do_cancel_order( std::move( order ) ),
            completionHandler
        );
    }

    constexpr std::string name( ) const & { return "MangoOrderClient"; }

private:
    // On mango account update, update reference data.
    void on_mango_account_notification( uint64_t subscriptionId, Solana::AccountNotification accountNotification );
    boost::asio::awaitable< void > do_on_mango_account_notification
    (
        uint64_t subscriptionId, 
        Solana::AccountNotification accountNotification
    );

    // Solana transaction require a recent blockhash.
    void on_recent_blockhash( Core::Hash recentBlockhash );
    boost::asio::awaitable< void > do_on_recent_blockhash( Core::Hash recentBlockhash );

    boost::asio::awaitable< void > do_load_mango_account( );

    boost::asio::awaitable< Trading::Order > do_send_order( Trading::Order order );
    boost::asio::awaitable< Trading::Order > do_cancel_order( Trading::Order order );

    boost::asio::awaitable< void > publish_order_statistic( Trading::Order order );

    Core::PublicKeyRef _splTokenProgram = nullptr;
    Core::PublicKeyRef _sysvarRent = nullptr;
    Core::KeyPairRef _mangoOwnerKey = nullptr;

    uint64_t _mangoOwnerLamports;

    Core::Hash _recentBlockhash;

    Mango::MangoAccount _mangoAccount;

    std::chrono::high_resolution_clock _clock;

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;

    Core::KeyStore _keyStore;

    Solana::SolanaHttpClient _solanaHttpClient;
    Solana::AccountSubscriber _accountSubscriber;
    Solana::SignatureSubscriber _signatureSubscriber;
    Solana::SlotSubscriber _slotSubscriber;

    Serum::SerumReferenceDataClient _serumReferenceDataClient;
    Mango::MangoReferenceDataClient _mangoReferenceDataClient;
    Statistics::StatisticsPublisher _statisticsPublisher;

    Serum::SerumReferenceData _serumReferenceData;
    Mango::MangoReferenceData _mangoReferenceData;

    mutable SynthfiLogger _logger;
};

} // namespace Mango
} // namespace Synthfi

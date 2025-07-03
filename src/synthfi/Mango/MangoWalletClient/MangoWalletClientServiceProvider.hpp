#pragma once

#include "synthfi/Trading/Wallet.hpp"

#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClientService.hpp"
#include "synthfi/Solana/AccountSubscriber/AccountSubscriberService.hpp"
#include "synthfi/Solana/SignatureSubscriber/SignatureSubscriberService.hpp"
#include "synthfi/Serum/SerumReferenceDataClient/SerumReferenceDataClientService.hpp"
#include "synthfi/Core/KeyStore/KeyStore.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherService.hpp"

#include "synthfi/Trading/Side.hpp"
#include "synthfi/Trading/Order.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

namespace Synthfi
{
namespace Mango
{

template< class Service >
class MangoWalletClientServiceProvider
{
public:
    explicit MangoWalletClientServiceProvider( Service & service )
        : _service( &service )
    { }

    MangoWalletClientServiceProvider
    (
        boost::asio::io_context & ioContext,
        Core::KeyStoreService & keyStoreService,
        Solana::SolanaHttpClientService & solanaHttpClientService,
        Solana::AccountSubscriberService & accountSubscriberService,
        Solana::SignatureSubscriberService & signatureSubscriberService,
        Serum::SerumReferenceDataClientService & serumReferenceDataClientService,
        MangoReferenceDataClientService & mangoReferenceDataClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService
    )
        : _service
        (
            &boost::asio::make_service< Service >
            (
                ioContext,
                keyStoreService,
                solanaHttpClientService,
                accountSubscriberService,
                signatureSubscriberService,
                serumReferenceDataClientService,
                mangoReferenceDataClientService,
                statisticsPublisherService
            )
        )
    { }

    template< boost::asio::completion_token_for< void( std::exception_ptr ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto subscribe_wallet
    (
        std::function< void( Trading::Wallet ) > onWallet,
        CompletionToken && token = { }
    )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr ) >
        (
            [ this, onWallet = std::move( onWallet ) ]< class Handler >( Handler && self )
            {
                _service->impl( ).subscribe_wallet
                (
                    std::move( onWallet ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex )
                    {
                        ( *self )( ex );
                    }
                );
            },
            token
        );
    }

private:
    Service * _service;
};

} // namespace Mango
} // namespace Synthfi

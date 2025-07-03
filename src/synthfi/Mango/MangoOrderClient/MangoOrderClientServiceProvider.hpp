#pragma once

#include "synthfi/Mango/MangoTypes.hpp"

#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClientService.hpp"
#include "synthfi/Solana/AccountSubscriber/AccountSubscriberService.hpp"
#include "synthfi/Solana/SignatureSubscriber/SignatureSubscriberService.hpp"
#include "synthfi/Solana/SlotSubscriber/SlotSubscriberService.hpp"
#include "synthfi/Serum/SerumReferenceDataClient/SerumReferenceDataClientService.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherService.hpp"
#include "synthfi/Core/KeyStore/KeyStore.hpp"

#include "synthfi/Trading/Side.hpp"
#include "synthfi/Trading/Order.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

namespace Synthfi
{
namespace Mango
{

template< class Service >
class MangoOrderClientServiceProvider
{
public:
    explicit MangoOrderClientServiceProvider( Service & service )
        : _service( &service )
    { }

    MangoOrderClientServiceProvider
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
                slotSubscriberService,
                serumReferenceDataClientService,
                mangoReferenceDataClientService,
                statisticsPublisherService
            )
        )
    { }

    template< boost::asio::completion_token_for< void( std::exception_ptr ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto load_mango_account( CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr ) >
        (
            [ this ]< class Handler >( Handler && self )
            {
                _service->impl( ).load_mango_account
                (
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex )
                    {
                        ( *self )( ex );
                    }
                );
            },
            token
        );
    }

    template< boost::asio::completion_token_for< void( std::exception_ptr, Trading::Order ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto send_order( Trading::Order order, CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, Trading::Order ) >
        (
            [ this, order = std::move( order ) ]
            < class Handler >( Handler && self )
            {
                _service->impl( ).send_order
                (
                    std::move( order ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]
                    ( std::exception_ptr ex, Trading::Order result )
                    {
                        ( *self )( ex, std::move( result ) );
                    }
                );
            },
            token
        );
    }

    template< boost::asio::completion_token_for< void( std::exception_ptr, Trading::Order ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto cancel_order
    (
        Trading::Order order,
        CompletionToken && token = { }
    )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, Trading::Order ) >
        (
            [ this, order ]
            < class Handler >( Handler && self )
            {
                _service->impl( ).cancel_order
                (
                    std::move( order ),
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex, Trading::Order result )
                    {
                        ( *self )( ex, std::move( result ) );
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

#pragma once

#include "synthfi/Trading/Wallet.hpp"

#include "synthfi/Ftx/FtxRestClient/FtxRestClientService.hpp"
#include "synthfi/Ftx/FtxReferenceDataClient/FtxReferenceDataClientService.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherService.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

namespace Synthfi
{
namespace Ftx
{

template< class Service >
class FtxWalletClientServiceProvider
{
public:
    explicit FtxWalletClientServiceProvider( Service & service )
        : _service( &service )
    { }

    FtxWalletClientServiceProvider
    (
        boost::asio::io_context & ioContext,
        Ftx::FtxRestClientService & ftxRestClientService,
        Ftx::FtxReferenceDataClientService & ftxReferenceDataClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService
    )
        : _service
        (
            &boost::asio::make_service< Service >
            (
                ioContext,
                ftxRestClientService,
                ftxReferenceDataClientService,
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

} // namespace Ftx
} // namespace Synthfi

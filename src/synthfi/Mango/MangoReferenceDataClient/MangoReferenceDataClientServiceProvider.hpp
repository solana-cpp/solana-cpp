#pragma once

#include "synthfi/Mango/MangoTypes.hpp"
#include "synthfi/Solana/AccountBatcher/AccountBatcherService.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherService.hpp"

namespace Synthfi
{
namespace Mango
{

template< class Service >
class MangoReferenceDataClientServiceProvider
{
public:
    explicit MangoReferenceDataClientServiceProvider( Service & service )
        : _service( &service )
    { }

    MangoReferenceDataClientServiceProvider
    (
        boost::asio::io_context & ioContext,
        Solana::AccountBatcherService & solanaAccountBatcherService,
        Serum::SerumReferenceDataClientService & serumReferenceDataClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        Core::PublicKey mangoAccountAddress
    )
        : _service
        (
            &boost::asio::make_service< Service >
            (
                ioContext,
                solanaAccountBatcherService,
                serumReferenceDataClientService,
                statisticsPublisherService,
                mangoAccountAddress
            )
        )
    { }

    template< boost::asio::completion_token_for< void( std::exception_ptr, MangoReferenceData ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto get_reference_data( CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, MangoReferenceData ) >
        (
            [ this ]< class Handler >( Handler && self )
            {
                _service->impl( ).get_reference_data
                (
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex, MangoReferenceData result )
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

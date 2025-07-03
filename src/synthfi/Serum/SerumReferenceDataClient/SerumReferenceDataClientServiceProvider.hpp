#pragma once

#include "synthfi/Serum/SerumTypes.hpp"
#include "synthfi/Solana/AccountBatcher/AccountBatcherService.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherService.hpp"

namespace Synthfi
{
namespace Serum
{

template< class Service >
class SerumReferenceDataClientServiceProvider
{
public:
    explicit SerumReferenceDataClientServiceProvider( Service & service )
        : _service( &service )
    { }

    SerumReferenceDataClientServiceProvider
    (
        boost::asio::io_context & ioContext,
        Solana::AccountBatcherService & solanaAccountBatcherService,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        std::vector< Core::PublicKey > serumAccountAddresses,
        std::vector< Core::PublicKey > mintAddresses
    )
        : _service
        (
            &boost::asio::make_service< Service >
            (
                ioContext,
                solanaAccountBatcherService,
                statisticsPublisherService,
                std::move( serumAccountAddresses ),
                std::move( mintAddresses )
            )
        )
    { }

    template< boost::asio::completion_token_for< void( std::exception_ptr, SerumReferenceData ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto get_reference_data( CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, SerumReferenceData ) >
        (
            [ this ]< class Handler >( Handler && self )
            {
                _service->impl( ).get_reference_data
                (
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex, SerumReferenceData result )
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

} // namespace Serum
} // namespace Synthfi

#pragma once

#include "synthfi/Ftx/FtxRestClient/FtxRestClientService.hpp"
#include "synthfi/Statistics/StatisticsPublisher/StatisticsPublisherService.hpp"

#include "synthfi/Ftx/FtxTypes.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

#include <boost/json/value.hpp>

namespace Synthfi
{
namespace Ftx
{

template< class Service >
class FtxReferenceDataClientServiceProvider
{
public:
    explicit FtxReferenceDataClientServiceProvider( Service & service )
        : _service( &service )
    { }

    FtxReferenceDataClientServiceProvider
    (
        boost::asio::io_context & ioContext,
        FtxRestClientService & ftxRestClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        std::vector< std::string > ftxMarketNames,
        std::vector< std::string > ftxCurrencyNames
    )
        : _service
        (
            &boost::asio::make_service< Service >
            (
                ioContext,
                ftxRestClientService,
                statisticsPublisherService,
                std::move( ftxMarketNames ),
                std::move( ftxCurrencyNames )
            )
        )
    { }

    template< boost::asio::completion_token_for< void( std::exception_ptr, FtxReferenceData ) > CompletionToken = boost::asio::use_awaitable_t< > >
    auto get_reference_data( CompletionToken && token = { } )
    {
        return boost::asio::async_initiate< CompletionToken, void( std::exception_ptr, FtxReferenceData ) >
        (
            [ this ]< class Handler >( Handler && self )
            {
                _service->impl( ).get_reference_data
                (
                    [ self = std::make_shared< Handler >( std::forward< Handler >( self ) ) ]( std::exception_ptr ex, FtxReferenceData result )
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

} // namespace Ftx
} // namespace Synthfi

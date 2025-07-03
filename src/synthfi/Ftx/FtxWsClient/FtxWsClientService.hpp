#pragma once

#include "synthfi/Ftx/FtxWsClient/FtxWsClientServiceImpl.hpp"

#include <boost/asio/execution_context.hpp>
#include <boost/asio/io_context.hpp>

#include <thread>

namespace Synthfi
{
namespace Ftx
{

class FtxWsClientService : public boost::asio::execution_context::service
{
public:
    using OnOrderbookMessageFn = FtxWsClientServiceImpl::OnOrderbookMessageFn;

    // Constructor creates a thread to run a private io_service.
    FtxWsClientService
    (
        boost::asio::execution_context & executionContext,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        std::string_view host
    )
        : boost::asio::execution_context::service( executionContext )
        , _work( boost::asio::require( _ioContext.get_executor( ),
                 boost::asio::execution::outstanding_work.tracked ) )
        , _workThread( [ this ]( ){ return this->_ioContext.run( ); } )
        , _impl( _ioContext, statisticsPublisherService, host )
    { }

    ~FtxWsClientService( )
    {
        // Indicate that we have finished with the private io_context.
        // io_context::run( ) function will exit once all other work has completed.
        _work = boost::asio::any_io_executor( );
    }

    FtxWsClientServiceImpl & impl( ) { return _impl; }

    static inline boost::asio::execution_context::id id;

private:
    // Destroy all user-defined handler objects owned by the service.
    void shutdown( ) noexcept override { }

    // Private io_context used for performing logging operations.
    boost::asio::io_context _ioContext;
    // A work-tracking executor giving work for the private io_context to perform.
    boost::asio::any_io_executor _work;
    std::jthread _workThread;

    FtxWsClientServiceImpl _impl;
};

} // namespace Ftx
} // namespace Synthfi

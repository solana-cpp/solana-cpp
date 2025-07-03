#pragma once

#include "synthfi/Ftx/FtxOrderClient/FtxOrderClientImpl.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/execution_context.hpp>
#include <boost/asio/io_context.hpp>

#include <thread>

namespace Synthfi
{
namespace Ftx
{

class FtxOrderClientService : public boost::asio::execution_context::service
{
public:
    // Constructor creates a thread to run a private io_service.
    FtxOrderClientService
    (
        boost::asio::execution_context & executionContext,
        FtxRestClientService & ftxRestClientService,
        FtxReferenceDataClientService & ftxReferenceDataClientService,
        Statistics::StatisticsPublisherService & statisticsPublisherService,
        const FtxAuthenticationConfig & config
    )
        : boost::asio::execution_context::service( executionContext )
        , _ioContext( )
        , _work( boost::asio::require( _ioContext.get_executor( ),
                 boost::asio::execution::outstanding_work.tracked ) )
        , _workThread( [ this ]( ){ return this->_ioContext.run( ); } )
        , _impl
        (
            _ioContext,
            ftxRestClientService,
            ftxReferenceDataClientService,
            statisticsPublisherService,
            config
        )
    { }

    ~FtxOrderClientService( )
    {
        // Indicate that we have finished with the private io_context.
        // io_context::run( ) function will exit once all other work has completed.
        _work = boost::asio::any_io_executor( );
    }

    FtxOrderClientImpl & impl( ) { return _impl; }

    static inline boost::asio::execution_context::id id;

private:
    // Destroy all user-defined handler objects owned by the service.
    void shutdown( ) noexcept override
    {
        SYNTHFI_LOG_DEBUG( _logger ) << "Shutting down FtxOrderClientService";
    }

    // Private io_context used for performing operations on this thread.
    boost::asio::io_context _ioContext;
    // A work-tracking executor giving work for the private io_context to perform.
    boost::asio::any_io_executor _work;
    std::jthread _workThread;

    FtxOrderClientImpl _impl;

    SynthfiLogger _logger;
};

} // namespace Ftx
} // namespace Synthfi

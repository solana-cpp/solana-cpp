#pragma once

#include "synthfi/Solana/AccountBatcher/AccountBatcherImpl.hpp"

#include "synthfi/Util/Logger.hpp"

#include <boost/asio/execution_context.hpp>
#include <boost/asio/io_context.hpp>

#include <thread>

namespace Synthfi
{
namespace Solana
{

class AccountBatcherService : public boost::asio::execution_context::service
{
public:
    // Constructor creates a thread to run a private io_service.
    AccountBatcherService
    (
        boost::asio::execution_context & executionContext,
        SolanaHttpClientService & solanaHttpClientService,
        size_t multipleAccountsBatchSize
    )
        : boost::asio::execution_context::service( executionContext )
        , _ioContext( )
        , _work( boost::asio::require( _ioContext.get_executor( ),
                 boost::asio::execution::outstanding_work.tracked ) )
        , _workThread( [ this ]( ){ return this->_ioContext.run( ); } )
        , _subscriber( _ioContext, solanaHttpClientService, multipleAccountsBatchSize )
    { }

    ~AccountBatcherService( )
    {
        // Indicate that we have finished with the private io_context.
        // io_context::run( ) function will exit once all other work has completed.
        _work = boost::asio::any_io_executor( );
    }

    AccountBatcherImpl & subscriber( ) { return _subscriber; }

    static inline boost::asio::execution_context::id id;

private:
    // Destroy all user-defined handler objects owned by the service.
    void shutdown( ) noexcept override
    {
        SYNTHFI_LOG_INFO( _logger ) << "Shutting down SignatureSubscriberService";
        // TODO: should cancel all signature subscriptions.
    }

    // Private io_context used for performing operations on this thread.
    boost::asio::io_context _ioContext;
    // A work-tracking executor giving work for the private io_context to perform.
    boost::asio::any_io_executor _work;
    std::jthread _workThread;

    AccountBatcherImpl _subscriber;

    SynthfiLogger _logger;
};

} // namespace Solana
} // namespace Synthfi

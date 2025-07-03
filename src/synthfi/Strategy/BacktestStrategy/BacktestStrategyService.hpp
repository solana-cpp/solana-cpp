#pragma once

#include "synthfi/Strategy/BacktestStrategy/BacktestStrategyImpl.hpp"
#include "synthfi/Strategy/BacktestStrategy/BacktestStrategyTypes.hpp"

#include <boost/asio/execution_context.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <thread>
#include <memory>

namespace Synthfi
{
namespace Strategy
{

class BacktestStrategyService : public boost::asio::execution_context::service
{
public:
    // Constructor creates a thread to run a private io_service.
    BacktestStrategyService( boost::asio::execution_context & executionContext, BacktestStrategyConfig config );

    ~BacktestStrategyService( )
    {
        // Indicate that we have finished with the private io_context.
        // io_context::run( ) function will exit once all other work has completed.
        _work = boost::asio::any_io_executor( );
    }

    BacktestStrategyImpl * impl( ) { return _impl.get( ); }

    static inline boost::asio::execution_context::id id;

    void run( )
    {
        boost::asio::co_spawn( _ioContext, impl( )->run( ), boost::asio::detached );
        _ioContext.run( );
    }

private:
    // Destroy all user-defined handler objects owned by the service.
    void shutdown( ) noexcept override { }

    // Private io_context used for performing operations on this thread.
    boost::asio::io_context _ioContext;
    // A work-tracking executor giving work for the private io_context to perform.
    boost::asio::any_io_executor _work;
    std::jthread _workThread;

    std::unique_ptr< BacktestStrategyImpl > _impl;
};

} // namespace Strategy
} // namespace Synthfi

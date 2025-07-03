#pragma once

#include "synthfi/Strategy/BacktestStrategy/BacktestStrategyTypes.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

namespace Synthfi
{
namespace Strategy
{

template< class Service >
class BacktestStrategyServiceProvider
{
public:
    BacktestStrategyServiceProvider
    (
        boost::asio::io_context & ioContext,
        BacktestStrategyConfig config
    )
        : _service( &boost::asio::make_service< Service >( ioContext, std::move( config ) ) )
    { }

    void run( )
    {
        _service->run( );
    }

private:
    Service * _service;
};

} // namespace Strategy
} // namespace Synthfi

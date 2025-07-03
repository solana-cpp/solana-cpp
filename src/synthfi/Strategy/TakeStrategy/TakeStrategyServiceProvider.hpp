#pragma once

#include "synthfi/Strategy/TakeStrategy/TakeStrategyTypes.hpp"

#include <boost/asio/async_result.hpp>
#include <boost/asio/io_context.hpp>

namespace Synthfi
{
namespace Strategy
{

template< class Service >
class TakeStrategyServiceProvider
{
public:
    TakeStrategyServiceProvider
    (
        boost::asio::io_context & ioContext,
        TakeStrategyConfig config
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

#pragma once

namespace Synthfi
{
    template< typename ...FutureResults >
    [[ nodiscard ]]
    auto get_results( FutureResults && ... futureResults )
    {
        return std::make_tuple( futureResults.get( )... );
    }

    template< typename ...FutureResults >
    void await_results( FutureResults && ... futureResults )
    {
        ( futureResults.wait( ), ... );
    }
} // namespace Synthfi

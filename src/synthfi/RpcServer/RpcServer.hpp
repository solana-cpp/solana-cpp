#pragma once

#include "synthfi/RpcServer/RpcServerService.hpp"
#include "synthfi/RpcServer/RpcServerServiceProvider.hpp"

namespace Synthfi
{
    using RpcServer = RpcServerServiceProvider< RpcServerService >;
} // namespace Synthfi

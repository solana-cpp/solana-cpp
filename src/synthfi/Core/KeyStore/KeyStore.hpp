#pragma once

#include "synthfi/Core/KeyStore/KeyStoreServiceProvider.hpp"
#include "synthfi/Core/KeyStore/KeyStoreService.hpp"

namespace Synthfi
{
namespace Core
{
    using KeyStore = KeyStoreServiceProvider< KeyStoreService >;
} // namespace Core
} // namespace Synthfi

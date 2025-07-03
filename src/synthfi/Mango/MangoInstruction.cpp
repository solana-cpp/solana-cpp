#include "synthfi/Mango/MangoInstruction.hpp"

#include "synthfi/Util/Utils.hpp"

namespace Synthfi
{
namespace Mango
{

PlaceSpotOrderInstruction::PlaceSpotOrderInstruction
(
    uint16_t programId,
    Solana::DynamicAccountIndexArray accountIndices,
    Trading::Side side,
    uint64_t price,
    uint64_t quantity,
    Trading::OrderType orderType,
    uint64_t maxQuoteQuantity,
    uint64_t clientOrderId
)
: BaseType
(
    programId,
    accountIndices,
    magic_enum::enum_integer( Mango::MangoInstructionType::PlaceSpotOrder2 ),
    magic_enum::enum_integer( side ),
    price,
    quantity,
    maxQuoteQuantity,
    magic_enum::enum_integer( Serum::SerumSelfTradeBehavior::DecrementTake ),
    magic_enum::enum_integer
    (
        orderType == Trading::OrderType::limit
            ? Serum::SerumOrderType::Limit
            : Serum::SerumOrderType::ImmediateOrCancel
    ),
    clientOrderId,
    65535
)
{ }

} // namespace Mango
} // namespace Synthfi

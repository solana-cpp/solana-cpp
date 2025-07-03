#include "synthfi/Solana/Token.hpp"
#include "synthfi/Util/Utils.hpp"

namespace Synthfi
{
namespace Solana
{

TokenMintAccount tag_invoke( Solana::account_to_tag< TokenMintAccount >, const AccountInfo & accountInfo )
{
    if ( accountInfo.data.size( ) != TokenMintAccount::size( ) )
    {
        throw SynthfiError( "Invalid TokenMint size" );
    }

    TokenMintAccount mintAccount;

    if ( static_cast< bool >( accountInfo.data[ 0 ] ) )
    {
        std::copy
        (
            &accountInfo.data[ 1 ],
            &accountInfo.data[ 1 + sizeof( Core::PublicKey ) ],
            &mintAccount.mintAuthority->data( )[ 0 ]
        );
    }

    std::copy
    (
        &accountInfo.data[ 36 ],
        &accountInfo.data[ 36 + sizeof ( uint64_t ) ],
        reinterpret_cast< std::byte * >( &mintAccount.supply )
    );

    mintAccount.decimals = static_cast< uint8_t >( accountInfo.data[ 44 ] );
    mintAccount.isInitialized = static_cast< bool >( accountInfo.data[ 45 ] );

    if ( static_cast< bool >( accountInfo.data[ 46 ] ) )
    {
        std::copy
        (
            &accountInfo.data[ 50 ],
            &accountInfo.data[ 50 + sizeof( Core::PublicKey ) ],
            &mintAccount.freezeAuthority->data( )[ 0 ]
        );
    }

    return mintAccount;
}

} // namespace Synthfi
} // namespace Solana

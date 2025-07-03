#include "synthfi/Solana/SolanaTypes.hpp"

#include "synthfi/Util/Utils.hpp"

namespace Synthfi
{
namespace Solana
{

AccountInfo tag_invoke( json_to_tag< AccountInfo >, simdjson::ondemand::value jsonValue )
{
    AccountInfo response;

    response.executable = jsonValue[ "executable" ].value( ).get_bool( ).value( );
    response.lamports = jsonValue[ "lamports" ].value( ).get_uint64( ).value( );

    response.owner.init_from_base58( jsonValue[ "owner" ].value( ).get_string( ) );

    auto dataObject = jsonValue[ "data" ].value( );

    switch ( dataObject.type( ).value( ) )
    {
        case simdjson::ondemand::json_type::string:
        {
            // Default base58 encoding.
            response.data = dec_base58( std::span( dataObject.get_string( ).value( ) ) );
            break;
        }
        case simdjson::ondemand::json_type::array:
        {
            // TODO: assumes base64, should check response encoding.
            response.data = dec_base64( std::span( dataObject.at( 0 ).get_string( ).value( ) ) );
            break;
        }
        default:
        {
            throw SynthfiError( "Invalid 'data' field type");
        }
    }

    return response;
}

SolanaEndpointConfig tag_invoke( json_to_tag< SolanaEndpointConfig >, simdjson::ondemand::value jsonValue )
{
    return SolanaEndpointConfig
    {
        .host = std::string( jsonValue[ "host" ].get_string( ).value( ) ),
        .service = std::string( jsonValue[ "service" ].get_string( ).value( ) ),
        .target = std::string( jsonValue[ "target" ].get_string( ).value( ) )
    };
}

} // namespace Solana
} // namespace Synthfi

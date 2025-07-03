#include "synthfi/Ftx/FtxUtils.hpp"
#include "synthfi/Ftx/FtxTypes.hpp"

#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/json/serialize.hpp>

#include <magic_enum/magic_enum.hpp>

namespace Synthfi
{
namespace Ftx
{

constexpr double synthfi_sigma_value( ) { return 1E-17; }

static uint32_t crc32_checksum( std::string data )
{
    constexpr uint32_t poly = 0xEDB88320;
    uint32_t crc = -1;

    for ( auto c : data )
    {
        crc = crc ^ c;
        for ( uint8_t bit = 0; bit < 8; ++bit )
        {
            crc = crc & 1 ? ( crc >> 1 ) ^ poly : crc >> 1;
        }
    }

    return ~crc;
}

// The checksum operates on a string that represents the first 100 orders on the orderbook on either side. The format of the string is:
// <best_bid_price>:<best_bid_size>:<best_ask_price>:<best_ask_size>:<second_best_bid_price>:<second_best_ask_price>:...
static std::string generate_checksum_string( const Trading::Book & book )
{
    std::stringstream checksumStream;

    // FTX serializes checksum strings with trailing zeros.
    auto serializeWithTrailingZero = [ &checksumStream ]< class FixedPointType >( FixedPointType fixedPoint )
    {
        FixedPointType integerPart;
        auto fractionalPart = boost::multiprecision::modf( fixedPoint, &integerPart );
        if ( fractionalPart )
        {
            checksumStream << fixedPoint.str( ) << ":";
        }
        else
        {
            checksumStream << fixedPoint.str( ) << ".0:";
        }
    };

    auto & bids = book.get( Trading::Side::bid );
    auto & asks = book.get( Trading::Side::ask );
    for ( size_t i = 0; i < std::min( 100UL, std::max( asks.size( ), bids.size( ) ) ); ++i )
    {
        if ( i < bids.size( ) )
        {
            serializeWithTrailingZero( bids[ i ].price( ) );
            serializeWithTrailingZero( bids[ i ].quantity( ) );
        }
        if ( i < asks.size( ) )
        {
            serializeWithTrailingZero( asks[ i ].price( ) );
            serializeWithTrailingZero( asks[ i ].quantity( ) );
        }
    }

    std::string checksumString = checksumStream.str( );
    if ( !checksumString.empty( ) )
    {
        checksumString.pop_back( );
    }

    return checksumString;
}

uint32_t ftx_orderbook_checksum( const Trading::Book & book )
{
    return crc32_checksum( generate_checksum_string( book ) );
}

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const FtxAuthenticationMessage & message )
{
    auto msIntegerRequestTime = std::chrono::duration_cast< std::chrono::milliseconds >( message.requestTime.time_since_epoch( ) ).count( );
    auto encodeString = std::to_string( msIntegerRequestTime ).append( "websocket_login" );

    Core::Signature signature;
    signature.sign_hmac( encodeString, message.apiSecret );

    jsonValue = boost::json::object
    {
        {
            "args",
            {
                { "key", message.apiKey },
                { "sign", signature.enc_hex< 32 >( ) },
                { "time", msIntegerRequestTime }
            }
        },
        { "op", magic_enum::enum_name( FtxOp::login ) }
    };
}

} // namespace Ftx
} // namespace Synthfi

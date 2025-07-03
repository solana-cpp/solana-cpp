#include "synthfi/RpcServer/ClientRequest.hpp"

#include "synthfi/Util/StringEncode.hpp"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>

#include "synthesizer/synthesizer.h"

namespace Synthfi
{

enum system_instruction : uint32_t
{
  e_create_account = 0,
  e_assign = 1,
  e_transfer = 2
};

GetSynthfiInfoRequest tag_invoke( const boost::json::value_to_tag< GetSynthfiInfoRequest > &, const boost::json::value & jsonValue )
{
    return { };
}

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetSynthfiInfoResponse & response )
{
    jsonValue = boost::json::value{ };
}

GetSynthfiAccountInfoRequest tag_invoke( const boost::json::value_to_tag< GetSynthfiAccountInfoRequest > &, const boost::json::value & jsonValue )
{
    const auto & accountToken = jsonValue.as_array( ).front( ).as_string( );
    GetSynthfiAccountInfoRequest request;
    request.accountPublicKey.init_from_base58( std::string( accountToken.begin( ), accountToken.end( ) ) );
    return request;
}

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const GetSynthfiAccountInfoResponse & response )
{
    jsonValue = boost::json::value{ };
}

void tag_invoke( boost::json::value_from_tag, boost::json::value & jsonValue, const ErrorResponse & response )
{
    jsonValue = boost::json::object( { { "message" , response.message } } );
}

/*

void RpcClientResponse::set_error( int errorCode, const std::string & errorMessage )
{
    SYNTHFI_LOG( error ) << "[RpcClientRequest] Sending error response, code: " << errorCode << ", error: " << errorMessage;

    // send error message back to user
    _jsonWriter.reset( );
    add_json_rpc_header( );
    _jsonWriter.add_key( "error", pc::json_wtr::e_obj );
    _jsonWriter.add_key( "message", errorMessage.c_str( ) );
    _jsonWriter.pop( );
    add_tail( );

    init( "200", "OK" );
    add_hdr( "Content-Type", "application/json" );
    add_hdr( "access-control-allow-origin", "*" ); // TODO properly handle CORS.
    commit( _jsonWriter );
    print( );
}

bool GetSynthfiInfoResponse::poll_status( )
{
    if ( _pendingTable.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready )
    {
        GetAccountInfoResponse tableAccountInfo;
        try
        {
            auto tableAccountInfo = _pendingTable.get( );
        }
        catch( const std::exception & ex )
        {
            set_error( 0, ex.what( ) );
            return true;
        }

        if ( tableAccountInfo.data.size( ) != sizeof( synthfi_product_table_t ) )
        {
            set_error( 0, "Invalid synthfi product table account data" );
            return true;
        }

        synthfiState.productTable = *reinterpret_cast< const synthfi_product_table_t * >(
            tableAccountInfo.data.data( ) );

        for ( size_t i = 0; i < synthfiState.productTable.productCount; ++i )
        {
            pc::pub_key productPublicKey;
            productPublicKey.init_from_buf( synthfiState.productTable.products[ i ].k1 );
            _pendingProducts.emplace_back( _manager->get_account_info( productPublicKey ) );
        }
    }

    auto productIt = _pendingProducts.begin( );
    auto productEnd = _pendingProducts.end( );
    while ( productIt != productEnd )
    {
        if ( productIt->wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready )
        {
            GetAccountInfoResponse productInfo;
            try
            {
                auto productInfo = productIt->get( );
            }
            catch ( const std::exception & ex )
            {
                set_error( 0, ex.what( ) );
                return true;
            }

            if ( productInfo.data.size( ) != sizeof( synthfi_product_t ) )
            {
                set_error( 0, "Invalid synthfi product account data" );
                return true;
            }

            auto & productAccount = synthfiState.products.emplace_back(
                *reinterpret_cast< const synthfi_product_t * >( productInfo.data.data( ) ) );
            for ( size_t i = 0; i < productAccount.depositAccountCount; ++i )
            {
                pc::pub_key depositAccountPublicKey;
                depositAccountPublicKey.init_from_buf( productAccount.depositAccounts[ i ].k1 );
                _pendingDepositAccounts.emplace_back(
                    _manager->get_account_info( depositAccountPublicKey ) );
            }
            productIt = _pendingProducts.erase( productIt );
        }
        else
        {
            ++productIt;
        }
    }

    SYNTHFI_LOG( info )
        << "Synthfi program state overview - product count: " << synthfiState.products.size( )
        << "deposit account count: " << synthfiState.depositAccounts.size( );

    if ( _pendingTable.valid( ) || !_pendingProducts.empty( ) || !_pendingDepositAccounts.empty( ) )
    {
        return false;
    }

    add_json_rpc_header( );
    _jsonWriter.add_key( "result", pc::json_wtr::e_obj );

    _jsonWriter.add_key( "syntheticProducts", pc::json_wtr::e_arr );
    for ( const auto & product : synthfiState.products )
    {
        _jsonWriter.add_val( pc::json_wtr::e_obj );
        _jsonWriter.add_key( "name", "AAPL" );
        _jsonWriter.add_key( "assetType", "equity" );
        _jsonWriter.add_key( "price", 10000000UL );
        _jsonWriter.add_key( "priceExponent", -9L );
        _jsonWriter.add_key( "totalDeposited", 15000UL );
        _jsonWriter.add_key( "totalSynthesized", 165UL );
        _jsonWriter.pop( );
    }
    _jsonWriter.pop( );

    _jsonWriter.add_key( "USDTPrice", 10000UL );
    _jsonWriter.add_key( "USDTExponent", -5L );
    _jsonWriter.pop( );
    _jsonWriter.pop( );

    add_tail( );

    init( "200", "OK" );
    add_hdr( "Content-Type", "application/json" );
    add_hdr( "access-control-allow-origin", "*" );
    commit( _jsonWriter );
    print( );

    return true;
}

bool GetSynthfiAccountInfoResponse::poll_status( )
{
    if ( _accountInfo.wait_for( std::chrono::seconds( 0 ) ) != std::future_status::ready )
    {
        return false;
    }

    GetAccountInfoResponse accountInfoResult;
    try
    {
        accountInfoResult = _accountInfo.get( );
    }
    catch ( const std::exception & ex )
    {
        set_error( 0, ex.what( ) );
        return true;
    }

    SynthfiAccountInfo synthfiAccountInfo;
    synthfiAccountInfo.lamports = accountInfoResult.lamports;

    add_json_rpc_header( );
    _jsonWriter.add_key( "result", pc::json_wtr::e_obj );
    _jsonWriter.add_key( "lamports", synthfiAccountInfo.lamports );

    _jsonWriter.add_key( "syntheticProducts", pc::json_wtr::e_arr );
    _jsonWriter.add_key( "name", "AAPL" );
    _jsonWriter.add_key( "assetType", "equity" );
    _jsonWriter.add_key( "price", 10000000UL );
    _jsonWriter.add_key( "priceExponent", -9L );
    _jsonWriter.add_key( "totalDeposited", 15000UL );
    _jsonWriter.add_key( "totalSynthesized", 165UL );
    _jsonWriter.pop( );

    _jsonWriter.add_key( "USDTBalance", 10000UL );
    _jsonWriter.add_key( "USDTBalanceExponent", -5L );
    _jsonWriter.pop( );
    _jsonWriter.pop( );

    add_tail( );

    init( "200", "OK" );
    add_hdr( "Content-Type", "application/json" );
    add_hdr( "access-control-allow-origin", "*" );
    commit( _jsonWriter );
    print( );

    return true;
}

*/

} // namespace Synthfi

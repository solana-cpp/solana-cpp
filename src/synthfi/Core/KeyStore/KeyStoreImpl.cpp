#include "synthfi/Core/KeyStore/KeyStoreImpl.hpp"

#include "synthfi/Util/Utils.hpp"

#include <boost/asio/stream_file.hpp>
#include <boost/asio/redirect_error.hpp>
#include <boost/asio/write.hpp>

#include <boost/json/array.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/stream_parser.hpp>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace asio = boost::asio;
namespace fs = std::filesystem;

namespace Synthfi
{
namespace Core
{


KeyStoreImpl::KeyStoreImpl( asio::io_context & ioContext, const std::filesystem::path & keyStorePath )
    : _keyStorePath( keyStorePath )
    , _strand( ioContext.get_executor( ) )
{
    _publicKeys[ "sysvar_program" ];
    _publicKeys[ "sysvar_program" ].zero( );

    _publicKeys[ "sysvar_rent" ] = base58_to_public_key( get_sysvar_rent( ) );
    _publicKeys[ "spl_token_program" ] = base58_to_public_key( get_spl_token_program( ) );
    _publicKeys[ "pyth_usdt_product" ] = base58_to_public_key( get_pyth_usdt( ) );
    _publicKeys[ "mainnet_usdt" ] = base58_to_public_key( get_usdt_mainnet( ) );
}

fs::path KeyStoreImpl::get_key_pair_path( std::string_view tag ) const
{
    return _keyStorePath / ( std::string( tag ) + "_keypair" + ".json" );
}

PublicKey KeyStoreImpl::base58_to_public_key( std::string_view text )
{
    PublicKey publicKey;
    if ( !publicKey.init_from_base58( text ) )
    {
        throw SynthfiError( "Invalid PublicKey " );
    }
    return publicKey;
}

asio::awaitable< void > KeyStoreImpl::do_create_key_store_directory( )
{
    std::error_code error;
    if ( !std::filesystem::create_directory( _keyStorePath, error ) )
    {
        if ( error )
        {
            SYNTHFI_LOG_ERROR( _logger ) << "Failed to create key store directory, error: " << error.message( );
            throw SynthfiError( error.message( ) );
        }
        else
        {
            SYNTHFI_LOG_ERROR( _logger ) << "Failed to create key store directory, directory already exists";
            throw SynthfiError( "Directory already exists" );
        }
    }

    std::filesystem::permissions( _keyStorePath, std::filesystem::perms::owner_all, error );
    if ( error )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Failed to set permissions in key store directory, error: " << error.message( );
        throw SynthfiError( error.message( ) );
    }

    BOOST_LOG( _logger ) << "Created key store directory, file path: " << _keyStorePath;

    co_return;
}

asio::awaitable< void > KeyStoreImpl::do_verify_key_store_directory( )
{
    std::error_code error;
    if ( std::filesystem::exists( _keyStorePath, error ) )
    {
        if ( !std::filesystem::is_directory( _keyStorePath, error ) )
        {
            SYNTHFI_LOG_ERROR( _logger ) << "Key store path does not reference a directory";
            throw SynthfiError( "Filepath is not a directory" );
        }
    }
    else if ( error )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Error key store directory does not exist, error: " << error.message( );
        throw SynthfiError( "Key store directory does not exist" );
    }

    // Linux specific fs stat API.
    struct stat fsStat;
    if ( 0 != ::stat( _keyStorePath.c_str( ), &fsStat ) )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Cannot check the key store directory owner";
        throw SynthfiError( "Permission denied" );
    }
    if ( fsStat.st_uid != getuid( ) || fsStat.st_uid != geteuid( ) )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "User must own the key store directory";
        throw SynthfiError( "User does not own directory" );
    }

    auto perms = std::filesystem::status( _keyStorePath, error ).permissions( );
    if ( error )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Cannot check key store directory permissions, error: " << error.message( );
        throw SynthfiError( "Cannot check permissions" );
    }

    if ( ( perms & std::filesystem::perms::owner_all ) != std::filesystem::perms::owner_all )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "User must have r/w/x access to the key store directory";
        throw SynthfiError( "User does not have r/w/x permissions" );
    }

    if ( ( perms & ~std::filesystem::perms::owner_all ) != std::filesystem::perms::none )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Key store directory access must be restricted to user";
        throw SynthfiError( "Directory access is not restricted to user" );
    }

    SYNTHFI_LOG_INFO( _logger ) << "Verified key store directory, file path: " << _keyStorePath;

    co_return;
}

asio::awaitable< void > KeyStoreImpl::do_write_key_pair( const KeyPair & keyPair, std::filesystem::path keyFile )
{
    SYNTHFI_LOG_DEBUG( _logger ) << "Writing key pair: " << keyPair.enc_base58_text( ) << ", to file: " << keyFile;

    asio::stream_file fileStream( _strand );

    boost::system::error_code errorCode;
    fileStream.open( keyFile, asio::stream_file::write_only | asio::stream_file::create | asio::stream_file::exclusive );

    auto serializedJson = boost::json::serialize( keyPair.enc_binary_json( ) );

    co_await asio::async_write( fileStream, asio::buffer( serializedJson ), asio::redirect_error( asio::use_awaitable, errorCode ) );
    if ( errorCode )
    {
        fileStream.close( );
        co_return;
    }

    SYNTHFI_LOG_INFO( _logger ) << "Successfully wrote key pair: " << keyPair.enc_base58_text( ) << ", to file: " << keyFile;

    fileStream.close( );
    co_return;
}

asio::awaitable< void > KeyStoreImpl::do_create_key_pair_tag( std::string tag )
{
    KeyPair tempKey;
    tempKey.gen( );

    auto filePath = get_key_pair_path( tag );
    co_await do_write_key_pair( tempKey, filePath );

    _publicKeys.emplace( tag, PublicKey( tempKey ) );
    _keyPairs.emplace( tag, tempKey );

    co_return;
}

asio::awaitable< std::string > KeyStoreImpl::do_create_key_pair( )
{
    KeyPair tempKey;
    tempKey.gen( );

    auto tag = PublicKey( tempKey ).enc_base58_text( );

    auto filePath = get_key_pair_path( tag );
    co_await do_write_key_pair( tempKey, filePath );

    _publicKeys.emplace( tag, PublicKey( tempKey ) );
    _keyPairs.emplace( tag, tempKey );

    co_return tag;
}

asio::awaitable< void > KeyStoreImpl::do_load_key_pair( std::string tag )
{
    const auto & findKeyPair = _keyPairs.find( tag );
    // Key pair is already loaded.
    if ( findKeyPair != _keyPairs.end( ) )
    {
        co_return;
    }

    auto keyFilePath = get_key_pair_path( tag );

    asio::stream_file fileStream( _strand );

    boost::system::error_code errorCode;
    fileStream.open( keyFilePath, asio::stream_file::read_only, errorCode );
    if ( errorCode )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Error loading key pair file: " << errorCode.message( ) << ", file path: " << keyFilePath;
        throw SynthfiError( "Error loading key pair file");
    }

    boost::json::stream_parser jsonParser;
    do
    {
        std::array< char, 1024 > readBuffer;
        auto bytesRead = co_await fileStream.async_read_some( asio::buffer( readBuffer ), asio::use_awaitable );
        if ( bytesRead == 0 )
        {
            break;
        }
        jsonParser.write_some( readBuffer.data( ), bytesRead );
    }
    while ( !jsonParser.done( ) );

    fileStream.close( );

    if ( !jsonParser.done( ) )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Invalid parsed json: " << keyFilePath;
        throw SynthfiError( "json parse error");
    }

    auto parsedJson = jsonParser.release( );
    if ( !parsedJson.is_array( ) )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Invalid parsed json: " << keyFilePath;
        throw SynthfiError( "json parse error");
    }

    KeyPair tempKey;
    if ( !tempKey.init_from_binary_json( parsedJson.as_array( ) ) )
    {
        throw SynthfiError( "invalid binary key pair");
    }

    SYNTHFI_LOG_INFO( _logger ) << "Successfully loaded key pair: " << tempKey.enc_base58_text( ) << ", from file: " << keyFilePath;

    _publicKeys.emplace( tag, PublicKey( tempKey ) );
    _keyPairs.emplace( tag, tempKey );

    co_return;
}

asio::awaitable< PublicKeyRef > KeyStoreImpl::do_get_public_key( std::string tag )
{
    auto findPublicKey = _publicKeys.find( tag );
    if ( findPublicKey == _publicKeys.end( ) )
    {
        throw SynthfiError( "Public key does not exist");
    }
    co_return &findPublicKey->second;
}

asio::awaitable< KeyPairRef > KeyStoreImpl::do_get_key_pair( std::string tag )
{
    const auto & findKeyPair = _keyPairs.find( tag );
    if ( findKeyPair == _keyPairs.end( ) )
    {
        throw SynthfiError( "Key pair does not exist");
    }
    co_return &findKeyPair->second;
}

} // namespace Core
} // namespace Synthfi
#include <boost/preprocessor/stringize.hpp>

// Core services
#include "synthfi/Core/KeyStore/KeyStore.hpp"

// Solana types
#include "synthfi/Solana/SignatureSubscriber/SignatureSubscriber.hpp"
#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClient.hpp"
#include "synthfi/Solana/SolanaHttpMessage.hpp"
#include "synthfi/Solana/SolanaWsMessage.hpp"

// Serum types
#include "synthfi/Serum/SerumTypes.hpp"

// Strategy
#include "synthfi/Strategy/TakeStrategy/TakeStrategy.hpp"
#include "synthfi/Strategy/TakeStrategy/TakeStrategyTypes.hpp"

#include "synthfi/Strategy/BacktestStrategy/BacktestStrategy.hpp"
#include "synthfi/Strategy/BacktestStrategy/BacktestStrategyTypes.hpp"

// Utils
#include "synthfi/Util/AsyncUtils.hpp"
#include "synthfi/Util/Logger.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/asio/use_future.hpp>

#include <boost/program_options.hpp>

#include <simdjson.h>

#include <iostream>
#include <optional>
#include <unordered_map>

#include <unistd.h>

namespace asio = boost::asio;
namespace po = boost::program_options;
namespace fs = std::filesystem;

//
// Synthfi command-line tool.
//

namespace Synthfi
{

static std::string get_version( ) { return "0.0.1"; }

static fs::path get_key_store_default( )
{
    fs::path dir = std::getenv( "HOME" );
    return dir / ".synthfi";
};

static std::string ftx_api_uri_default( ) { return "ftx.us"; }

static std::string solana_api_host_default( ) { return "localhost"; }
static constexpr uint16_t solana_http_api_port_default( ) { return 8899; }
static constexpr uint16_t solana_ws_api_port_default( ) { return 8900; }
static constexpr uint64_t solana_max_multiple_accounts_default( ) { return 100; }

static std::string rpc_listen_address_default( ) { return "localhost" ; }
static constexpr uint16_t rpc_listen_port_default( ) { return 8799; }

static fs::path synthfi_data_directory_default( ) { return "synthfi_data"; }
// KeyStore options.

#define DECLARE_KEY_STORE_DIRECTORY_OPTION( keyStoreDirectory ) \
( \
    "key_store_directory,k", \
    po::value< fs::path >( keyStoreDirectory )->default_value( get_key_store_default( ) ), \
    "File path to directory containing key pairs." \
)

#define DECLARE_PUBLIC_KEY_TAG_OPTION( publicKeyPath ) \
( \
    "file,f", \
    po::value< std::string >( publicKeyPath ), \
    "KeyPair tag." \
)

// FTX API options.

#define DECLARE_FTX_API_KEY_OPTION( ftxApiKey ) \
( \
    "ftx_api_key", \
    po::value< std::string >( ftxApiKey )->required( ), \
    "FTX API key." \
)

#define DECLARE_FTX_API_SECRET_OPTION( ftxApiSecret ) \
( \
    "ftx_api_secret", \
    po::value< std::string >( ftxApiSecret )->required( ), \
    "FTX API secret." \
)

#define DECLARE_FTX_API_URI_OPTION( ftxhost ) \
( \
    "ftx_api_uri", \
    po::value< std::string >( ftxhost )->default_value( ftx_api_uri_default( ) ), \
    "FTX API URI." \
)

// Solana API options.

#define DECLARE_SOLANA_MAX_MULTIPLE_ACCOUNTS( solanaMaxMultipleAccounts ) \
( \
    "solana_max_multiple_accounts", \
    po::value< uint64_t >( solanaMaxMultipleAccounts )->default_value( solana_max_multiple_accounts_default( ) ), \
    "The maximum number of accounts sent in a getMultipleAccounts HTTP RPC method." \
)

#define DECLARE_SOLANA_API_HOST_OPTION( solanaApiHost ) \
( \
    "solana_api_host,r", \
    po::value< std::string >( solanaApiHost )->default_value( solana_api_host_default( ) ), \
    "Solana RPC API hostname or URL." \
)

#define DECLARE_SOLANA_HTTP_API_PORT_OPTION( solanaHttpApiPort ) \
( \
    "solana_http_api_port,p", \
    po::value< uint16_t >( solanaHttpApiPort )->default_value( solana_http_api_port_default( ) ), \
    "Solana HTTP RPC API server port." \
)

#define DECLARE_SOLANA_WS_API_PORT_OPTION( solanaWsApiPort ) \
( \
    "solana_ws_api_port,w", \
    po::value< uint16_t >( solanaWsApiPort )->default_value( solana_ws_api_port_default( ) ), \
    "Solana WS RPC API server port." \
)

// Strategy Options

#define DECLARE_STRATEGY_CONFIG_PATH( strategyConfigPath ) \
( \
    "strategy_config_path", \
    po::value< std::filesystem::path >( strategyConfigPath )->required( ), \
    "Path to strategy config json file." \
)

// RPC Server API options.

#define DECLARE_RPC_LISTEN_ADDRESS_OPTION( rpcListenAddress ) \
( \
    "rpc_listen_address,a", \
    po::value<  std::string >( rpcListenAddress )->default_value( rpc_listen_address_default( ) ), \
    "Synthfi RPC listen address." \
)

#define DECLARE_RPC_LISTEN_PORT_OPTION( rpcListenPort ) \
( \
    "rpc_listen_port,l", \
    po::value<  uint16_t >( rpcListenPort )->default_value( rpc_listen_port_default( ) ), \
    "Synthfi RPC listen port." \
)

class ClientCommand
{
public:
    ClientCommand( const std::string & name ) : _name( name ), _commandOptions( _name ) { }
    virtual ~ClientCommand( ) = default;

    const std::string & command_name( ) const { return _name; }
    const po::options_description & get_command_options( ) const { return _commandOptions; }

    // Returns 0 on success, otherwise error code.
    virtual int on_command( ) const & = 0;

protected:
    std::string _name;
    po::options_description _commandOptions;

    mutable SynthfiLogger _logger;
};

class InitKeyCommand : public ClientCommand
{
public:
    InitKeyCommand( ) : ClientCommand( "init_key" )
    {
        _commandOptions.add_options( )
            DECLARE_KEY_STORE_DIRECTORY_OPTION( &_keyStoreDirectory );
    }

    int on_command( ) const & override
    {
        SYNTHFI_LOG_INFO( _logger ) << "Initializing Key Store Directory";

        boost::asio::io_context ioContext;
        Core::KeyStore keyStore( ioContext, { _keyStoreDirectory } );

        keyStore.create_key_store_directory( boost::asio::use_future ).wait( );
        keyStore.verify_key_store_directory( boost::asio::use_future ).wait( );

        keyStore.create_key_pair( "publish", boost::asio::use_future ).wait( );

        return 0;
    }

private:
    fs::path _keyStoreDirectory;
};

class InitUsdtCommand : public ClientCommand
{
public:
    InitUsdtCommand( ) : ClientCommand( "init_usdt" )
    {
        _commandOptions.add_options( )
            DECLARE_KEY_STORE_DIRECTORY_OPTION( &_keyStoreDirectory );
    }

    int on_command( ) const & override
    {
        SYNTHFI_LOG_INFO( _logger ) << "Initializing USDT token key pair";

        boost::asio::io_context ioContext;
        Core::KeyStore keyStore( ioContext, { _keyStoreDirectory } );

        keyStore.verify_key_store_directory( boost::asio::use_future ).wait( );
        keyStore.create_key_pair( "usdt_testnet", boost::asio::use_future ).wait( );

        return 0;
    }

private:
    fs::path _keyStoreDirectory;
};

class InitProgramCommand : public ClientCommand
{
public:
    InitProgramCommand( ) : ClientCommand( "init_program" )
    {
        _commandOptions.add_options( )
            DECLARE_KEY_STORE_DIRECTORY_OPTION( &_keyStoreDirectory );
    }

    int on_command( ) const & override
    {
        boost::asio::io_context ioContext;
        Core::KeyStore keyStore( ioContext, { _keyStoreDirectory } );

        keyStore.verify_key_store_directory( boost::asio::use_future ).wait( );
        keyStore.create_key_pair( "synthfi_program", boost::asio::use_future ).wait( );

        return 0;
    }

private:
    fs::path _keyStoreDirectory;
};

class InitSynthfiCommand : public ClientCommand
{
public:
    InitSynthfiCommand( ) : ClientCommand( "init_synthfi" )
    {
        _commandOptions.add_options( )
            DECLARE_KEY_STORE_DIRECTORY_OPTION( &_keyStoreDirectory )
            DECLARE_SOLANA_API_HOST_OPTION( &_solanaApiHost )
            DECLARE_SOLANA_HTTP_API_PORT_OPTION( &_solanaApiPort )
            DECLARE_SOLANA_WS_API_PORT_OPTION( &_solanaWsPort );
    }

    int on_command( ) const & override
    {
        boost::asio::io_context ioContext;
        // KeyStore keyStore( ioContext, { _keyStoreDirectory } );
        // keyStore.verify_key_store_directory( boost::asio::use_future ).wait( );

        // await_results
        // (
        //     keyStore.load_key_pair( "publish", boost::asio::use_future ),
        //     keyStore.load_key_pair( "synthfi_program", boost::asio::use_future ),
        //     keyStore.create_key_pair( "synthfi_table", boost::asio::use_future )
        // );

        // Solana::SolanaHttpClient solanaHttpClient( ioContext, _solanaApiHost, _solanaApiPort );

        // auto [ recentBlockhash, minBalanceForRentExemption ] = get_results(
        //         solanaHttpClient.send_request< Solana::GetRecentBlockhashRequest, Solana::GetRecentBlockhashResponse >( { }, boost::asio::use_future ),
        //         solanaHttpClient.send_request< Solana::GetMinBalanceForRentExemptionRequest, Solana::GetMinBalanceForRentExemptionResponse >(
        //             { sizeof( synthfi_product_t ) },
        //             boost::asio::use_future ) );

        // auto [ publishKeyPair, synthfiTableKeyPair, synthfiProgramKeyPair, sysvarProgramPublicKey ] =
        //     get_results
        //     (
        //         keyStore.get_key_pair( "publish", boost::asio::use_future ),
        //         keyStore.get_key_pair( "synthfi_table", boost::asio::use_future ),
        //         keyStore.get_public_key( "synthfi_program", boost::asio::use_future ),
        //         keyStore.get_public_key( "sysvar_program", boost::asio::use_future )
        //     );

        // auto & solanaWsClientService = boost::asio::make_service< Solana::SolanaWsClientService >
        // (
        //     ioContext,
        //     statisticsPublisherService,
        //     _solanaApiHost,
        //     _solanaWsPort
        // );
        // Solana::SignatureSubscriber signatureSubscriber( ioContext, solanaWsClientService );

        // auto response = solanaHttpClient.send_request< Solana::SendInitSynthfiRequest, Solana::SendTransactionResponse >
        // (
        //     {
        //         recentBlockhash.blockhash,
        //         minBalanceForRentExemption.lamports,
        //         publishKeyPair,
        //         synthfiTableKeyPair,
        //         synthfiProgramKeyPair,
        //         sysvarProgramPublicKey
        //     },
        //     boost::asio::use_future
        // )
        // .get( );

        // auto signature = signatureSubscriber.signature_subscribe( response.transactionSignature, Solana::Commitment::finalized, boost::asio::use_future ).get( );
        // if ( signature.error )
        // {
        //     SYNTHFI_LOG_ERROR( _logger ) << "Transaction signature error: " << signature.error.value( );
        // }
        // else
        // {
        //     SYNTHFI_LOG_INFO( _logger ) << "Initialize Synthfi transaction finalized, signature: " << response.transactionSignature;
        // }

        return 0;
    }

private:


    fs::path _keyStoreDirectory;
    std::string _solanaApiHost;
    uint16_t _solanaApiPort;
    uint16_t _solanaWsPort;
};

class InitProductCommand : public ClientCommand
{
public:
    InitProductCommand( ) : ClientCommand( "init_product" )
    {
        _commandOptions.add_options( )
            DECLARE_KEY_STORE_DIRECTORY_OPTION( &_keyStoreDirectory )
            DECLARE_SOLANA_API_HOST_OPTION( &_solanaApiHost )
            DECLARE_SOLANA_HTTP_API_PORT_OPTION( &_solanaApiPort );
    }

    int on_command( ) const & override
    {
        // initialize connection to blockchain
        boost::asio::io_context ioContext;
        // KeyStore keyStore( ioContext, { _keyStoreDirectory } );
        // keyStore.verify_key_store_directory( boost::asio::use_future ).wait( );

        // await_results
        // (
        //     keyStore.load_key_pair( "publish", boost::asio::use_future ),
        //     keyStore.load_key_pair( "synthfi_program", boost::asio::use_future ),
        //     keyStore.load_key_pair( "synthfi_table", boost::asio::use_future )
        // );

        // auto [ productTag, productTokenTag, mintTag ] =
        //     get_results
        //     (
        //         keyStore.create_key_pair( boost::asio::use_future ),
        //         keyStore.create_key_pair( boost::asio::use_future ),
        //         keyStore.create_key_pair( boost::asio::use_future )
        //     );

        // auto [ publishKeyPair, productKeyPair, productTokenKeyPair, mintKeyPair, synthfiTableKeyPair ] =
        //     get_results
        //     (
        //         keyStore.get_key_pair( "publish", boost::asio::use_future ),
        //         keyStore.get_key_pair( productTag, boost::asio::use_future ),
        //         keyStore.get_key_pair( productTokenTag, boost::asio::use_future ),
        //         keyStore.get_key_pair( mintTag, boost::asio::use_future ),
        //         keyStore.get_key_pair( "synthfi_table", boost::asio::use_future )
        //     );

        // auto [ synthfiProgramPublicKey, sysvarProgramPublicKey, sysvarRentPublicKey, splTokenProgramPublicKey ] =
        //     get_results
        //     (
        //         keyStore.get_public_key( "synthfi_program", boost::asio::use_future ),
        //         keyStore.get_public_key( "sysvar_program", boost::asio::use_future ),
        //         keyStore.get_public_key( "sysvar_rent", boost::asio::use_future ),
        //         keyStore.get_public_key( "spl_token_program", boost::asio::use_future )
        //     );

        // Solana::SolanaHttpClient solanaHttpClient( ioContext, _solanaApiHost, _solanaApiPort );

        // auto [ recentBlockhash, minBalanceForRentExemption ] =
        //     get_results
        //     (
        //         solanaHttpClient.send_request< Solana::GetRecentBlockhashRequest, Solana::GetRecentBlockhashResponse >( { }, boost::asio::use_future ),
        //         solanaHttpClient.send_request< Solana::GetMinBalanceForRentExemptionRequest, Solana::GetMinBalanceForRentExemptionResponse >(
        //             { Solana::SendAddSynthfiProductRequest::synthfi_product_account_space( ) },
        //             boost::asio::use_future )
        //     );

        // auto response = solanaHttpClient.send_request< Solana::SendCreateAccountRequest, Solana::SendTransactionResponse >(
        //     {
        //         recentBlockhash.blockhash,
        //         minBalanceForRentExemption.lamports,
        //         publishKeyPair,
        //         productKeyPair,
        //         synthfiProgramPublicKey,
        //         sysvarProgramPublicKey,
        //         Solana::SendAddSynthfiProductRequest::synthfi_product_account_space( )
        //     },
        //     boost::asio::use_future )
        //     .get( );

        // auto & solanaWsClientService = boost::asio::make_service< Solana::SolanaWsClientService >
        // (
        //     ioContext,
        //     _solanaApiHost,
        //     static_cast< uint16_t >( _solanaApiPort + 1 )
        // );
        // Solana::SignatureSubscriber signatureSubscriber( ioContext, solanaWsClientService );

        // auto signature = signatureSubscriber.signature_subscribe( response.transactionSignature, Solana::Commitment::finalized, boost::asio::use_future ).get( );
        // if ( signature.error )
        // {
        //     SYNTHFI_LOG_ERROR( _logger ) << "Transaction signature error: " << signature.error.value( );
        // }
        // else
        // {
        //     SYNTHFI_LOG_INFO( _logger ) << "Initialize product account transaction finalized, signature: " << response.transactionSignature;
        // }

        // recentBlockhash = solanaHttpClient.send_request< Solana::GetRecentBlockhashRequest, Solana::GetRecentBlockhashResponse >( { }, boost::asio::use_future ).get( );
        // response = solanaHttpClient.send_request< Solana::SendInitMintRequest, Solana::SendTransactionResponse >
        // (
        //     {
        //         recentBlockhash.blockhash,
        //         minBalanceForRentExemption.lamports,
        //         publishKeyPair,
        //         mintKeyPair,
        //         static_cast< Core::PublicKeyRef >( productKeyPair ),
        //         sysvarProgramPublicKey,
        //         sysvarRentPublicKey,
        //         splTokenProgramPublicKey
        //     },
        //     boost::asio::use_future
        // )
        // .get( );

        // signature = signatureSubscriber.signature_subscribe( response.transactionSignature, Solana::Commitment::finalized, boost::asio::use_future ).get( );
        // if ( signature.error )
        // {
        //     SYNTHFI_LOG_ERROR( _logger ) << "Transaction signature error: " << signature.error.value( );
        // }
        // else
        // {
        //     SYNTHFI_LOG_INFO( _logger ) << "Initialize mint account transaction finalized, signature: " << response.transactionSignature;
        // }

        // recentBlockhash = solanaHttpClient.send_request< Solana::GetRecentBlockhashRequest, Solana::GetRecentBlockhashResponse >( { }, boost::asio::use_future ).get( );
        // response = solanaHttpClient.send_request< Solana::SendInitTokenAccountRequest, Solana::SendTransactionResponse >
        // (
        //     {
        //         recentBlockhash.blockhash,
        //         minBalanceForRentExemption.lamports,
        //         publishKeyPair,
        //         productTokenKeyPair,
        //         synthfiProgramPublicKey,
        //         static_cast< Core::PublicKeyRef >( mintKeyPair ),
        //         sysvarProgramPublicKey,
        //         sysvarRentPublicKey,
        //         splTokenProgramPublicKey
        //     },
        //     boost::asio::use_future
        // )
        // .get( );

        // signature = signatureSubscriber.signature_subscribe( response.transactionSignature, Solana::Commitment::finalized, boost::asio::use_future ).get( );
        // if ( signature.error )
        // {
        //     SYNTHFI_LOG_ERROR( _logger ) << "Transaction signature error: " << signature.error.value( );
        // }
        // else
        // {
        //     SYNTHFI_LOG_INFO( _logger ) << "Initialize token account transaction finalized, signature: " << response.transactionSignature;
        // }

        // recentBlockhash = solanaHttpClient.send_request< Solana::GetRecentBlockhashRequest, Solana::GetRecentBlockhashResponse >( { }, boost::asio::use_future ).get( );
        // response = solanaHttpClient.send_request< Solana::SendAddSynthfiProductRequest, Solana::SendTransactionResponse >
        // (
        //     {
        //         recentBlockhash.blockhash,
        //         minBalanceForRentExemption.lamports,
        //         publishKeyPair,
        //         synthfiTableKeyPair,
        //         productKeyPair,
        //         synthfiProgramPublicKey
        //     },
        //     boost::asio::use_future
        // )
        // .get( );

        // signature = signatureSubscriber.signature_subscribe( response.transactionSignature, Solana::Commitment::finalized, boost::asio::use_future ).get( );
        // if ( signature.error )
        // {
        //     SYNTHFI_LOG_ERROR( _logger ) << "Transaction signature error: " << signature.error.value( );
        // }
        // else
        // {
        //     SYNTHFI_LOG_INFO( _logger ) << "Successfully initialized synthfi product account, signature: " << response.transactionSignature;
        // }

        return 0;
    }

private:
    fs::path _keyStoreDirectory;
    std::string _solanaApiHost;
    uint16_t _solanaApiPort;
};

class RpcServerCommand : public ClientCommand
{
public:
    RpcServerCommand( ) : ClientCommand ( "synthfi_rpc_server" )
    {
        _commandOptions.add_options( )
            DECLARE_KEY_STORE_DIRECTORY_OPTION( &_keyStoreDirectory )
            DECLARE_SOLANA_API_HOST_OPTION( &_solanaApiHost )
            DECLARE_SOLANA_HTTP_API_PORT_OPTION( &_solanaApiPort )
            DECLARE_RPC_LISTEN_ADDRESS_OPTION( &_rpcListenAddress )
            DECLARE_RPC_LISTEN_PORT_OPTION( &_rpcListenPort );
    }

    int on_command( ) const & override
    {
        boost::system::error_code errorCode;
        auto listenAddress = boost::asio::ip::make_address( _rpcListenAddress, errorCode );
        if ( errorCode )
        {
            SYNTHFI_LOG_ERROR( _logger ) << "Invalid rpc listen address: " << errorCode;
            return 1;
        }

        boost::asio::io_context ioContext;
        // RpcServer rpcServer( ioContext, _solanaApiHost, _solanaApiPort, boost::asio::ip::tcp::endpoint( listenAddress, _rpcListenPort ) );

        return 0;
    }

private:
    fs::path _keyStoreDirectory;

    std::string _solanaApiHost;
    uint16_t _solanaApiPort;

    std::string _rpcListenAddress;
    uint16_t _rpcListenPort;
};

class TakeStrategyCommand : public ClientCommand
{
public:
    TakeStrategyCommand( ) : ClientCommand( "take_strategy" )
    {
        _commandOptions.add_options( )
            DECLARE_STRATEGY_CONFIG_PATH( &_strategyConfigPath );
    }

    int on_command( ) const & override
    {
        SYNTHFI_LOG_INFO( _logger ) << "Executing TakeStrategy";

        Synthfi::Strategy::TakeStrategyConfig config;

        try
        {
            simdjson::ondemand::parser parser;
            simdjson::padded_string configBuffer = simdjson::padded_string::load( _strategyConfigPath.native( ) );
            simdjson::ondemand::document doc = parser.iterate( configBuffer );
            simdjson::ondemand::value jsonValue( doc );
            config = json_to< Strategy::TakeStrategyConfig >( jsonValue );
        }
        catch ( const std::exception & ex )
        {

            SYNTHFI_LOG_INFO( _logger ) << "Error parsing config: " << ex.what( );
            return 1;
        }

        boost::asio::io_context ioContext;
        Strategy::TakeStrategy takeStrategy
        (
            ioContext,
            std::move( config )
        );

        takeStrategy.run( );

        SYNTHFI_LOG_INFO( _logger ) << "Exiting TakeStrategy";

        return 0;
    }

private:
    std::filesystem::path _strategyConfigPath;
};

class BacktestStrategyCommand : public ClientCommand
{
public:
    BacktestStrategyCommand( ) : ClientCommand( "backtest_strategy" )
    {
        _commandOptions.add_options( )
            DECLARE_STRATEGY_CONFIG_PATH( &_strategyConfigPath );
    }

    int on_command( ) const & override
    {
        SYNTHFI_LOG_INFO( _logger ) << "Executing BacktestStrategy";

        Strategy::BacktestStrategyConfig config;
        try
        {
            simdjson::ondemand::parser parser;
            simdjson::padded_string configBuffer = simdjson::padded_string::load( _strategyConfigPath.native( ) );
            simdjson::ondemand::document doc = parser.iterate( configBuffer );
            simdjson::ondemand::value jsonValue( doc );
            config = json_to< Strategy::BacktestStrategyConfig >( jsonValue );
        }
        catch ( const std::exception & ex )
        {
            SYNTHFI_LOG_INFO( _logger ) << "Error parsing config: " << ex.what( );
            return 1;
        }

        boost::asio::io_context ioContext;
        Strategy::BacktestStrategy backtestStrategy
        (
            ioContext,
            std::move( config )
        );

        backtestStrategy.run( );

        SYNTHFI_LOG_INFO( _logger ) << "Exiting BacktestStrategy";

        return 0;
    }

private:
    std::filesystem::path _strategyConfigPath;
};

class GetPublicKeyCommand : public ClientCommand
{
public:
    GetPublicKeyCommand( ) : ClientCommand( "get_public_key" )
    {
        _commandOptions.add_options( )
            DECLARE_KEY_STORE_DIRECTORY_OPTION( &_keyStoreDirectory )
            DECLARE_PUBLIC_KEY_TAG_OPTION( &_keyPairTag );
    }

    int on_command( ) const & override
    {
        boost::asio::io_context ioContext;

        Core::KeyStore keyStore( ioContext, { _keyStoreDirectory } );
        keyStore.verify_key_store_directory( boost::asio::use_future ).wait( );

        try
        {
            keyStore.load_key_pair( _keyPairTag, boost::asio::use_future ).get( );
        }
        catch ( const std::exception & ex )
        {
            std::cout << "Error loading key pair: " << ex.what( );
            return 1;
        }

        auto publicKey = keyStore.get_public_key( _keyPairTag, boost::asio::use_future ).get( );

        std::cout << "Key store directory: " << _keyStoreDirectory << std::endl;
        std::cout << "Key pair tag: " << _keyPairTag << std::endl;
        std::cout << "Public key: " << *publicKey << std::endl;

        return 0;
    }

private:
    fs::path _keyStoreDirectory;
    std::string _keyPairTag;
};

class SynthfiClient
{
public:
    explicit SynthfiClient( const std::string & programName )
        : _programName( programName )
        , _clientArguments( "Options" )
        , _optionalArguments( "optional arguments" )
    {
        _optionalArguments.add_options( )
            ( "help,h", "Show the help message and exit" )
            (
                "log_level",
                po::value< std::string >( )->default_value( "trace" ),
                "Filter console logs by severity"
            )
            ( "version,V", "Show the version number and exit" );

        _clientArguments.add( _optionalArguments );

        register_command( std::make_unique< Synthfi::TakeStrategyCommand >( ) );
        register_command( std::make_unique< Synthfi::BacktestStrategyCommand >( ) );
        register_command( std::make_unique< Synthfi::GetPublicKeyCommand >( ) );
        register_command( std::make_unique< Synthfi::InitKeyCommand >( ) );
        register_command( std::make_unique< Synthfi::InitProductCommand >( ) );
        register_command( std::make_unique< Synthfi::InitProgramCommand >( ) );
        register_command( std::make_unique< Synthfi::InitSynthfiCommand >( ) );
        register_command( std::make_unique< Synthfi::InitUsdtCommand >( ) );
        register_command( std::make_unique< Synthfi::RpcServerCommand >( ) );
    }

    std::optional< po::variables_map > parse_command_line( int argc, char ** argv )
    {
        try
        {
            po::variables_map parsedArgs;
            po::store(
                po::command_line_parser( argc, argv ).options( _clientArguments ).run( ),
                parsedArgs );
            notify( parsedArgs );
            return { parsedArgs };
        }
        catch ( std::exception & ex )
        {
            print_usage_error( ex.what( ) );
            return { };
        }
    }

    bool is_command_valid( const std::string & command ) const
    {
        return _clientCommands.contains( command );
    }

    void execute_command( const std::string & command ) const
    {
        const auto & findCommand = _clientCommands.find( command );
        if ( findCommand == _clientCommands.end( ) )
        {
            print_usage_error( "invalid command: " + command );
            return;
        }
        findCommand->second->on_command( );
    }

    void add_command( const std::string & command )
    {
        const auto & findCommand = _clientCommands.find( command );
        BOOST_ASSERT_MSG( findCommand != _clientCommands.end( ), "Duplicate command" );

        const auto * clientCommand = findCommand->second.get( );
        _clientArguments.add( clientCommand->get_command_options( ) );
    }

    void print_usage( ) const
    {
        std::cout << "usage: " << _programName << " [-h] command ...\n" << std::endl;
        std::cout << _programName << " is a tool for managing and deploying the synthfi project\n" << std::endl;
    };

    void print_usage_error( const std::string & error ) const
    {
        std::cerr << "usage: " << _programName << " [-h] command ...\n";
        std::cerr << _programName << ": error: " << error << std::endl;
    }

    void print_help( ) const
    {
        print_usage( );
        std::cout << _clientArguments << std::endl;
    }

    void print_positional_help( ) const
    {
        print_help( );

        std::cout << "positional arguments:\n";
        std::cout << "  command:\n";
        for ( const auto & [ commandName, _ ] : _clientCommands )
        {
            std::cout << "    " << commandName << "\n";
        }
        std::cout << std::endl;
    }

    void print_version( )
    {
        std::cout << _programName << " client version: " << get_version( ) << ", contract version: " << SYNTHFI_VERSION << std::endl;
    }

private:
    void register_command( std::unique_ptr< Synthfi::ClientCommand > command )
    {
        const auto & commandName = command->command_name( );
        auto inserted = _clientCommands.emplace( commandName, std::move( command ) ).second;
        BOOST_ASSERT_MSG( inserted, "Registered duplicate command" );
    }

    std::string _programName;

    po::options_description _clientArguments;
    po::options_description _optionalArguments;
    std::unordered_map< std::string, std::unique_ptr< Synthfi::ClientCommand > > _clientCommands;
};

} // namespace Synthfi

int main( int argc, char ** argv )
{
    auto programName = fs::path( argv[ 0 ] ).filename( );
    Synthfi::SynthfiClient synthfiClient( programName );

    std::string command;
    if ( argc > 1 )
    {
        command = argv[ 1 ];
        if ( synthfiClient.is_command_valid( command ) )
        {
            synthfiClient.add_command( command );
        }
    }

    auto parsedArgs = synthfiClient.parse_command_line( argc, argv );
    if ( !parsedArgs ) return 1;

    if ( parsedArgs->count( "help" ) )
    {
        synthfiClient.is_command_valid( command ) ? synthfiClient.print_help( ) : synthfiClient.print_positional_help( );
        return 0;
    }

    if ( parsedArgs->count( "version" ) )
    {
        synthfiClient.print_version( );
        return 0;
    }

    // Initialize logger and severity filter.
    boost::log::trivial::severity_level logLevel;
    const auto & logLevelArg = parsedArgs->find( "log_level" );
    BOOST_ASSERT_MSG( logLevelArg != parsedArgs->end( ), "Expected log_level command-line option" );

    const auto & logLevelString = logLevelArg->second.as< std::string >( );
    auto success = boost::log::trivial::from_string( logLevelString.data( ), logLevelString.size( ), logLevel );
    if ( !success )
    {
        std::cerr << "Invalid log-level option, valid options are: TRACE, DEBUG, INFO, ERROR" << std::endl;
        return 1;
    }
    Synthfi::init_logger( logLevel );

    // Execute user's command.
    synthfiClient.execute_command( command );

    return 0;
};

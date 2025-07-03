#include "synthfi/Mango/MangoOrderClient/MangoOrderClientImpl.hpp"

#include "synthfi/Mango/MangoInstruction.hpp"
#include "synthfi/Solana/SolanaHttpMessage.hpp"

#include <boost/asio/as_tuple.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/experimental/promise.hpp>
#include <boost/asio/high_resolution_timer.hpp>

#include <boost/asio/experimental/awaitable_operators.hpp>

#include <fmt/core.h>

using namespace boost::asio::experimental::awaitable_operators;
using namespace std::chrono_literals;

namespace asio = boost::asio;
namespace beast = boost::beast;

namespace Synthfi
{
namespace Mango
{

// Delay in ms to wait to subscribe to private WS channels after logging in.
static constexpr std::chrono::milliseconds Mango_authentication_delay_ms( ) { return 1000ms; }

MangoOrderClientImpl::MangoOrderClientImpl
(
    boost::asio::io_context & ioContext,
    Core::KeyStoreService & keyStoreService,
    Solana::SolanaHttpClientService & solanaHttpClientService,
    Solana::AccountSubscriberService & accountSubscriberService,
    Solana::SignatureSubscriberService & signatureSubscriberService,
    Solana::SlotSubscriberService & slotSubscriberService,
    Serum::SerumReferenceDataClientService & serumReferenceDataClientService,
    Mango::MangoReferenceDataClientService & mangoReferenceDataClientService,
    Statistics::StatisticsPublisherService & statisticsPublisherService
)
    : _strand( ioContext.get_executor( ) )
    , _keyStore( keyStoreService )
    , _solanaHttpClient( solanaHttpClientService ) 
    , _accountSubscriber( accountSubscriberService )
    , _signatureSubscriber( signatureSubscriberService )
    , _slotSubscriber( slotSubscriberService )
    , _serumReferenceDataClient( serumReferenceDataClientService )
    , _mangoReferenceDataClient( mangoReferenceDataClientService )
    , _statisticsPublisher( statisticsPublisherService, "mango_order_client" )
{ }

void MangoOrderClientImpl::on_mango_account_notification( uint64_t subscriptionId, Solana::AccountNotification accountNotification )
{
    co_spawn( _strand, do_on_mango_account_notification( subscriptionId, std::move( accountNotification ) ), boost::asio::detached );
}

boost::asio::awaitable< void > MangoOrderClientImpl::do_on_mango_account_notification( uint64_t subscriptionId, Solana::AccountNotification accountNotification )
{
    _mangoReferenceData.mangoAccount = Solana::account_to< Mango::MangoAccount >( std::move( accountNotification.accountInfo ) );

    co_return;
}

void MangoOrderClientImpl::on_recent_blockhash( Core::Hash recentBlockhash )
{
    co_spawn( _strand, do_on_recent_blockhash( recentBlockhash ), boost::asio::detached );
}

boost::asio::awaitable< void > MangoOrderClientImpl::do_on_recent_blockhash( Core::Hash recentBlockhash )
{
    _recentBlockhash = recentBlockhash;

    co_return;
}

boost::asio::awaitable< void > MangoOrderClientImpl::do_load_mango_account( )
{
    // Slot subscribe
    co_await _slotSubscriber.subscribe_recent_blockhash
    (
        std::bind( &MangoOrderClientImpl::on_recent_blockhash, this, std::placeholders::_1 )
    );

    // Load reference data.
    auto [ serumReferenceData, mangoReferenceData ] = co_await
    (
        _serumReferenceDataClient.get_reference_data( )
        && _mangoReferenceDataClient.get_reference_data( )
    );
    _serumReferenceData = std::move( serumReferenceData );
    _mangoReferenceData = std::move( mangoReferenceData );

    // Load mango account owner key pair.
    auto mangoAccountOwnerTag = _mangoReferenceData.mangoAccount.owner.enc_base58_text( );
    co_await _keyStore.load_key_pair( mangoAccountOwnerTag );

    auto [ mangoOwnerKey, splTokenProgram, sysvarRent ] =
    co_await
    (
        _keyStore.get_key_pair( mangoAccountOwnerTag )
        && _keyStore.get_public_key( "spl_token_program" )
        && _keyStore.get_public_key( "sysvar_rent" )
    );

    _mangoOwnerKey = mangoOwnerKey;
    _splTokenProgram = splTokenProgram;
    _sysvarRent = sysvarRent;

    // Subscribe to Mango account.
    co_await _accountSubscriber.account_subscribe
    (
        Solana::AccountSubscribe{ _mangoReferenceData.mangoAccountAddress, Solana::Commitment::processed },
        std::bind( &MangoOrderClientImpl::on_mango_account_notification, this, std::placeholders::_1, std::placeholders::_2 )
    );

    co_return;
}

asio::awaitable< Trading::Order > MangoOrderClientImpl::do_send_order( Trading::Order order )
{
    const auto & serumTradingPair = _serumReferenceData.tradingPairs[ order.trading_pair_index( ) ];
    const auto & mangoTradingPair = _mangoReferenceData.tradingPairs[ order.trading_pair_index( ) ];

    const auto mangoTradingPairIndex = mangoTradingPair.mangoTradingPairIndex;
    const auto mangoBaseCurrencyIndex = mangoTradingPair.mangoBaseCurrencyIndex;
    const auto mangoQuoteCurrencyIndex = mangoTradingPair.mangoQuoteCurrencyIndex;

    const auto & mangoBaseCurrency = _mangoReferenceData.currencies[ mangoBaseCurrencyIndex ];
    const auto & mangoQuoteCurrency = _mangoReferenceData.currencies[ mangoQuoteCurrencyIndex ];

    const auto baseCurrencyIndex = serumTradingPair.baseCurrencyIndex;
    const auto quoteCurrencyIndex = serumTradingPair.quoteCurrencyIndex;

    const auto & serumBaseCurrency = _serumReferenceData.currencies[ baseCurrencyIndex ];
    const auto & serumQuoteCurrency = _serumReferenceData.currencies[ quoteCurrencyIndex ];

    std::vector< Core::KeyPairRef > signingWritableKeyPairs
    {
        _mangoOwnerKey // 3 ( 0 )
    };

    std::vector< Core::PublicKeyRef > writablePublicKeys
    {
        &_mangoReferenceData.mangoAccountAddress, // 2 ( 1 )
        &serumTradingPair.serumMarketAddress, // 6 ( 2 )
        &serumTradingPair.serumMarketAccount.bids, // 7 ( 3 )
        &serumTradingPair.serumMarketAccount.asks, // 8 ( 4 )
        &serumTradingPair.serumMarketAccount.requestQueue, // 9 ( 5 )
        &serumTradingPair.serumMarketAccount.eventQueue, // 10 ( 6 )
        &serumTradingPair.serumMarketAccount.baseVault, // 11 ( 7 )
        &serumTradingPair.serumMarketAccount.quoteVault, // 12 ( 8 )
        &mangoBaseCurrency.nodeBankAddress, // 14 ( 9 )
        &mangoBaseCurrency.nodeBank.vaultAddress, // 15 ( 10 )
        &mangoQuoteCurrency.nodeBankAddress, // 17 ( 11 )
        &mangoQuoteCurrency.nodeBank.vaultAddress, // 18 ( 12 )
        &_mangoReferenceData.mangoAccount.openOrdersAddresses[ mangoTradingPairIndex ] // 23 ( 13 )
    };

    Core::PublicKey dexSigner;
    dexSigner.create_program_address
    (
        _mangoReferenceData.mangoGroup.spotMarkets[ mangoTradingPairIndex ].spotMarketAddress,
        _serumReferenceData.serumProgramId,
        serumTradingPair.serumMarketAccount.vaultSignerNonce
    );

    std::vector< Core::PublicKeyRef > readOnlyPublicKeys
    {
        &_mangoReferenceData.mangoAccount.mangoGroupAddress, // 1 ( 14 )
        &_mangoReferenceData.mangoGroup.mangoCache, // 4 ( 15 )
        &_mangoReferenceData.mangoGroup.dexProgramId, // 5 ( 16 )
        &_mangoReferenceData.mangoGroup.tokens[ mangoBaseCurrencyIndex ].rootBankAddress,// 13 ( 17 )
        &_mangoReferenceData.mangoGroup.tokens[ mangoQuoteCurrencyIndex ].rootBankAddress, // 16 ( 18 )
        _splTokenProgram, // 19 ( 19 )
        &_mangoReferenceData.mangoGroup.signerKey, // 20 ( 20 )
        &dexSigner, // 21 ( 21 )
        &_mangoReferenceData.mangoGroup.msrmVault, // 22 ( 22 )
        &_mangoReferenceData.mangoProgramId // programId ( 23 )
    };
    std::vector< uint8_t > accountIndices = { 14, 1, 0, 15, 16, 2, 3, 4, 5, 6, 7, 8, 17, 9, 10, 18, 11, 12, 19, 20, 21, 22, 13 };

    size_t accountIndexIt = 24;
    for ( size_t i = 0; i < _mangoAccount.inMarginBasket.size( ); ++i )
    {
        if ( _mangoReferenceData.mangoAccount.inMarginBasket[ i ] && i != mangoTradingPairIndex )
        {
            readOnlyPublicKeys.emplace_back( &_mangoReferenceData.mangoAccount.openOrdersAddresses[ i ] );
            accountIndices.emplace_back( accountIndexIt++ );
        }
    }

    auto serumPrice =
        static_cast< uint64_t >( order.price( ) * boost::multiprecision::pow( Trading::Price( 10 ), serumQuoteCurrency.mintAccount.decimals ) )
        / serumTradingPair.serumMarketAccount.quoteLotSize;
    auto serumQuantity =
        static_cast< uint64_t >( order.quantity( ) * boost::multiprecision::pow( Trading::Quantity( 10 ), serumBaseCurrency.mintAccount.decimals ) )
        / serumTradingPair.serumMarketAccount.baseLotSize;
    auto maxQuoteQuantity =
        static_cast< uint64_t >
        (
            ( ( ( 1 + Serum::serum_taker_fee( ) ) * order.price( ) * boost::multiprecision::pow( Trading::Price( 10 ), serumQuoteCurrency.mintAccount.decimals ) )
            / serumTradingPair.serumMarketAccount.quoteLotSize )
            *
            ( ( order.quantity( ) * boost::multiprecision::pow( Trading::Quantity( 10 ), serumBaseCurrency.mintAccount.decimals ) )
            / serumTradingPair.serumMarketAccount.baseLotSize )
        ); // TODO implement getFeeTier/getFeeRate
    
    SYNTHFI_LOG_INFO( _logger ) << fmt::format( "price: {}, quantity: {}, maxQuoteQty: {}", serumPrice, serumQuantity, maxQuoteQuantity );

    // Set client order id, use current timestamp as unique id.
    order.client_order_id( ) = _clock.now( ).time_since_epoch( ).count( );

    PlaceSpotOrderInstruction orderInstruction
    (
        23, // programId
        Solana::DynamicAccountIndexArray( std::move( accountIndices ) ),
        order.side( ),
        serumPrice,
        serumQuantity,
        order.order_type( ),
        maxQuoteQuantity,
        order.client_order_id( )
    );

    SendPlaceSpotOrderTransaction transaction
    (
        _recentBlockhash,
        std::move( signingWritableKeyPairs ),
        { },
        std::move( writablePublicKeys ),
        std::move( readOnlyPublicKeys ),
        std::move( orderInstruction )
    );

    auto pendingPlaceSpotOrderResponse = _solanaHttpClient.send_request
    <
        SendPlaceSpotOrderTransactionRequest,
        Solana::SendTransactionResponse
    >
    (
        { std::move( transaction ) }
    );

    // Copy order for logging.
    co_spawn( _strand, publish_order_statistic( order ), asio::detached );

    // Wait for RPC response.
    auto placeSpotOrderResponse = co_await std::move( pendingPlaceSpotOrderResponse );

    auto transactionResult = co_await _signatureSubscriber.signature_subscribe
    (
        placeSpotOrderResponse.transactionSignature, Solana::Commitment::processed // TODO: maybe should be confirmed, idk.
    );

    order.order_state( ) = Trading::OrderState::CLOSED;

    co_spawn( _strand, publish_order_statistic( order ), asio::detached );

    co_return order;
}

asio::awaitable< void > MangoOrderClientImpl::publish_order_statistic( Trading::Order order )
{
    Statistics::InfluxMeasurement orderMeasurement;
    orderMeasurement.measurementName = "order";
    orderMeasurement.tags = 
        {
            { "source", "mango" },
            { "trading_pair_index", std::to_string( order.trading_pair_index( ) ) }
        };

    orderMeasurement.fields =
        {
            { "price", Statistics::InfluxDataType( order.price( ) ) },
            { "quantity", Statistics::InfluxDataType( order.quantity( ) ) },
            { "side", std::string( magic_enum::enum_name( order.side( ) ) ) },
            { "order_type", std::string( magic_enum::enum_name( order.order_type( ) ) ) },
            { "client_order_id", Statistics::InfluxDataType( order.client_order_id( ) ) },
            { "order_state", Statistics::InfluxDataType( Trading::order_state_to_lower( order.order_state( ) ) ) },
            { "average_fill_price", Statistics::InfluxDataType( order.average_fill_price( ) ) },
            { "fill_quantity", Statistics::InfluxDataType( order.fill_quantity( ) ) }
        };

    co_await _statisticsPublisher.publish_statistic( std::move( orderMeasurement ) );

    co_return;
}

asio::awaitable< Trading::Order > MangoOrderClientImpl::do_cancel_order
(
    Trading::Order order
)
{
    const auto & serumTradingPair = _serumReferenceData.tradingPairs[ order.trading_pair_index( ) ];
    const auto & mangoTradingPair = _mangoReferenceData.tradingPairs[ order.trading_pair_index( ) ];

    const auto mangoTradingPairIndex = mangoTradingPair.mangoTradingPairIndex;
    const auto mangoBaseCurrencyIndex = mangoTradingPair.mangoBaseCurrencyIndex;
    const auto mangoQuoteCurrencyIndex = mangoTradingPair.mangoQuoteCurrencyIndex;

    const auto & mangoBaseCurrency = _mangoReferenceData.currencies[ mangoBaseCurrencyIndex ];
    const auto & mangoQuoteCurrency = _mangoReferenceData.currencies[ mangoQuoteCurrencyIndex ];

    const auto baseCurrencyIndex = serumTradingPair.baseCurrencyIndex;
    const auto quoteCurrencyIndex = serumTradingPair.quoteCurrencyIndex;

    const auto & serumBaseCurrency = _serumReferenceData.currencies[ baseCurrencyIndex ];
    const auto & serumQuoteCurrency = _serumReferenceData.currencies[ quoteCurrencyIndex ];

    std::array< Core::KeyPairRef, 1 > signingWritableKeyPairs
    {
        _mangoOwnerKey // 4 - cancel, 2 - settle ( 0 )
    };

    std::array< Core::PublicKeyRef, 13 > writablePublicKeys
    {
        &_mangoReferenceData.mangoAccountAddress, // 3 - settle ( 1 )
        &serumTradingPair.serumMarketAddress, // 0 - cancel, 5 - settle ( 2 )
        &serumTradingPair.serumMarketAccount.bids, // 1 - cancel, 6 - settle ( 3 )
        &serumTradingPair.serumMarketAccount.asks, // 2 - cancel, 7 - settle ( 4 )
        &serumTradingPair.serumMarketAccount.requestQueue, // 9 ( 5 )
        &serumTradingPair.serumMarketAccount.eventQueue, // 5 - cancel ( 6 )
        &serumTradingPair.serumMarketAccount.baseVault, // 8 - settle ( 7 )
        &serumTradingPair.serumMarketAccount.quoteVault, // 9 - settle ( 8 )
        &mangoBaseCurrency.nodeBankAddress, // 11 - settle ( 9 )
        &mangoBaseCurrency.nodeBank.vaultAddress, // 14 - settle ( 10 )
        &mangoQuoteCurrency.nodeBankAddress, // 13 - settle ( 11 )
        &mangoQuoteCurrency.nodeBank.vaultAddress, // 15 - settle ( 12 )
        &_mangoReferenceData.mangoAccount.openOrdersAddresses[ mangoTradingPairIndex ] // 3 - cancel, 6 - settle ( 13 )
    };

    Core::PublicKey dexSigner;
    dexSigner.create_program_address
    (
        _mangoReferenceData.mangoGroup.spotMarkets[ mangoTradingPairIndex ].spotMarketAddress,
        _serumReferenceData.serumProgramId,
        serumTradingPair.serumMarketAccount.vaultSignerNonce
    );

    std::array< Core::PublicKeyRef, 10 > readOnlyPublicKeys
    {
        &_mangoReferenceData.mangoAccount.mangoGroupAddress, // 0 - settle ( 14 )
        &_mangoReferenceData.mangoGroup.mangoCache, // 1 - settle ( 15 )
        &_mangoReferenceData.mangoGroup.dexProgramId, // 4 - settle ( 16 )
        &_mangoReferenceData.mangoGroup.tokens[ mangoBaseCurrencyIndex ].rootBankAddress,// 10 - settle ( 17 )
        &_mangoReferenceData.mangoGroup.tokens[ mangoQuoteCurrencyIndex ].rootBankAddress, // 12 - settle ( 18 )
        _splTokenProgram, // 17 - settle ( 19 )
        &_mangoReferenceData.mangoGroup.signerKey, // 7 - settle ( 20 )
        &dexSigner, // 16 - settle ( 21 )
        &_mangoReferenceData.mangoGroup.msrmVault, // 22 ( 22 )
        &_mangoReferenceData.mangoProgramId // programId ( 23 )
    };

    auto serumPrice =
        static_cast< uint64_t >( order.price( ) * boost::multiprecision::pow( Trading::Price( 10 ), serumQuoteCurrency.mintAccount.decimals ) )
        / serumTradingPair.serumMarketAccount.quoteLotSize;

    CancelSpotOrderInstruction
    <
        23, // mango program
        2, // market
        3, // bids
        4, // asks
        13, // open orders
        0, // owner
        6 // event queue
    > cancelOrderInstruction
    (
        order.side( ),
        order.order_id( ), // upper 64 bits
        serumPrice // lower 64 bits
    );

    SettleFundsInstruction
    <
        23, // mango program
        14, // mango group
        15, // mango cache
        0, // owner
        3, // mango account
        16, // dex program id
        13, // open orders
        20, // mango group signer key
        7, // base vault
        8, // quote vault
        17, // base root bank
        9, // base node bank
        18, // quote root bank
        11, // quote node bank
        10, // base vault
        12, // quote vault
        21, // dex signer
        19 // settle
    > settleFundsInstruction;

    Solana::Transaction cancelOrderTransaction
    (
        _recentBlockhash,
        signingWritableKeyPairs,
        std::array< Core::KeyPairRef, 0 >{ },
        writablePublicKeys,
        readOnlyPublicKeys,
        cancelOrderInstruction,
        settleFundsInstruction
    );

    auto pendingCancelOrderInstruction = _solanaHttpClient.send_request
    <
        Solana::SendTransactionRequest< decltype( cancelOrderTransaction ) >,
        Solana::SendTransactionResponse
    >
    (
        { std::move( cancelOrderTransaction ) }
    );

    co_return order;
}

} // namespace Mango
} // namespace Synthfi

#include "synthfi/Solana/AccountBatcher/AccountBatcherImpl.hpp"

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/redirect_error.hpp>

using namespace std::chrono_literals;

namespace asio = boost::asio;

namespace Synthfi
{
namespace Solana
{

AccountBatcherImpl::AccountBatcherImpl
(
    asio::io_context & ioContext,
    SolanaHttpClientService & solanaHttpClientService,
    size_t multipleAccountsBatchSize
)
    : _multipleAccountsBatchSize( multipleAccountsBatchSize )
    , _strand( ioContext.get_executor( ) )
    , _solanaHttpClient( solanaHttpClientService )
{ }

asio::awaitable< GetAccountInfoResponse > AccountBatcherImpl::do_get_account( GetAccountInfoRequest request )
{
    co_return co_await _solanaHttpClient.send_request< GetAccountInfoRequest, GetAccountInfoResponse >( std::move( request ) );
}

void AccountBatcherImpl::on_multiple_accounts_batch( uint64_t batchId, size_t batchIndex, std::exception_ptr error, GetMultipleAccountsResponse batchResponse )
{
    co_spawn( _strand, do_handle_multiple_account_batch( batchId, batchIndex, error, std::move( batchResponse ) ), boost::asio::detached );
}

asio::awaitable< GetMultipleAccountsResponse > AccountBatcherImpl::do_get_multiple_accounts( GetMultipleAccountsRequest request )
{
    if ( request.accountPublicKeys.empty( ) )
    {
        co_return GetMultipleAccountsResponse{ };
    }

    auto batchId = _nextBatchId++;

    auto batchDivison = std::lldiv( request.accountPublicKeys.size( ), _multipleAccountsBatchSize );
    auto requestCount = batchDivison.quot + ( batchDivison.rem > 0 ? 1 : 0 );

    SYNTHFI_LOG_DEBUG( _logger )
        << fmt::format
        (
            "[{}] Batching multiple account request of count: {}, into requests count: {}, batch id: {}",
            name( ),
            request.accountPublicKeys.size( ),
            requestCount,
            batchId
        );

    auto & batch = _multipleAccountBatches.emplace
    (
        batchId,
        MultipleAccountBatch( _strand, 30s, requestCount, request.accountPublicKeys.size( ) )
    )
    .first->second;

    size_t batchIndex = 0;
    while ( batchIndex < request.accountPublicKeys.size( ) )
    {
        size_t batchRequestSize = std::min( _multipleAccountsBatchSize, request.accountPublicKeys.size( ) - batchIndex );
        GetMultipleAccountsRequest batchRequest{ std::vector< Core::PublicKey >( batchRequestSize ) };
        std::copy
        (
            request.accountPublicKeys.begin( ) + batchIndex,
            request.accountPublicKeys.begin( ) + batchIndex + batchRequestSize,
            batchRequest.accountPublicKeys.begin( )
        );
        asio::co_spawn
        (
            _strand,
            _solanaHttpClient.send_request< GetMultipleAccountsRequest, GetMultipleAccountsResponse >( std::move( batchRequest ) ),
            std::bind
            (
                &AccountBatcherImpl::on_multiple_accounts_batch,
                this,
                batchId,
                batchIndex,
                std::placeholders::_1,
                std::placeholders::_2
            )
        );

        batchIndex += _multipleAccountsBatchSize;
    }

    boost::system::error_code errorCode;
    co_await batch.expiryTimer.async_wait( asio::redirect_error( asio::use_awaitable, errorCode ) );

    if ( errorCode == asio::error::operation_aborted )
    {
        auto findBatch = _multipleAccountBatches.find( batchId );

        if ( findBatch->second.error )
        {
            auto error = findBatch->second.error;
            _multipleAccountBatches.erase( findBatch );
            std::rethrow_exception( error );
        }

        auto response = findBatch->second.response;
        _multipleAccountBatches.erase( findBatch );
        co_return response;
    }
    else if ( errorCode )
    {
        throw SynthfiError( errorCode.what( ) );
    }

    throw SynthfiError( "Timed out ");
}

asio::awaitable< void > AccountBatcherImpl::do_handle_multiple_account_batch
(
    uint64_t batchId,
    size_t batchIndex,
    std::exception_ptr error,
    GetMultipleAccountsResponse batchResponse
)
{
    const auto & findBatch = _multipleAccountBatches.find( batchId );
    if ( findBatch == _multipleAccountBatches.end( ) )
    {
        SYNTHFI_LOG_ERROR( _logger ) << "Missing batch id: " << batchId;
        co_return;
    }

    if ( error )
    {
        findBatch->second.error = error;
    }
    else
    {
        if ( batchResponse.accountInfos.size( ) + batchIndex > findBatch->second.response.accountInfos.size( ) )
        {
            SYNTHFI_LOG_ERROR( _logger ) << "Invalid GetMultipleAcccounts batch response size: " << batchResponse.accountInfos.size( );
            findBatch->second.error = std::make_exception_ptr( SynthfiError( "Invalid GetMultipleAccountsResponse size") );
        }
        else
        {
            std::copy
            (
                batchResponse.accountInfos.begin( ),
                batchResponse.accountInfos.end( ),
                findBatch->second.response.accountInfos.begin( ) + batchIndex
            );
        }
    }

    if ( --findBatch->second.pendingBatches == 0 )
    {
        findBatch->second.expiryTimer.cancel( );
    }

    co_return;
}

} // namespace Solana
} // namespace Synthfi

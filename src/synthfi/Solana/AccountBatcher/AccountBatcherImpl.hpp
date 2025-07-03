#pragma once

#include "synthfi/Solana/SolanaHttpClient/SolanaHttpClient.hpp"
#include "synthfi/Solana/SolanaHttpMessage.hpp"

#include <boost/asio/awaitable.hpp>

namespace Synthfi
{
namespace Solana
{

class AccountBatcherImpl
{
public:
    AccountBatcherImpl
    (
        boost::asio::io_context & ioContext,
        SolanaHttpClientService & solanaHttpClientService,
        size_t multipleAccountsBatchSize
    );

    template< class CompletionHandlerType >
    void get_multiple_accounts( GetMultipleAccountsRequest request, CompletionHandlerType completionHandler )
    {
        co_spawn( _strand, do_get_multiple_accounts( std::move( request ) ), completionHandler );
    }

    template< class CompletionHandlerType >
    void get_account( GetAccountInfoRequest request, CompletionHandlerType completionHandler )
    {
        co_spawn( _strand, do_get_account( std::move( request ) ), completionHandler );
    }

    constexpr std::string name( ) const & { return "AccountBatcher"; }

private:
    boost::asio::awaitable< GetMultipleAccountsResponse > do_get_multiple_accounts( GetMultipleAccountsRequest request );
    boost::asio::awaitable< GetAccountInfoResponse > do_get_account( GetAccountInfoRequest request );

    void on_multiple_accounts_batch( uint64_t batchId, size_t batchIndex, std::exception_ptr error, GetMultipleAccountsResponse batchResponse );
    boost::asio::awaitable< void > do_handle_multiple_account_batch
    (
        uint64_t batchId,
        size_t batchIndex,
        std::exception_ptr error,
        GetMultipleAccountsResponse batchResponse
    );

    struct MultipleAccountBatch
    {
        MultipleAccountBatch
        (
            boost::asio::strand< boost::asio::io_context::executor_type > & strand,
            std::chrono::seconds timeout,
            size_t pendingBatches,
            size_t requestCount
        )
            : expiryTimer( strand, timeout )
            , pendingBatches( pendingBatches )
            , response( std::vector< AccountInfo >( requestCount ) )
        { }

        std::exception_ptr error;
        boost::asio::high_resolution_timer expiryTimer;

        uint64_t pendingBatches;
        GetMultipleAccountsResponse response;
    };

    size_t _multipleAccountsBatchSize;

    boost::asio::strand< boost::asio::io_context::executor_type > _strand;
    SolanaHttpClient _solanaHttpClient;

    uint64_t _nextBatchId = 0;
    std::unordered_map< uint64_t, MultipleAccountBatch > _multipleAccountBatches;

    SynthfiLogger _logger;
};

} // namespace Solana
} // namespace Synthfi

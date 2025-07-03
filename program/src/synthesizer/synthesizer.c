/*
 * BPF pyth price oracle program
 */
#include <solana_sdk.h>
#include "synthesizer.h"

static bool valid_funding_account( SolAccountInfo *ka )
{
    return ka->is_signer &&
         ka->is_writable;
}

static bool valid_signable_account( SolParameters *prm,
                                    SolAccountInfo *ka,
                                    uint64_t dlen )
{
    sol_log_64(ka->is_signer,
         ka->is_writable,
         SolPubkey_same( ka->owner, prm->program_id ),
         ka->data_len, dlen );
    return ka->is_signer &&
         ka->is_writable &&
         SolPubkey_same( ka->owner, prm->program_id ) &&
         ka->data_len >= dlen;
}

static bool valid_writable_account( SolParameters *prm,
                                    SolAccountInfo *ka,
                                    uint64_t dlen )
{
  return ka->is_writable &&
         SolPubkey_same( ka->owner, prm->program_id ) &&
         ka->data_len >= dlen;
}

static bool valid_readable_account( SolParameters *prm,
                                    SolAccountInfo *ka,
                                    uint64_t dlen )
{
  return SolPubkey_same( ka->owner, prm->program_id ) &&
         ka->data_len >= dlen;
}

static uint64_t init_synthfi( SolParameters *prm, SolAccountInfo *ka )
{
    sol_log( "Initializing synthfi" );
    // Verify that the new account is signed and writable, with correct
    // ownership and size

    if ( prm->ka_num < 2 ||
        !valid_funding_account( &ka[0] ) ||
        !valid_signable_account( prm, &ka[1], sizeof( synthfi_product_table_t ) ) )
    {
        return ERROR_INVALID_ARGUMENT;
    }

    // Check that the account has not already been initialized
    synthfi_product_table_t *tptr = (synthfi_product_table_t*)ka[1].data;
    if ( tptr->magic != 0 || tptr->version != 0 )
    {
        return ERROR_INVALID_ARGUMENT;
    }

    // Initialize by setting to zero again (just in case) and setting
    // the version number
    command_header_t *hdr = (command_header_t*)prm->data;
    sol_memset( tptr, 0, sizeof( synthfi_product_table_t ) );
    tptr->magic = SYNTHFI_MAGIC;
    tptr->version = hdr->version;
    tptr->type  = SYNTHFI_ACCTYPE_TABLE;
    tptr->size  = sizeof( synthfi_product_table_t ) - sizeof( tptr->products );

    return SUCCESS;
}

static uint64_t init_product( SolParameters * prm, SolAccountInfo * ka)
{
    sol_log( "Initializing product" );
    // Account (1) is the synthfi account that we're going to add to and
    // Account (2) is the new product account
    // Verify that these are signed, writable accounts with correct ownership
    // and size

    if ( prm->ka_num < 3 ||
         !valid_funding_account( &ka[0] ) ||
         !valid_signable_account( prm, &ka[1], sizeof( synthfi_product_table_t ) ) ||
         !valid_signable_account( prm, &ka[2], sizeof( synthfi_product_t ) ) )
    {
        return ERROR_INVALID_ARGUMENT;
    }

    // Verify that mapping account is a valid  account
    // that the new product account is uninitialized and that there is space
    // in the mapping account
    command_header_t * header = ( command_header_t * )prm->data;
    synthfi_product_table_t *tptr = ( synthfi_product_table_t * )ka[1].data;
    synthfi_product_t *sptr = (synthfi_product_t * )ka[2].data;
    if ( tptr->magic != SYNTHFI_MAGIC ||
        tptr->version != header->version ||
        tptr->type != SYNTHFI_ACCTYPE_TABLE ||
        sptr->magic != 0 ||
        tptr->productCount >= SYNTHFI_MAX_PRODUCTS )
    {
        return ERROR_INVALID_ARGUMENT;
    }

    // Initialize product account
    sol_memset( sptr, 0, sizeof( synthfi_product_t ) );
    sptr->magic = SYNTHFI_MAGIC;
    sptr->version = header->version;
    sptr->type  = SYNTHFI_ACCTYPE_PRODUCT;
    sptr->size  = sizeof( synthfi_product_t );

    // finally add synthfi account link
    synthfi_pub_key_assign( &tptr->products[ tptr->productCount++ ], ( synthfi_pub_key_t * )ka[2].key );
    tptr->size  = sizeof( synthfi_product_table_t ) - sizeof( tptr->products ) +
        ( tptr->productCount * sizeof( synthfi_pub_key_t ) );

    sol_log_64( tptr->productCount, sptr->size, tptr->size, sptr->magic, *(uint64_t*)&ka[2] );

    return SUCCESS;
}

static uint64_t init_deposit_account( SolParameters * prm, SolAccountInfo * ka)
{
    // Account (1) is the product account that we're going to add to and
    // Account (2) is the new deposit account
    // Verify that these are signed, writable accounts with correct ownership
    // and size

    if ( prm->ka_num < 3 ||
        !valid_funding_account( &ka[0] ) ||
        !valid_signable_account( prm, &ka[1], sizeof( synthfi_product_t ) ) ||
        !valid_signable_account( prm, &ka[2], sizeof( synthfi_deposit_account_t ) ) )
        {
            return ERROR_INVALID_ARGUMENT;
        }

    // Verify that product account is a valid account
    // that the new product account is uninitialized and that there is space
    // in the mapping account
    command_header_t * header = ( command_header_t * )prm->data;
    synthfi_product_t *tptr = ( synthfi_product_t * )ka[1].data;
    synthfi_deposit_account_t *sptr = (synthfi_deposit_account_t * )ka[2].data;
    if ( tptr->magic != SYNTHFI_MAGIC ||
        tptr->version != header->version ||
        tptr->type != SYNTHFI_ACCTYPE_PRODUCT ||
        sptr->magic != 0 ||
        tptr->depositAccountCount >= SYNTHFI_MAX_DEPOSIT_ACCTS )
    {
        return ERROR_INVALID_ARGUMENT;
    }

    // Initialize product account
    sol_memset( sptr, 0, sizeof( synthfi_deposit_account_t ) );
    sptr->magic = SYNTHFI_MAGIC;
    sptr->version = header->version;
    sptr->type  = SYNTHFI_ACCTYPE_DEPOSIT;
    sptr->size  = sizeof( synthfi_deposit_account_t );

    // finally add synthfi account link
    synthfi_pub_key_assign( &tptr->depositAccounts[ tptr->depositAccountCount++ ], ( synthfi_pub_key_t * )ka[2].key );
    tptr->size  = sizeof( synthfi_deposit_account_t ) - sizeof( tptr->depositAccounts ) +
        ( tptr->depositAccountCount * sizeof( synthfi_pub_key_t ) );

    return SUCCESS;
}

static uint64_t synthesize_synthfi_product( SolParameters * prm, SolAccountInfo * ka)
{
    return SUCCESS;
}

static uint64_t redeem_synthfi_product( SolParameters * prm, SolAccountInfo * ka)
{
    return SUCCESS;
}

static uint64_t dispatch_1( SolParameters * prm, SolAccountInfo * ka )
{
    sol_log( "dispatch 1" );
    command_header_t * header = ( command_header_t * )prm->data;
    switch( header->command )
    {
        case SYNTHFI_INIT: return init_synthfi( prm, ka );
        case SYNTHFI_ADD_PRODUCT: return init_product( prm, ka );
        case SYNTHFI_ADD_DEPOSIT_ACCT: return init_deposit_account( prm, ka );
        case SYNTHFI_SYNTHESIZE: return synthesize_synthfi_product( prm, ka );
        case SYNTHFI_REDEEM: return redeem_synthfi_product( prm, ka );
        break;
    }

    return ERROR_INVALID_ARGUMENT;
}

static uint64_t dispatch( SolParameters *prm, SolAccountInfo *ka )
{
    sol_log( "dispatch" );
    if (prm->data_len < sizeof(command_header_t) )
    {
        return ERROR_INVALID_ARGUMENT;
    }

    command_header_t * header = ( command_header_t * )prm->data;
    switch( header->version )
    {
        case SYNTHFI_VERSION_1: return dispatch_1( prm, ka );
        default: break;
    }
    return ERROR_INVALID_ARGUMENT;
}

extern uint64_t entrypoint(const uint8_t *input)
{
    sol_log( "entry" );
    SolAccountInfo ka[3];
    SolParameters prm[1];
    prm->ka = ka;
    if (! sol_deserialize( input, prm, SOL_ARRAY_SIZE( ka )) )
    {
        return ERROR_INVALID_ARGUMENT;
    }
    return dispatch( prm, ka );
}

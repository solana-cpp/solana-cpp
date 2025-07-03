#pragma once


#ifdef __cplusplus
extern "C" {
#endif

// magic number at head of account
#define SYNTHFI_MAGIC 0x464e5953

// versions available
#define SYNTHFI_VERSION_1 1U

// latest program version
#define SYNTHFI_VERSION SYNTHFI_VERSION_1

// various size constants
#define SYNTHFI_PUBKEY_SIZE       32
#define SYNTHFI_PUBKEY_SIZE_64    ( SYNTHFI_PUBKEY_SIZE / sizeof( uint64_t ) )

#define SYNTHFI_MAX_PRODUCTS      8
#define SYNTHFI_MAX_DEPOSIT_ACCTS 64

// account types
#define SYNTHFI_ACCTYPE_TABLE     1
#define SYNTHFI_ACCTYPE_PRODUCT   2
#define SYNTHFI_ACCTYPE_DEPOSIT   3

// binary version of sysvar_clock account id
const uint64_t sysvar_clock[] =
{
  0xc974c71817d5a706UL,
  0xb65e1d6998635628UL,
  0x5c6d4b9ba3b85e8bUL,
  0x215b5573UL
};

// public key of symbol or liquidator account
typedef union synthfi_pub_key
{
    uint8_t  k1 [SYNTHFI_PUBKEY_SIZE];
    uint64_t k8 [SYNTHFI_PUBKEY_SIZE_64];
} synthfi_pub_key_t;
static_assert( sizeof( synthfi_pub_key_t ) == SYNTHFI_PUBKEY_SIZE , "synthfi_pub_key invalid size" );

// account header information
typedef struct synthfi_account
{
    uint32_t magic; // synthfi magic number
    uint32_t version; // program/account version
    uint32_t type; // account type
    uint32_t size; // size of populated region of account
} synthfi_account_t;

// hash table of symbol to price account mappings
typedef struct synthfi_product_table
{
    uint32_t magic; // synthfi magic number
    uint32_t version; // program/account version
    uint32_t type; // account type
    uint32_t size; // size of populated region of account
    uint32_t productCount; // number of products
    uint32_t _unused; // 64bit padding

    synthfi_pub_key_t products[ SYNTHFI_MAX_PRODUCTS ]; // product accounts
} synthfi_product_table_t;

// synthfi product account
typedef struct synthfi_product
{
    uint32_t magic; // synthfi magic number
    uint32_t version; // program version
    uint32_t type; // account type
    uint32_t size; // size of populated region of account

    uint8_t productDecimals;
    uint8_t oracleDecimals;
    uint16_t depositAccountCount;
    uint32_t _unused2;

    uint32_t synthesizeCollateralRatio;
    uint32_t liquidateCollateralRatio;

    uint64_t totalDeposited;
    uint64_t totalSynthesized;

    char productName[16];

    synthfi_pub_key_t mint; // synthetic product token mint address
    synthfi_pub_key_t oracle; // reference price for product
    synthfi_pub_key_t referenceData; // reference data for product
    synthfi_pub_key_t depositAccounts[ SYNTHFI_MAX_DEPOSIT_ACCTS ];
} synthfi_product_t;

typedef struct synthfi_deposit_account
{
    uint32_t magic; // synthfi magic number
    uint32_t version; // program version
    uint32_t type; // account type
    uint32_t size; // size of populated region of account

    uint64_t deposited;
    uint64_t synthesized;

    synthfi_pub_key_t product; // synthetic product token mint address
    synthfi_pub_key_t owner; // synthetic product token mint address

} synthfi_deposit_account_t;

// variable length string
typedef struct synthfi_str
{
    uint8_t len;
    char data[1];
} synthfi_str_t;

// command enumeration
typedef enum {
    // initialize first mapping list account
    SYNTHFI_INIT = 0,
    SYNTHFI_ADD_PRODUCT = 1,
    SYNTHFI_ADD_DEPOSIT_ACCT = 2,
    SYNTHFI_SYNTHESIZE = 3,
    SYNTHFI_REDEEM = 4
} command_t;

typedef struct command_header
{
    uint32_t version;
    int32_t command;
} command_header_t;

// structure of clock sysvar account
typedef struct sysvar_clock
{
    uint64_t    slot;
    int64_t     epoch_start_timestamp;
    uint64_t    epoch;
    uint64_t    leader_schedule_epoch;
    int64_t     unix_timestamp;
} sysvar_clock_t;

// compare if two pub_keys (accounts) are the same
inline bool synthfi_pub_key_equal( synthfi_pub_key_t *p1, synthfi_pub_key_t *p2 )
{
    return p1->k8[0] == p2->k8[0] &&
        p1->k8[1] == p2->k8[1] &&
        p1->k8[2] == p2->k8[2] &&
        p1->k8[3] == p2->k8[3];
}

// check for null (zero) public key
inline bool synthfi_pub_key_is_zero( synthfi_pub_key_t *p )
{
    return p->k8[0] == 0UL &&
        p->k8[1] == 0UL &&
        p->k8[2] == 0UL &&
        p->k8[3] == 0UL;
}

// set public key to zero
inline void synthfi_pub_key_set_zero( synthfi_pub_key_t *p )
{
    p->k8[0] = p->k8[1] = p->k8[2] = p->k8[3] = 0UL;
}

// assign one pub_key from another
inline void synthfi_pub_key_assign( synthfi_pub_key_t *tgt, synthfi_pub_key_t *src )
{
    tgt->k8[0] = src->k8[0];
    tgt->k8[1] = src->k8[1];
    tgt->k8[2] = src->k8[2];
    tgt->k8[3] = src->k8[3];
}

#ifdef __cplusplus
}
#endif

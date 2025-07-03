#pragma once

#include "synthfi/Core/KeyPair.hpp"
#include "synthfi/Util/StringEncode.hpp"
#include "synthfi/Util/Logger.hpp"

#include <boost/assert.hpp>
#include <boost/json/serialize.hpp>

#include <fmt/format.h>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <fmt/ostream.h>

#include <ranges>

#pragma GCC diagnostic push "-Wpadded"

namespace Synthfi
{
namespace Solana
{

static constexpr size_t solana_max_transaction_size( ) { return 1232; }

// A compact-u16 is a multi-byte encoding of 16 bits. The first byte contains the lower 7 bits of the value in its lower 7 bits.
// If the value is above 0x7f, the high bit is set and the next 7 bits of the value are placed into the lower 7 bits of a second byte.
// If the value is above 0x3fff, the high bit is set and the remaining 2 bits of the value are placed into the lower 2 bits of a third byte.

// TODO: 3 byte compact-u16.
template< uint16_t value, class = std::enable_if_t< value < 0x80 > >
static constexpr std::array< std::byte, 1 > compact_u16( )
{
    if ( value < 0x80 )
    {
        return { std::byte{ value } };
    }
}

template< uint16_t value, class = std::enable_if_t< value >= 0x80 > >
static constexpr std::array< std::byte, 2 > compact_u16( )
{
    return { static_cast< std::byte >( 0x80 | ( value & 0x7f ) ), static_cast< std::byte >( value >> 7 & 0x7f ) };
}

static auto encode_compact_u16( uint16_t value, std::span< std::byte > output )
{
    auto it = output.begin( );
    if ( value < 0x80 )
    {
        *(it++) = static_cast< std::byte >( value );
    }
    else
    {
        *(it++) = static_cast< std::byte >( 0x80 | ( value & 0x7f ) );
        *(it++) = static_cast< std::byte >( value >> 7 & 0x7f );
    }
    return it; 
}

static std::vector< std::byte > dynamic_compact_u16( uint16_t value )
{
    if ( value < 0x80 )
    {
        return { static_cast< std::byte >( value ) };
    }
    return { static_cast< std::byte >( 0x80 | ( value & 0x7f ) ), static_cast< std::byte >( value >> 7 & 0x7f ) };
}

static uint8_t sizeof_compact_u16( uint16_t value )
{
    if ( value < 0x80 )
    {
        return 1;
    }
    return 2;
}

template< class T, uint16_t Length >
class CompactArray
{
public:
    constexpr CompactArray( ) : _length( compact_u16< Length >( ) )
    {
        static_assert( sizeof( *this ) == sizeof( _length ) + sizeof( _data ), "Invalid CompactArray size" );
    }

    constexpr uint16_t length( ) const { return Length; }

    constexpr T * begin( ) { return reinterpret_cast< T * >( _data.begin( ) ); }
    constexpr T * end( ) { return reinterpret_cast< T * >( _data.end( ) ); }

    constexpr const T * begin( ) const { return reinterpret_cast< const T * >( _data.begin( ) ); }
    constexpr const T * end( ) const { return reinterpret_cast< const T * >( _data.end( ) ); }

private:
    std::array< std::byte, sizeof( decltype( compact_u16< Length >( ) ) ) > _length;
    std::array< std::byte, sizeof( T ) * Length > _data;
};

template< class T >
class DynamicCompactArray
{
public:
    explicit DynamicCompactArray( uint16_t length )
        : _length( length )
        , _data( sizeof_compact_u16( length ) + ( sizeof( T ) * length ) )
    {
        auto encodedLength = dynamic_compact_u16( _length );
        std::copy( encodedLength.begin( ), encodedLength.end( ), _data.begin( ) );
    }

    uint16_t length( ) const & { return _length; }

    T * begin( ) & { return reinterpret_cast< T * >( &_data[ 0 ] + sizeof_compact_u16( _length ) ); }
    T * end( ) & { return reinterpret_cast< T * >( &_data[ 0 ] + _data.size( ) ); }

    const T * begin( ) const & { return reinterpret_cast< const T * >( &_data[ 0 ] + sizeof_compact_u16( _length ) ); }
    const T * end( ) const & { return reinterpret_cast< const T * >( &_data[ 0 ] + _data.size( ) ); }

    const std::vector< std::byte > & data( ) const & { return _data; }
    std::vector< std::byte > & data( ) & { return _data; }

private:
    uint16_t _length;
    std::vector< std::byte > _data;
};

// Compact-array of account address indexes.
template< uint8_t ... Indices >
class AccountIndexArray : public CompactArray< uint8_t, sizeof ... ( Indices ) >
{
public:
    using BaseType = CompactArray< uint8_t, sizeof ... ( Indices ) >;

    AccountIndexArray( )
    {
        auto i = BaseType::begin( );
        ( ( *(i++) = Indices ), ... );
    }

    friend std::ostream & operator<< ( std::ostream & o, const AccountIndexArray< Indices ... > & accountIndexArray )
    {
        return o << boost::json::array( accountIndexArray.begin( ), accountIndexArray.end( ) );
    }
};


// Compact-array of opaque 8-bit data.
template< class ... Items >
class InstructionDataArray : public CompactArray< uint8_t, ( 0 + ... + sizeof( Items ) ) >
{
public:
    using BaseType = CompactArray< uint8_t, ( 0 + ... + sizeof( Items ) ) >;

    InstructionDataArray( Items ... items )
    {
        auto i = BaseType::begin( );
        ( ( i = insert( i, items ) ), ... );

        BOOST_ASSERT_MSG( i == BaseType::end( ), "Invalid size after adding all items to instruction" );
    }

    friend std::ostream & operator<< ( std::ostream & o, const InstructionDataArray< Items ... > & accountIndexArray )
    {
        return o << boost::json::array( accountIndexArray.begin( ), accountIndexArray.end( ) );
    }

private:
    template< class IntegralType, class = std::enable_if< std::is_integral_v< IntegralType > > >
    auto insert( auto position, IntegralType value )
    {
        BOOST_ASSERT_MSG( position + sizeof( value ) <= BaseType::end( ), "Invalid size after adding item to instruction" );

        return std::copy( reinterpret_cast< uint8_t * >( &value ), reinterpret_cast< uint8_t * >( &value ) + sizeof( value ), position );
    }

    auto insert( auto position, const Core::Hash & value )
    {
        BOOST_ASSERT_MSG( position + sizeof( Core::Hash::size ) <= BaseType::end( ), "Invalid size after adding Core::Hash to instruction" );

        return std::copy( value.data( ).begin( ), value.data( ).end( ), position );
    }
};


// An instruction contains a program id index,
// followed by a compact-array of account address indexes,
// followed by a compact-array of opaque 8-bit data.
template< uint8_t ProgramId, class AccountIndices, class ... Items >
class Instruction
{
public:
    Instruction( Items ... items )
        : _instructionData( items ... )
    {
        static_assert( sizeof( *this ) == sizeof( _programId ) + sizeof( _accountIndices ) + sizeof( _instructionData ), "Invalid Instruction size" );
    }

    friend std::ostream & operator<< ( std::ostream & o, const Instruction< ProgramId, AccountIndices, Items ... > & instruction )
    {
        return o << fmt::format
        (
            "Program Id: {}, account indices: {}, instruction data: {}",
            std::to_string( instruction._programId ),
            instruction._accountIndices,
            instruction._instructionData
        );
    }

private:
    // The instruction's program id specifies which program will process this instruction.
    // The program's account's owner specifies which loader should be used to load and execute the program
    // and the data contains information about how the runtime should execute the program.
    uint8_t _programId = ProgramId;
    AccountIndices _accountIndices;
    InstructionDataArray< Items ... > _instructionData;
};


// Compact array of signatures.
template< uint16_t SigningWritableCount, uint16_t SigningReadOnlyCount >
class SignatureArray : public CompactArray<Core::Signature, SigningWritableCount + SigningReadOnlyCount >
{
public:
    using BaseType = CompactArray< Core::Signature, SigningWritableCount + SigningReadOnlyCount >;

    SignatureArray( ) = default;

    template< size_t MessageSize >
    void sign
    (
        std::span< const std::byte, MessageSize > message,
        const std::array< Core::KeyPairRef, SigningWritableCount > & signingWritableKeys,
        const std::array< Core::KeyPairRef, SigningReadOnlyCount > & signingReadOnlyKeys
    )
    {
        auto signatureIt = BaseType::begin( );
        for ( const auto & keyPair : signingWritableKeys )
        {
            signatureIt->sign_ed25519( message, keyPair );
            ++signatureIt;
        }

        for ( const auto & keyPair : signingReadOnlyKeys )
        {
            signatureIt->sign_ed25519( message, keyPair );
            ++signatureIt;
        }
    }

    friend std::ostream & operator<< ( std::ostream & o, const SignatureArray< SigningWritableCount, SigningReadOnlyCount > signatureArray )
    {
        boost::json::array result;
        for ( const auto & signature : signatureArray )
        {
            result.emplace_back( signature.enc_base58_text( ) );
        }
        return o << result;
    }
};


// Compact-array of accounts.
template
<
    size_t SigningWritableCount,
    size_t SigningReadOnlyCount,
    size_t WritableCount,
    size_t ReadOnlyCount
>
class AccountArray : public CompactArray< Core::PublicKey, SigningWritableCount + SigningReadOnlyCount + WritableCount + ReadOnlyCount >
{
public:
    using BaseType = CompactArray< Core::PublicKey, SigningWritableCount + SigningReadOnlyCount + WritableCount + ReadOnlyCount >;

    constexpr AccountArray
    (
        const std::array< Core::KeyPairRef, SigningWritableCount > & signingWritableKeyPairs,
        const std::array< Core::KeyPairRef, SigningReadOnlyCount > & signingReadOnlyKeyPairs,
        const std::array< Core::PublicKeyRef, WritableCount > & writablePublicKeys,
        const std::array< Core::PublicKeyRef, ReadOnlyCount > & readOnlyPublicKeys
    )
    {
        auto it = BaseType::begin( );

        for ( auto & keyPair : signingWritableKeyPairs )
        {
            *( it++ ) = *keyPair;
        }

        for ( auto & keyPair : signingReadOnlyKeyPairs )
        {
            *( it++ ) = *keyPair;
        }

        for ( auto & keyPair : writablePublicKeys )
        {
            *( it++ ) = *keyPair;
        }

        for ( auto & keyPair : readOnlyPublicKeys )
        {
            *( it++ ) = *keyPair;
        }

        BOOST_ASSERT_MSG( it == BaseType::end( ), "Invalid AccountArray size" );
    }

    friend std::ostream & operator<<
    (
        std::ostream & o,
        const AccountArray< SigningWritableCount, SigningReadOnlyCount, WritableCount, ReadOnlyCount > & value
    )
    {
        return o << fmt::format("{}", fmt::join( std::span( value.begin( ), value.length( ) ), "," ) );
    }
};

// Compact-array of instructions.
template< class ... Instructions >
class InstructionArray
{
public:
    InstructionArray( Instructions ... instructions )
        : _length( compact_u16< sizeof ... ( Instructions ) >( ) )
    {
        auto it = _data.begin( );
        ( ( it = std::copy(
            reinterpret_cast< const std::byte * >( &instructions ),
            reinterpret_cast< const std::byte * >( &instructions + 1 ),
            it ) )
        , ... );

        BOOST_ASSERT_MSG( it == _data.end( ), "Unexpected instruction array size" );
    }

private:
    std::array< std::byte, sizeof( decltype( compact_u16< sizeof ... ( Instructions ) >( ) ) ) > _length;
    std::array< std::byte, ( 0 + ... + sizeof( Instructions ) ) > _data;
};

template
<
    size_t SigningWritableCount,
    size_t SigningReadOnlyCount,
    size_t WritableCount,
    size_t ReadOnlyCount,
    class ... Instructions
>
class Transaction
{
public:
    Transaction
    (
        const Core::Hash & recentBlockhash,
        const std::array< Core::KeyPairRef, SigningWritableCount > & signingWritableKeyPairs,
        const std::array< Core::KeyPairRef, SigningReadOnlyCount > & signingReadOnlyKeyPairs,
        const std::array< Core::PublicKeyRef, WritableCount > & writablePublicKeys,
        const std::array< Core::PublicKeyRef, ReadOnlyCount > & readOnlyPublicKeys,
        Instructions ... instructions
    )
        : _accounts( signingWritableKeyPairs, signingReadOnlyKeyPairs, writablePublicKeys, readOnlyPublicKeys )
        , _recentBlockhash( recentBlockhash )
        , _instructions( instructions ... )
    {
        static_assert( sizeof( *this ) == size( ), "Invalid Transaction packing" );

        _signatures.sign
        (
            std::span< const std::byte, message_size( ) >( begin( ) + sizeof( Signatures ), end( ) ),
            signingWritableKeyPairs,
            signingReadOnlyKeyPairs
        );
    }

    const std::byte * begin( ) const { return reinterpret_cast< const std::byte * >( this ); }
    const std::byte * end( ) const { return reinterpret_cast< const std::byte * >( this + 1 ); }

    static constexpr size_t size( )
    {
        return sizeof( Signatures ) + sizeof( MessageHeader ) + sizeof( Accounts ) + sizeof( Core::Hash ) + sizeof( InstructionData );
    }

    static constexpr size_t message_size( )
    {
        return sizeof( MessageHeader ) + sizeof( Accounts ) + sizeof( Core::Hash ) + sizeof( InstructionData );
    }

    friend std::ostream & operator <<
    (
        std::ostream & o,
        const Transaction< SigningWritableCount, SigningReadOnlyCount, WritableCount, ReadOnlyCount, Instructions ... > & transaction
    )
    {
        return o
            << fmt::format
            (
                "Signatures:{}, transaction header: {}, accounts: {}, recent blockhash: {}, instruction data: {}",
                transaction._signatures,
                std::span
                (
                    transaction._messageHeader.begin( ),
                    transaction._messageHeader.end( )
                ),
                transaction._accounts,
                transaction._recentBlockhash,
                std::span
                (
                    transaction.end( ) - sizeof( transaction._instructions ),
                    transaction.end( )
                )
            );
    }

    friend void tag_invoke
    (
        boost::json::value_from_tag,
        boost::json::value & jsonValue,
        const Transaction< SigningWritableCount, SigningReadOnlyCount, WritableCount, ReadOnlyCount, Instructions ... > & request )
    {
        jsonValue = boost::json::array
        {
            enc_base64( std::span( request.begin( ), request.end( ) ) ),
            { { "encoding", "base64" } }
        };
    }

private:
    // The message header contains three unsigned 8-bit values.
    // The first value is the number of required signatures in the containing transaction.
    // The second value is the number of those corresponding account addresses that are read-only.
    // The third value in the message header is the number of read-only account addresses not requiring signatures.
    static constexpr std::array< uint8_t, 3 > message_header( )
    {
        return { SigningWritableCount + SigningReadOnlyCount, SigningReadOnlyCount, ReadOnlyCount };
    }

    using Signatures = SignatureArray< SigningWritableCount, SigningReadOnlyCount >;
    using MessageHeader = std::array< uint8_t, 3 >;
    using Accounts = AccountArray< SigningWritableCount, SigningReadOnlyCount, WritableCount, ReadOnlyCount >;
    using InstructionData = InstructionArray< Instructions... >;

    Signatures _signatures;
    MessageHeader _messageHeader = message_header( );
    Accounts _accounts;
    Core::Hash _recentBlockhash;
    InstructionData _instructions;
};

#pragma GCC diagnostic pop "-Wpadded"

// Dynamic Solana Transaction API

class DynamicAccountIndexArray : public DynamicCompactArray< uint8_t >
{
public:
    using BaseType = DynamicCompactArray< uint8_t >;

    explicit DynamicAccountIndexArray( std::vector< uint8_t > accountIndices )
        : BaseType( accountIndices.size( ) )
    {
        std::copy( accountIndices.begin( ), accountIndices.end( ), begin( ) );
    }

    friend std::ostream & operator<< ( std::ostream & o, const DynamicAccountIndexArray & accountIndexArray )
    {
        return o << fmt::format( "Account indices: {}", accountIndexArray );
    }
};

template< class ... Items >
class DynamicInstruction
{
public:
    DynamicInstruction( uint8_t programId, DynamicAccountIndexArray accountIndices, Items ... items )
        : _data( 1 + accountIndices.data( ).size( ) + sizeof( InstructionDataArray< Items ... > ) )
    {
        InstructionDataArray< Items ... > instructionData( items ... );
        _data[ 0 ] = static_cast< std::byte >( programId );
        auto it = std::copy( accountIndices.data( ).begin( ), accountIndices.data( ).end( ), _data.begin( ) + 1 );
        it = std::copy( reinterpret_cast< const std::byte * >( &instructionData ), reinterpret_cast< const std::byte * >( &instructionData + 1 ), it );

        BOOST_ASSERT_MSG( it == _data.end( ), "Unexpected DynamicInstruction serialization length" );
    }

    const std::vector< std::byte > & data( ) const & { return _data; }
    std::vector< std::byte > & data( ) & { return _data; }

    friend std::ostream & operator<< ( std::ostream & o, const DynamicInstruction< Items ... > & instruction )
    {
        return o << fmt::format( "Instruction: {}", instruction.data( ) );
    }

private:
    std::vector< std::byte > _data;
};

template< class ... DynamicInstructions >
class DynamicTransaction
{
public:
    DynamicTransaction
    (
        const Core::Hash & recentBlockhash,
        std::span< const Core::KeyPairRef > signingWritableKeyPairs,
        std::span< const Core::KeyPairRef > signingReadOnlyKeyPairs,
        std::span< const Core::PublicKeyRef > writablePublicKeys,
        std::span< const Core::PublicKeyRef > readOnlyPublicKeys,
        DynamicInstructions ... instructions
    )
    {
        const auto signatureLength = signingWritableKeyPairs.size( ) + signingReadOnlyKeyPairs.size( );
        const auto accountsLength = signingWritableKeyPairs.size( ) + signingReadOnlyKeyPairs.size( ) + writablePublicKeys.size( ) + readOnlyPublicKeys.size( );

        auto data = std::span< std::byte, std::dynamic_extent >( _data );

        auto it = encode_compact_u16( signatureLength, data );
        auto signatures = std::span< Core::Signature >( reinterpret_cast< Core::Signature * >( &*it ), signatureLength );
        it += signatureLength * sizeof( Core::Signature );
        auto messageBegin = it;

        // Encode message header.
        it = encode_message_header
        (
            static_cast< uint8_t >( signingWritableKeyPairs.size( ) ), 
            static_cast< uint8_t >( signingReadOnlyKeyPairs.size( ) ),
            static_cast< uint8_t >( readOnlyPublicKeys.size( ) ),
            std::span< std::byte >( it, data.end( ) )
        );

        // Encode accounts.
        it = encode_compact_u16( accountsLength, std::span( it, data.end( ) ) );
        auto accounts = std::span< Core::Hash >( reinterpret_cast< Core::Hash * >( &*it ), accountsLength );
        auto accountsIt = accounts.begin( );

        for ( auto & keyPair : signingWritableKeyPairs )
        {
            *( accountsIt++ ) = *keyPair;
        }

        for ( auto & keyPair : signingReadOnlyKeyPairs )
        {
            *( accountsIt++ ) = *keyPair;
        }

        for ( auto & keyPair : writablePublicKeys )
        {
            *( accountsIt++ ) = *keyPair;
        }

        for ( auto & keyPair : readOnlyPublicKeys )
        {
            *( accountsIt++ ) = *keyPair;
        }
        it += accountsLength * sizeof( Core::Hash );

        // Encode recent blockhash.
        auto recentBlockhashBegin = it;
        it = std::copy( recentBlockhash.data( ).begin( ), recentBlockhash.data( ).end( ), it );

        auto instructionBegin = it;

        auto instructionLength = sizeof ... ( instructions );
        it = encode_compact_u16( instructionLength, std::span( it, data.end( ) ) );
        ( ( it = std::copy( instructions.data( ).begin( ), instructions.data( ).end( ), it ) ), ... );

        sign_message( signingWritableKeyPairs, signingReadOnlyKeyPairs, std::span< const std::byte >( messageBegin, it ), signatures );
        _transaction = std::span( data.begin( ), it );

        // For print debugging.
        _signatures = signatures;
        _accounts = accounts;
        _messageHeader = std::span< std::byte >( messageBegin, 3 );
        _instructions = std::span< std::byte >( instructionBegin, it );
        _recentBlockhash = std::span< std::byte >( recentBlockhashBegin, sizeof( Core::Hash ) );
    }

    std::span< const std::byte > data( ) const & { return _transaction; }

    friend std::ostream & operator <<
    (
        std::ostream & o,
        const DynamicTransaction< DynamicInstructions ... > & transaction
    )
    {
        return o << fmt::format
        (
            "Signatures: {}, transaction header: {}, accounts: {}, recent blockhash: {}, instructions: {}",
            fmt::join( transaction._signatures, "," ),
            fmt::join( transaction._messageHeader, "," ),
            fmt::join( transaction._accounts, "," ),
            *reinterpret_cast< Core::Hash * >( &transaction._recentBlockhash[ 0 ] ),
            fmt::join( transaction._instructions, "," )
        );
    }

    friend void tag_invoke
    (
        boost::json::value_from_tag,
        boost::json::value & jsonValue,
        const DynamicTransaction< DynamicInstructions ... > & request
    )
    {
        jsonValue = boost::json::array
        {
            enc_base64( std::span< const std::byte >( request.data( ) ) ),
            { { "encoding", "base64" } }
        };
    }

private:
    // The message header contains three unsigned 8-bit values.
    // The first value is the number of required signatures in the containing transaction.
    // The second value is the number of those corresponding account addresses that are read-only.
    // The third value in the message header is the number of read-only account addresses not requiring signatures.
    static constexpr auto encode_message_header
    (
        uint8_t signingWritableCount,
        uint8_t signingReadOnlyCount,
        uint8_t readOnlyCount,
        std::span< std::byte > output
    )
    {
        auto it = output.begin( );
        *(it++) = static_cast< std::byte >( signingWritableCount + signingReadOnlyCount );
        *(it++) = static_cast< std::byte >( signingReadOnlyCount );
        *(it++ ) = static_cast< std::byte >( readOnlyCount );
        return it;
    }

    void sign_message
    (
        std::span< const Core::KeyPairRef > signingWritableKeys,
        std::span< const Core::KeyPairRef > signingReadOnlyKeys,
        std::span< const std::byte > message,
        std::span< Core::Signature > signatures
    )
    {
        auto signatureIt = signatures.begin( );

        for ( const auto & keyPair : signingWritableKeys )
        {
            signatureIt->sign_ed25519( message, keyPair );
            ++signatureIt;
        }

        for ( const auto & keyPair : signingReadOnlyKeys )
        {
            signatureIt->sign_ed25519( message, keyPair );
            ++signatureIt;
        }
    }

    std::array< std::byte, solana_max_transaction_size( ) > _data; // Solana max transaction size is 1232.
    std::span< std::byte > _transaction;

    // For print debugging.
    std::span< Core::Signature > _signatures;
    std::span< std::byte > _messageHeader;
    std::span< Core::Hash > _accounts;
    std::span< std::byte > _recentBlockhash;
    std::span< std::byte > _instructions;
};

} // namespace Solana
} // namespace Synthfi

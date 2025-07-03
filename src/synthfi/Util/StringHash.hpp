#pragma once

#include <boost/json/string.hpp>
#include <string>
#include <unordered_map>

namespace Synthfi
{

struct StringHash
{
    using hash_type = std::hash< std::string_view >;
    using is_transparent = void;

    size_t operator( )( const char * str ) const        { return hash_type{ }( str ); }
    size_t operator( )( std::string_view str ) const   { return hash_type{ }( str ); }
    size_t operator( )( const std::string & str ) const { return hash_type{ }( str ); }
    size_t operator( )( const boost::json::string & str ) const { return hash_type{ }( str ); }
};

template< class ValueType >
using TransparentStringMap = std::unordered_map< std::string, ValueType, StringHash, std::equal_to< > >;

} // namespace Synthfi

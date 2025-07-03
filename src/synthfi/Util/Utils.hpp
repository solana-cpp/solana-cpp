#pragma once

#include <stdexcept>

constexpr int json_parse_error( ) { return -32700; }
constexpr int json_invalid_request( ) { return -32600; }
constexpr int json_unknown_method( ) { return -32601; }
constexpr int json_invalid_params( ) { return -32602; }
constexpr int json_unknown_symbol( ) { return -32000; }
constexpr int json_missing_permissionS( ) { return -32001; }
constexpr int json_not_ready( ) { return -32002; }

namespace Synthfi
{

class SynthfiError : public std::runtime_error
{
    using std::runtime_error::runtime_error;
};


} // namespace Synthfi

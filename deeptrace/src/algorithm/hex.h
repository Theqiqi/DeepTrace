#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace::internal {

// Encode bytes to uppercase hex string, no separator.
std::string hex_encode(const uint8_t* data, size_t len);

// Decode hex string (may contain spaces and an optional 0x prefix).
// Returns false on invalid input (odd length or non-hex char).
bool hex_decode(const std::string& hex, std::vector<uint8_t>& out);

// Parse a numeric string: 0x prefix -> hex, otherwise decimal.
bool parse_uint64(const std::string& s, uint64_t& out);

}  // namespace deeptrace::internal

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace pmem::internal {

// A single pattern byte. wildcard=true means any byte matches (??).
struct PatternByte {
    uint8_t value = 0;
    bool wildcard = false;
};

// Parse an AOB pattern string like "48 8B ?? ?? 00".
// Returns false on BadFormat.
bool parse_pattern(const std::string& pattern, std::vector<PatternByte>& out);

// Find all occurrences of pattern in [data, data+len).
// Returns offsets relative to data. May be large; caller caps as needed.
std::vector<size_t> scan_bytes(const uint8_t* data, size_t len,
                               const std::vector<PatternByte>& pattern);

}  // namespace pmem::internal

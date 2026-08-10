#include "algorithm/scan.h"

#include <cctype>

namespace deeptrace::internal {

static int nibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

bool parse_pattern(const std::string& pattern, std::vector<PatternByte>& out) {
    out.clear();
    std::string token;
    for (char c : pattern) {
        if (c == ' ' || c == '\t') {
            if (!token.empty()) {
                if (token == "??" || token == "?") {
                    out.push_back(PatternByte{0, true});
                } else if (token.size() == 2) {
                    int hi = nibble(token[0]);
                    int lo = nibble(token[1]);
                    if (hi < 0 || lo < 0) return false;
                    out.push_back(PatternByte{static_cast<uint8_t>((hi << 4) | lo), false});
                } else {
                    return false;
                }
                token.clear();
            }
            continue;
        }
        token.push_back(c);
    }
    if (!token.empty()) {
        if (token == "??" || token == "?") {
            out.push_back(PatternByte{0, true});
        } else if (token.size() == 2) {
            int hi = nibble(token[0]);
            int lo = nibble(token[1]);
            if (hi < 0 || lo < 0) return false;
            out.push_back(PatternByte{static_cast<uint8_t>((hi << 4) | lo), false});
        } else {
            return false;
        }
    }
    return !out.empty();
}

std::vector<size_t> scan_bytes(const uint8_t* data, size_t len,
                               const std::vector<PatternByte>& pattern) {
    std::vector<size_t> hits;
    if (pattern.empty() || len < pattern.size()) return hits;
    const size_t plen = pattern.size();
    const size_t last = len - plen;
    for (size_t i = 0; i <= last; ++i) {
        size_t j = 0;
        for (; j < plen; ++j) {
            if (!pattern[j].wildcard && data[i + j] != pattern[j].value) break;
        }
        if (j == plen) hits.push_back(i);
    }
    return hits;
}

}  // namespace deeptrace::internal

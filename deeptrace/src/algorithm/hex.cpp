#include "algorithm/hex.h"

#include <cctype>
#include <cstdio>

namespace deeptrace::internal {

static const char* kHexDigits = "0123456789ABCDEF";

std::string hex_encode(const uint8_t* data, size_t len) {
    std::string out;
    out.reserve(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out.push_back(kHexDigits[(data[i] >> 4) & 0xF]);
        out.push_back(kHexDigits[data[i] & 0xF]);
    }
    return out;
}

bool hex_decode(const std::string& hex, std::vector<uint8_t>& out) {
    std::string cleaned;
    cleaned.reserve(hex.size());
    for (char c : hex) {
        if (c == ' ' || c == '\t') continue;
        cleaned.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
    }
    if (cleaned.size() >= 2 && cleaned[0] == '0' && cleaned[1] == 'X') {
        cleaned.erase(0, 2);
    }
    if (cleaned.size() % 2 != 0) return false;
    out.clear();
    out.reserve(cleaned.size() / 2);
    for (size_t i = 0; i < cleaned.size(); i += 2) {
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hi = nibble(cleaned[i]);
        int lo = nibble(cleaned[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return true;
}

bool parse_uint64(const std::string& s, uint64_t& out) {
    std::string t;
    for (char c : s) {
        if (c == ' ' || c == '\t') continue;
        t.push_back(c);
    }
    if (t.empty()) return false;
    int base = 10;
    size_t start = 0;
    if (t.size() >= 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) {
        base = 16;
        start = 2;
    }
    if (start >= t.size()) return false;
    uint64_t value = 0;
    for (size_t i = start; i < t.size(); ++i) {
        char c = t[i];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (base == 16 && c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return false;
        if (value > (UINT64_MAX - static_cast<uint64_t>(d)) / static_cast<uint64_t>(base)) {
            return false;  // overflow
        }
        value = value * static_cast<uint64_t>(base) + static_cast<uint64_t>(d);
    }
    out = value;
    return true;
}

}  // namespace deeptrace::internal

#include "infrastructure/assembly/asmenc.h"

#include <keystone/keystone.h>

#include <string>
#include <vector>

namespace deeptrace::internal {

// Assemble a single x64 instruction line using Keystone (Intel syntax, 64-bit
// mode). The base address is used to compute PC-relative displacements.
bool asm_one_at(const std::string& line, uint64_t base_addr,
                std::vector<uint8_t>& out) {
    out.clear();

    ks_engine* ks = nullptr;
    ks_err err = ks_open(KS_ARCH_X86, KS_MODE_64, &ks);
    if (err != KS_ERR_OK) return false;

    unsigned char* encode = nullptr;
    size_t size = 0;
    size_t count = 0;
    bool ok = false;
    if (ks_asm(ks, line.c_str(), base_addr, &encode, &size, &count) == KS_ERR_OK &&
        count > 0) {
        out.assign(encode, encode + size);
        ok = true;
    }

    if (encode) ks_free(encode);
    ks_close(ks);
    return ok;
}

bool asm_one(const std::string& line, std::vector<uint8_t>& out) {
    return asm_one_at(line, 0, out);
}

// Encode a near jmp (E9 rel32) from insn_addr to target.
std::vector<uint8_t> encode_jmp_rel32(uint64_t insn_addr, uint64_t target) {
    int64_t rel = static_cast<int64_t>(target) - (static_cast<int64_t>(insn_addr) + 5);
    std::vector<uint8_t> bytes;
    bytes.push_back(0xE9);
    for (int i = 0; i < 4; ++i) {
        bytes.push_back(static_cast<uint8_t>((static_cast<uint64_t>(rel) >> (8 * i)) & 0xFF));
    }
    return bytes;
}

// Assemble a multi-line stream that may define and reference labels. Labels
// defined in the text (\"name:\") are resolved by Keystone itself; any symbol
// Keystone cannot resolve locally is asked of the optional resolver callback.
bool asm_labels(const std::string& code, uint64_t base_addr,
                bool (*resolver)(const char* symbol, uint64_t* value),
                std::vector<uint8_t>& out) {
    out.clear();

    ks_engine* ks = nullptr;
    ks_err err = ks_open(KS_ARCH_X86, KS_MODE_64, &ks);
    if (err != KS_ERR_OK) return false;

    if (resolver) {
        ks_option(ks, KS_OPT_SYM_RESOLVER, reinterpret_cast<size_t>(resolver));
    }

    unsigned char* encode = nullptr;
    size_t size = 0;
    size_t count = 0;
    bool ok = false;
    if (ks_asm(ks, code.c_str(), base_addr, &encode, &size, &count) == KS_ERR_OK &&
        count > 0) {
        out.assign(encode, encode + size);
        ok = true;
    }

    if (encode) ks_free(encode);
    ks_close(ks);
    return ok;
}

}  // namespace deeptrace::internal

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

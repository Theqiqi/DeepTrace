#include "infrastructure/assembly/asmenc.h"

#include <keystone/keystone.h>

#include <string>
#include <vector>

namespace pmem::internal {

// Assemble a single x64 instruction line using Keystone (Intel syntax, 64-bit
// mode).  Replaces the previous hand-written subset encoder; the public
// asm_one() interface is unchanged so callers (service/asm.cpp) stay intact.
bool asm_one(const std::string& line, std::vector<uint8_t>& out) {
    out.clear();

    ks_engine* ks = nullptr;
    ks_err err = ks_open(KS_ARCH_X86, KS_MODE_64, &ks);
    if (err != KS_ERR_OK) return false;

    unsigned char* encode = nullptr;
    size_t size = 0;
    size_t count = 0;
    bool ok = false;
    if (ks_asm(ks, line.c_str(), 0, &encode, &size, &count) == KS_ERR_OK && count > 0) {
        out.assign(encode, encode + size);
        ok = true;
    }

    if (encode) ks_free(encode);
    ks_close(ks);
    return ok;
}

}  // namespace pmem::internal

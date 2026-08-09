#include "infrastructure/disassembly/disasm.h"

#include <capstone/capstone.h>

#include <string>

namespace pmem::internal {

// Decode a single x64 instruction with Capstone (Intel syntax, 64-bit mode).
// Replaces the previous hand-written subset decoder; the public disasm_one()
// interface is unchanged so callers (service/disasm.cpp) stay intact.
// Returns false if the bytes are invalid or too short to form a complete
// instruction (matches the previous "stream too short" contract).
//
// Note: use cs_disasm(count=1), NOT cs_disasm_iter + a stack cs_insn.  In the
// MSVC debug build, cs_disasm_iter with a caller-provided (uninitialized)
// cs_insn crashes with 0xc0000005 on every decode (verified v1.2); cs_disasm
// allocates its own insn array internally and is stable.  Do not "simplify"
// this back to cs_disasm_iter without retesting.
bool disasm_one(const uint8_t* bytes, size_t max_len, uint64_t address,
                DecodedInsn& out) {
    csh handle = 0;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK) return false;
    cs_option(handle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_INTEL);

    cs_insn* insns = nullptr;
    size_t n = cs_disasm(handle, bytes, max_len, address, 1, &insns);
    bool ok = false;
    if (n > 0) {
        out.length = static_cast<uint8_t>(insns[0].size);
        out.text = insns[0].mnemonic;
        if (insns[0].op_str[0] != '\0') {
            out.text += ' ';
            out.text += insns[0].op_str;
        }
        ok = true;
    }
    if (insns) cs_free(insns, n);
    cs_close(&handle);
    return ok;
}

}  // namespace pmem::internal

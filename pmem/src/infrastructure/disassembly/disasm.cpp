#include "infrastructure/disassembly/disasm.h"

#include <capstone/capstone.h>

#include <string>

namespace pmem::internal {

// Decode a single x64 instruction with Capstone (Intel syntax, 64-bit mode).
// Replaces the previous hand-written subset decoder; the public disasm_one()
// interface is unchanged so callers (service/disasm.cpp) stay intact.
// Returns false if the bytes are invalid or too short to form a complete
// instruction (matches the previous "stream too short" contract).
bool disasm_one(const uint8_t* bytes, size_t max_len, uint64_t address,
                DecodedInsn& out) {
    csh handle = 0;
    if (cs_open(CS_ARCH_X86, CS_MODE_64, &handle) != CS_ERR_OK) return false;
    cs_option(handle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_INTEL);

    cs_insn insn;
    const uint8_t* code = bytes;
    size_t size = max_len;
    uint64_t addr = address;
    bool ok = false;
    if (cs_disasm_iter(handle, &code, &size, &addr, &insn)) {
        out.length = static_cast<uint8_t>(insn.size);
        out.text = insn.mnemonic;
        if (insn.op_str[0] != '\0') {
            out.text += ' ';
            out.text += insn.op_str;
        }
        ok = true;
    }
    cs_close(&handle);
    return ok;
}

}  // namespace pmem::internal

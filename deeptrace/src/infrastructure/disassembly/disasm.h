#pragma once
#include <cstdint>
#include <string>

namespace deeptrace::internal {

struct DecodedInsn {
    uint8_t length = 0;      // number of bytes consumed
    std::string text;        // pure ASCII, Capstone Intel syntax, e.g. "mov rax, qword ptr [rbp + 8]"
};

// Decode one x64 instruction from bytes at the given virtual address.
// Returns false if the stream is too short (incomplete instruction).
bool disasm_one(const uint8_t* bytes, size_t max_len, uint64_t address,
                DecodedInsn& out);

}  // namespace deeptrace::internal

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace::internal {

// Assemble a single x64 instruction line like "mov rax, 1" (Intel syntax,
// 64-bit mode) using the Keystone engine.  Jumps/calls accept an absolute
// target address (dec or 0x hex); Keystone emits the PC-relative displacement
// computed from the given base address (the address of the first byte of the
// instruction stream).  Returns false on unsupported mnemonic / bad operand
// syntax.
bool asm_one_at(const std::string& line, uint64_t base_addr,
                std::vector<uint8_t>& out);

// Assemble a single instruction with base address 0 (absolute displacement
// from 0). Kept for callers that do not know the final base.
bool asm_one(const std::string& line, std::vector<uint8_t>& out);

// Assemble a multi-line instruction stream that may define labels ("name:")
// and reference them ("jmp name") or reference external symbols via the
// resolver. base_addr is the address of the first byte of the stream, used to
// compute PC-relative displacements. Returns false on error (unknown mnemonic,
// undefined symbol, syntax error).
bool asm_labels(const std::string& code, uint64_t base_addr,
                bool (*resolver)(const char* symbol, uint64_t* value),
                std::vector<uint8_t>& out);

// Split "mov rax, qword ptr [rbp+8]" style lines is not required;
// asm_one takes a single instruction line.

}  // namespace deeptrace::internal

#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace::internal {

// Assemble a single x64 instruction line like "mov rax, 1" (Intel syntax,
// 64-bit mode) using the Keystone engine.  Jumps/calls accept an absolute
// target address (dec or 0x hex); Keystone emits the PC-relative displacement.
// Returns false on unsupported mnemonic / bad operand syntax.
bool asm_one(const std::string& line, std::vector<uint8_t>& out);

// Split "mov rax, qword ptr [rbp+8]" style lines is not required;
// asm_one takes a single instruction line.

}  // namespace deeptrace::internal

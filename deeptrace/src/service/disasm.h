#pragma once
#include "domain/types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace deeptrace {
Result disasm_at(uintptr_t addr, uint32_t count, std::vector<Instruction>& out);
Result disasm_range(uintptr_t start, uintptr_t end, std::vector<Instruction>& out);
// Disassemble a local byte buffer (no session). base_addr is the address
// shown for data[0] (0 for files); decoding stops at the first undecodable
// byte. count 1..10000. v2.13.0.
Result disasm_buffer(const uint8_t* data, size_t size, uintptr_t base_addr,
                     uint32_t count, std::vector<Instruction>& out);
}  // namespace deeptrace

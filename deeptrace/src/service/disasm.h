#pragma once
#include "domain/types.h"

#include <cstdint>
#include <vector>

namespace deeptrace {
Result disasm_at(uintptr_t addr, uint32_t count, std::vector<Instruction>& out);
Result disasm_range(uintptr_t start, uintptr_t end, std::vector<Instruction>& out);
}  // namespace deeptrace

#pragma once
#include "domain/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace {
Result asm_assemble(const std::string& code, std::vector<uint8_t>& out,
                    std::string* out_text);
}  // namespace deeptrace

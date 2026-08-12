#pragma once
#include "domain/types.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace deeptrace {
Result asm_assemble(const std::string& code, std::vector<uint8_t>& out,
                    std::string* out_text);

// Assemble multi-line code that may define labels ("name:") and reference
// them ("jmp name") or reference external symbols from `symbols`. base_addr is
// the address of the first byte of the stream (used for PC-relative
// displacements). Undefined symbol / bad instruction -> BadFormat.
Result asm_assemble_labels(const std::string& code, uintptr_t base_addr,
                           const std::map<std::string, uintptr_t>& symbols,
                           std::vector<uint8_t>& out, std::string* out_text);
}  // namespace deeptrace

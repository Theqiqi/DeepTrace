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
//
// v2.5.0: external symbols may be referenced by any instruction, not only
// jmp/call - memory operands ("mov [sym],rcx" -> RIP-relative [rip+disp32];
// "mov [sym],rax" / "mov rax,[sym]" -> self-encoded moffs64 A1/A3) and
// immediates ("mov rax,sym" -> address literal; the result is re-decoded with
// Capstone so silent truncation is rejected with BadFormat). Complex memory
// expressions ("[rsp+sym]") and immediate refs to stream-internal labels are
// unsupported (BadFormat).
Result asm_assemble_labels(const std::string& code, uintptr_t base_addr,
                           const std::map<std::string, uintptr_t>& symbols,
                           std::vector<uint8_t>& out, std::string* out_text);
}  // namespace deeptrace

#pragma once
#include "domain/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace {
Result dll_inject(const std::string& path, InjectInfo& out);
Result dll_eject(const std::string& path_or_addr);
Result dll_list(std::vector<InjectInfo>& out);
Result dll_status(std::vector<InjectInfo>& out);
Result shellcode_inject(const std::vector<uint8_t>& bytes, InjectInfo& out);
Result shellcode_inject_at(uintptr_t addr, const std::vector<uint8_t>& bytes,
                           InjectInfo& out);
Result shellcode_status(std::vector<InjectInfo>& out);
}  // namespace deeptrace

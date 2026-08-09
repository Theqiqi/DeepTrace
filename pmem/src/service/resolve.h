#pragma once
#include "domain/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace pmem {
Result resolve_base(const std::string& name, uintptr_t* out_base);
Result pattern_scan(const std::string& pattern, std::vector<uintptr_t>& out);
}  // namespace pmem

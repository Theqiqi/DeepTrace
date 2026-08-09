#pragma once
#include "domain/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace {
Result watch_list(std::vector<WatchEntry>& out);
Result watch_add(const std::string& desc, uintptr_t addr, ValueType type);
Result watch_remove(uint32_t index);
Result watch_refresh(std::vector<WatchEntry>& out);
Result watch_clear();
}  // namespace deeptrace

#pragma once
#include "domain/types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace {
Result memory_read(uintptr_t addr, void* buf, size_t size, size_t* out_read);
Result memory_write(uintptr_t addr, const void* buf, size_t size, size_t* out_written);
Result memory_dump(uintptr_t addr, size_t size, std::vector<uint8_t>& out);
Result memory_regions(std::vector<MemoryRegion>& out);
Result memory_readval(uintptr_t addr, ValueType type, std::string& out_text);
}  // namespace deeptrace

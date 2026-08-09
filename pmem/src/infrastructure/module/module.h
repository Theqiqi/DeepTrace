#pragma once
#include "domain/types.h"

#include <cstdint>
#include <vector>

namespace pmem::internal {

// Enumerate modules of the given process handle.
Result EnumModules(void* hprocess, std::vector<ModuleInfo>& out);

// Parse PE export table of a module by reading its memory from the remote process.
Result ParseExports(void* hprocess, uintptr_t base, std::vector<ExportInfo>& out);

}  // namespace pmem::internal

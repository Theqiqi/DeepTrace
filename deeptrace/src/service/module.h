#pragma once
#include "domain/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace {
Result module_list(std::vector<ModuleInfo>& out);
Result module_find(const std::string& name, ModuleInfo& out);
Result module_base(const std::string& name, uintptr_t* out_base);
Result module_exports(const std::string& name, std::vector<ExportInfo>& out);
Result module_dump(const std::string& name, const std::string& output_file,
                   std::string* out_hex);
}  // namespace deeptrace

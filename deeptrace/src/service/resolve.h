#pragma once
#include "domain/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace {
Result resolve_base(const std::string& name, uintptr_t* out_base);
Result pattern_scan(const std::string& pattern, std::vector<uintptr_t>& out);

// v2.12.0 pointer-chain reverse-walk. See deeptrace.h for semantics.
Result pointer_map_snapshot(const PointerScanConfig& cfg,
                            std::vector<PointerChain>& out);
Result pointer_map_rescan(const std::vector<PointerChain>& base,
                          uintptr_t new_target,
                          const PointerScanConfig& cfg,
                          std::vector<PointerChain>& out);
}  // namespace deeptrace

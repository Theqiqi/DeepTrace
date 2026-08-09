#pragma once
#include "domain/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace {
Result debug_attach();
Result debug_detach();
Result debug_pause();
Result debug_resume();
Result debug_step(uint32_t tid, uintptr_t* out_rip);
Result debug_step_over(uint32_t tid, uintptr_t* out_rip);
Result breakpoint_set(uintptr_t addr, BreakpointInfo& out);
Result breakpoint_clear(uintptr_t addr);
Result hw_breakpoint_set(uintptr_t addr, uint32_t type, uint32_t length);
Result hw_breakpoint_clear(uintptr_t addr);
Result guard_set(uintptr_t addr, size_t size);
Result guard_clear(uintptr_t addr, size_t size);
Result debug_status(DebugStatus& out);
Result registers_get(std::vector<RegisterInfo>& out, uint32_t tid);
Result register_get(const std::string& name, uint64_t* out_value, uint32_t tid);
}  // namespace deeptrace

#pragma once
#include "domain/types.h"

#include <cstdint>
#include <vector>

namespace deeptrace {
Result enumerate_processes(std::vector<ProcessInfo>& out);
Result attach(uint32_t pid);
Result detach();
Result process_info(uint32_t pid, ProcessInfo& out);
Result suspend_process(uint32_t pid);
Result resume_process(uint32_t pid);
Result terminate_process(uint32_t pid, uint32_t exit_code);
Result session_pid(uint32_t* out_pid);
Result session_permissions(uint32_t* out_mask);
}  // namespace deeptrace

#pragma once
#include "domain/types.h"

#include <cstdint>
#include <vector>

namespace deeptrace {
Result thread_list(std::vector<ThreadInfo>& out);
Result thread_suspend(uint32_t tid);
Result thread_resume(uint32_t tid);
Result thread_terminate(uint32_t tid, uint32_t exit_code);

// Create a remote thread at an arbitrary executable address (unlike
// shellcode_run, the address does not need to be in the inject record).
Result thread_create_at(uintptr_t addr, uint32_t* out_tid);
}  // namespace deeptrace

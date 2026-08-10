#pragma once
#include "domain/types.h"

#include <cstdint>

namespace deeptrace::internal {

// Create a remote thread that starts at entry with argument arg.
// Returns the thread id in out_tid (0 if not available).
Result CreateRemoteThreadEx(void* hprocess, uintptr_t entry, uintptr_t arg,
                            uint32_t* out_tid);

// Wait for a remote thread to finish (ms timeout). Returns Ok if finished.
Result WaitRemoteThread(void* hprocess, uint32_t tid, uint32_t timeout_ms,
                        uint32_t* out_exit_code);

// Check if a remote thread is still running.
Result IsRemoteThreadRunning(void* hprocess, uint32_t tid, bool* out_running,
                             uint32_t* out_exit_code);

}  // namespace deeptrace::internal

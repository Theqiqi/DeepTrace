#pragma once
#include "domain/types.h"

#include <cstdint>
#include <vector>

namespace deeptrace {
Result thread_list(std::vector<ThreadInfo>& out);
Result thread_suspend(uint32_t tid);
Result thread_resume(uint32_t tid);
Result thread_terminate(uint32_t tid, uint32_t exit_code);
}  // namespace deeptrace

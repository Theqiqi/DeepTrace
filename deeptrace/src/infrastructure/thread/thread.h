#pragma once
#include "domain/types.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace deeptrace::internal {

// Enumerate threads of a process.
Result EnumThreads(uint32_t pid, std::vector<ThreadInfo>& out);

// Iterate all threads; returns Ok. Callback should not be heavy.
Result ForEachThread(uint32_t pid, const std::function<void(const ThreadInfo&)>& cb);

// Open a thread handle. tid=0 -> first thread of process.
void* OpenThreadById(uint32_t pid, uint32_t tid, uint32_t access, Result* err);

// Resolve tid=0 to the first thread id of the process.
Result ResolveTid(uint32_t pid, uint32_t tid, uint32_t* out_tid);

}  // namespace deeptrace::internal

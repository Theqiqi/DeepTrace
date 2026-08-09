#pragma once
#include "domain/types.h"

#include <cstdint>
#include <vector>

struct _PROCESS_INFORMATION;  // not needed

namespace pmem::internal {

// Open a process handle with the requested access. Returns nullptr on failure
// and sets err.
void* OpenProcessById(uint32_t pid, uint32_t access, Result* err);

// Enumerate all processes.
Result EnumProcesses(std::vector<ProcessInfo>& out);

// Query info for a single pid by opening a QUERY_LIMITED_INFORMATION handle.
Result QueryProcessInfo(uint32_t pid, ProcessInfo& out);

// Suspend/resume all threads of a process via thread enumeration.
Result SuspendProcessThreads(uint32_t pid);
Result ResumeProcessThreads(uint32_t pid);

// Terminate a process.
Result TerminateProcessById(uint32_t pid, uint32_t exit_code);

}  // namespace pmem::internal

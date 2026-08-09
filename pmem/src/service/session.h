#pragma once
#include "domain/types.h"

#include <cstdint>
#include <string>

namespace pmem::internal {

struct Session {
    uint32_t pid = 0;
    void* handle = nullptr;  // PROCESS handle
    bool debug_mode = false;
};

Session& session();

// Path to the state directory for a pid: %TEMP%/pmem_<pid>/
std::string state_dir(uint32_t pid);

}  // namespace pmem::internal

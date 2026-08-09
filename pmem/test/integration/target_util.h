#pragma once
// Helper to spawn pmem_target.exe, capture PID and known symbol addresses.
#include <string>
#include <vector>

namespace testutil {

struct TargetInfo {
    uint32_t pid = 0;
    uint32_t worker_tid = 0;
    uintptr_t g_int = 0;
    uintptr_t g_int64 = 0;
    uintptr_t g_float = 0;
    uintptr_t g_double = 0;
    uintptr_t g_bytes = 0;
    uintptr_t g_flag = 0;
    uintptr_t g_counter = 0;
    uintptr_t worker_fn = 0;
};

// Launch the target exe and parse its banner. Returns false on failure.
bool spawn_target(const std::string& exe_path, TargetInfo& out);

// Kill a target by pid.
void kill_target(uint32_t pid);

// Path to the target exe (same build dir as the test).
std::string target_exe_path();

// Path to the companion testdll.dll (same build dir as the test).
std::string test_dll_path();

// File existence check (for companion resources).
bool file_exists(const std::string& path);

}  // namespace testutil

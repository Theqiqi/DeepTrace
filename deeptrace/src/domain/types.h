#pragma once
// deeptrace data layer: public data structures.
// Only standard C++ types are exposed; no windows.h types.

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace {

enum class Result {
    Ok = 0,
    Error,
    InvalidArg,
    NotAttached,
    NoSuchProcess,
    AccessDenied,
    ReadFault,
    WriteFault,
    NotFound,
    Timeout,
    NotSupported,
    AlreadyExists,
    NotExecutable,
    BadFormat
};

enum class ValueType { Byte, Word, Dword, Qword, Float, Double };

enum class BreakpointType { Software, Hardware, PageGuard };

struct ProcessInfo {
    uint32_t pid = 0;
    std::wstring name;
    uint32_t parent_pid = 0;
    uint32_t thread_count = 0;
};

struct MemoryRegion {
    uintptr_t base = 0;
    size_t size = 0;
    uint32_t protection = 0;
    uint32_t state = 0;
    uint32_t type = 0;
};

struct ModuleInfo {
    uintptr_t base = 0;
    size_t size = 0;
    std::wstring name;
    std::wstring path;
};

struct ExportInfo {
    std::string name;
    uintptr_t address = 0;
};

struct ThreadInfo {
    uint32_t tid = 0;
    int32_t priority = 0;
    uintptr_t start_address = 0;
};

struct RegisterInfo {
    std::string name;
    uint64_t value = 0;
};

struct BreakpointInfo {
    uintptr_t address = 0;
    BreakpointType type = BreakpointType::Software;
    uint8_t original_byte = 0;
    int32_t hw_index = -1;
};

struct WatchEntry {
    uint32_t index = 0;
    std::string description;
    uintptr_t address = 0;
    ValueType type = ValueType::Dword;
    std::string value;
    bool valid = false;
};

struct Instruction {
    uintptr_t address = 0;
    std::vector<uint8_t> bytes;
    std::string text;  // pure ASCII text
};

struct DebugStatus {
    bool attached = false;
    uint32_t pid = 0;
    uint32_t breakpoint_count = 0;
    uint32_t hw_breakpoint_count = 0;
};

// Outcome of debug_continue: stop reason after resuming the debuggee.
struct ContinueInfo {
    bool hit = false;             // stopped on an exception (breakpoint/guard/other)
    bool exited = false;          // target process exited
    uint32_t exit_code = 0;       // valid when exited
    uint32_t exception = 0;       // exception code (valid when hit)
    uintptr_t address = 0;        // exception address (valid when hit)
    uintptr_t rip = 0;            // stopped-thread RIP (software breakpoint: post-instruction)
    uint32_t tid = 0;             // stopped-thread id (valid when hit)
};

struct InjectInfo {
    std::wstring path;
    uintptr_t remote_base = 0;
    uint32_t thread_id = 0;
    bool running = false;
    std::string kind;  // "dll" | "shellcode"
    size_t size = 0;
};

// hook record: target patched to jump to newmem, original bytes saved.
struct HookInfo {
    uintptr_t target = 0;
    uintptr_t newmem = 0;
    std::vector<uint8_t> orig_bytes;  // original bytes that were overwritten
    size_t size = 0;                  // patch region length (bytes)
};

// script enable record (per script path).
struct ScriptInfo {
    std::string path;                  // script file path (identity)
    std::string state;                 // "enabled" | "disabled"
    std::vector<HookInfo> hooks;       // hooks registered by this script
    std::vector<std::pair<std::string, uintptr_t>> allocs;  // symbol -> addr
};

// Pointer-chain scan configuration (v2.12.0). Reverse-walk from a target
// value address: each level finds qword pointers whose value lands within
// +/-max_offset of the current target address.
struct PointerScanConfig {
    uintptr_t target = 0;        // value address to reverse-walk from
    uint32_t max_offset = 2048;  // pointer value within target +/- this counts
    uint32_t max_level = 5;      // max chain depth (number of offset levels)
    uint32_t max_results = 10000;  // snapshot output cap (anti false-positive)
    std::string module;          // anchor module name; empty = no anchoring
    uint32_t thread_count = 0;   // 0 = hardware_concurrency
};

// One pointer chain: root address + offset sequence (signed, may be
// negative for pointers stored above the matched field).
// Evaluate: addr = root; for off in offsets: addr = *(qword)addr + off.
struct PointerChain {
    uintptr_t root = 0;
    std::vector<int64_t> offsets;
};

}  // namespace deeptrace

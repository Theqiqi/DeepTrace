#pragma once
// pmem data layer: public data structures.
// Only standard C++ types are exposed; no windows.h types.

#include <cstdint>
#include <string>
#include <vector>

namespace pmem {

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

struct InjectInfo {
    std::wstring path;
    uintptr_t remote_base = 0;
    uint32_t thread_id = 0;
    bool running = false;
    std::string kind;  // "dll" | "shellcode"
    size_t size = 0;
};

}  // namespace pmem

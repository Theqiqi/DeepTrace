# Data Structures (Structs)

Defined in `deeptrace.h` (via `domain/types.h`). All structs use only standard C++ types and expose no `windows.h` types; every field has a default-initialized value.

## ProcessInfo — Process Information

```cpp
struct ProcessInfo {
    uint32_t pid = 0;          // process ID
    std::wstring name;         // process image name (e.g. L"notepad.exe")
    uint32_t parent_pid = 0;   // parent process ID
    uint32_t thread_count = 0; // thread count
};
```

## MemoryRegion — Memory Region

```cpp
struct MemoryRegion {
    uintptr_t base = 0;      // region start address
    size_t size = 0;         // region size (bytes)
    uint32_t protection = 0; // protection attributes (PAGE_READONLY/PAGE_EXECUTE_READ etc., includes the PAGE_GUARD bit)
    uint32_t state = 0;      // state (MEM_COMMIT/MEM_RESERVE/MEM_FREE)
    uint32_t type = 0;       // type (MEM_PRIVATE/MEM_MAPPED/MEM_IMAGE)
};
```

## ModuleInfo — Module Information

```cpp
struct ModuleInfo {
    uintptr_t base = 0;      // module base address
    size_t size = 0;         // module image size (bytes)
    std::wstring name;       // module name (e.g. L"kernel32.dll")
    std::wstring path;       // module full path
};
```

## ExportInfo — Export Symbol

```cpp
struct ExportInfo {
    std::string name;    // exported function name (ASCII)
    uintptr_t address = 0; // exported function address (module base + RVA)
};
```

## ThreadInfo — Thread Information

```cpp
struct ThreadInfo {
    uint32_t tid = 0;            // thread ID
    int32_t priority = 0;        // thread priority
    uintptr_t start_address = 0; // thread entry address
};
```

## RegisterInfo — Register Value

```cpp
struct RegisterInfo {
    std::string name;  // register name ("rax", "rip", "eflags", etc.)
    uint64_t value = 0; // register value
};
```

## BreakpointInfo — Breakpoint Information

```cpp
struct BreakpointInfo {
    uintptr_t address = 0;                    // breakpoint address
    BreakpointType type = BreakpointType::Software; // breakpoint type
    uint8_t original_byte = 0;                // software breakpoint: the overwritten original byte (the value before 0xCC)
    int32_t hw_index = -1;                    // hardware breakpoint: DR0-DR3 slot (-1 means not a hardware breakpoint)
};
```

## WatchEntry — Watch Entry

```cpp
struct WatchEntry {
    uint32_t index = 0;             // watch entry index (starting at 0)
    std::string description;        // description text
    uintptr_t address = 0;          // watched address
    ValueType type = ValueType::Dword; // value type
    std::string value;              // current value text (e.g. "0x11223344"); "??" when the read failed
    bool valid = false;             // whether the target memory value was read successfully
};
```

## Instruction — Disassembly Instruction

```cpp
struct Instruction {
    uintptr_t address = 0;    // instruction address
    std::vector<uint8_t> bytes; // instruction machine code
    std::string text;          // disassembly text (pure ASCII, e.g. "mov rax, rbx")
};
```

## DebugStatus — Debug Status

```cpp
struct DebugStatus {
    bool attached = false;      // whether attached (debug session or process session)
    uint32_t pid = 0;           // current session target process ID
    uint32_t breakpoint_count = 0;     // software breakpoint count (persisted records)
    uint32_t hw_breakpoint_count = 0;  // hardware breakpoint count (persisted records)
};
```

## InjectInfo — Injection Information

```cpp
struct InjectInfo {
    std::wstring path;          // DLL path (dll injection); empty for shellcode injection
    uintptr_t remote_base = 0;  // module base inside the target process / shellcode allocation address
    uint32_t thread_id = 0;     // remote thread ID that performed the injection
    bool running = false;       // DLL: whether still loaded; shellcode: whether the remote thread is still running
    std::string kind;           // "dll" or "shellcode"
    size_t size = 0;            // injected data size (dll: path length; shellcode: byte count)
};
```

## See Also

- [Result](RESULT.md)
- [ValueType / BreakpointType](ENUMS.md)
- [GettingStarted](../GettingStarted.md)

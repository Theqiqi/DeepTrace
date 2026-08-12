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

## ContinueInfo — Debug Continue Stop Reason

Output of `debug_continue`: why the debuggee stopped after being resumed.

```cpp
struct ContinueInfo {
    bool hit = false;             // stopped on an exception (breakpoint/guard/other)
    bool exited = false;          // target process exited
    uint32_t exit_code = 0;       // valid when exited
    uint32_t exception = 0;       // exception code (valid when hit)
    uintptr_t address = 0;        // exception address (valid when hit)
    uintptr_t rip = 0;            // stopped-thread RIP (software breakpoint: post-instruction)
    uint32_t tid = 0;             // stopped-thread id (valid when hit)
};
```

| Field | Meaning |
|-------|---------|
| `hit` | `true` when the call returned because the debuggee raised an exception (self-set software breakpoint, hardware breakpoint, page guard, or other unhandled exception) |
| `exited` | `true` when the debuggee process exited; `exit_code` is then valid |
| `exit_code` | target process exit code (valid only when `exited` is `true`) |
| `exception` | exception code of the stop (e.g. `0x80000003` EXCEPTION_BREAKPOINT); valid when `hit`; final field name `exception` (an earlier design name `exception_code` was dropped — it collides with the Windows SDK `excpt.h` macro) |
| `address` | exception address (the instruction that raised the exception); valid when `hit` |
| `rip` | stopped-thread RIP; for a self-set software breakpoint this is the **post-instruction** RIP (the breakpoint instruction has been executed); for other exceptions it is the faulting instruction address |
| `tid` | stopped-thread ID; valid when `hit` |

`hit` and `exited` are mutually exclusive; when both are `false` the call returned `Result::Timeout` and the debuggee keeps running (see [debug_continue](../Modules/DEBUG.md#deeptracedebug_continue)).

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

## HookInfo — Hook Record

Output of `hook_set`; describes a 5-byte `jmp` patch applied to a target address.

```cpp
struct HookInfo {
    uintptr_t target = 0;              // hooked target address
    uintptr_t newmem = 0;              // jump destination (the injected code buffer)
    std::vector<uint8_t> orig_bytes;   // original bytes that were overwritten
    size_t size = 0;                   // patch region length (bytes, always 5)
};
```

| Field | Meaning |
|-------|---------|
| `target` | the hooked instruction address; the first 5 bytes there were replaced with `E9 <rel32>` (`jmp newmem`) |
| `newmem` | the jump destination, i.e. the injected code buffer the target now jumps to |
| `orig_bytes` | the true original bytes captured before patching (kept even across idempotent re-sets); restored byte-for-byte by `hook_clear` |
| `size` | length of the patched region in bytes (always 5 for the 5-byte `E9 rel32` patch) |

## ScriptInfo — Script Record

One entry of the `script_status` output; describes the persisted state of one script path.

```cpp
struct ScriptInfo {
    std::string path;                                    // script file path (identity)
    std::string state;                                   // "enabled" | "disabled"
    std::vector<HookInfo> hooks;                         // hooks registered by this script
    std::vector<std::pair<std::string, uintptr_t>> allocs;  // symbol -> addr
};
```

| Field | Meaning |
|-------|---------|
| `path` | the script file path, used as the identity key (matching `script_enable`/`script_disable` arguments) |
| `state` | persisted enable state: `"enabled"` or `"disabled"` |
| `hooks` | hooks owned by this script (created through `hook_set` with this path as `owner`), each fully described by `HookInfo` |
| `allocs` | memory allocations owned by this script, as `(symbol name, address)` pairs (created through `script_alloc`/`script_alloc_near`) |

## PointerScanConfig — Pointer-Chain Scan Configuration

The configuration of `pointer_map_snapshot` / `pointer_map_rescan` (v2.12.0).

```cpp
struct PointerScanConfig {
    uintptr_t target = 0;         // value address to reverse-walk from
    uint32_t max_offset = 2048;   // pointer value within target +/- this counts
    uint32_t max_level = 5;       // max chain depth (number of offset levels)
    uint32_t max_results = 10000; // snapshot output cap (anti false-positive)
    std::string module;           // anchor module name; empty = no anchoring
    uint32_t thread_count = 0;    // 0 = hardware_concurrency
};
```

| Field | Meaning |
|-------|---------|
| `target` | the address of the value you are reversing from (e.g. an address found by `pattern_scan`). `pointer_map_snapshot` requires it non-zero |
| `max_offset` | a pointer slot counts as a hit when its stored qword value is within `target ± max_offset` (anti ASLR/rounding jitter); must be > 0 |
| `max_level` | maximum chain depth (number of offset levels walked backward); must be > 0 |
| `max_results` | snapshot output cap; `pointer_map_snapshot` stops emitting chains once this many are collected |
| `module` | anchor module name; when non-empty, only chains whose outermost slot (root) lies inside that module are emitted; empty = any readable region |
| `thread_count` | worker thread count for the scan; 0 = `std::thread::hardware_concurrency()`; only honored where the scan is parallelizable |

## PointerChain — One Pointer Chain

One output row of `pointer_map_snapshot` / `pointer_map_rescan`.

```cpp
struct PointerChain {
    uintptr_t root = 0;               // outermost pointer address (inside the anchor module when anchored)
    std::vector<int64_t> offsets;     // offset sequence (signed; negative when the pointer is stored above the matched field)
};
```

| Field | Meaning |
|-------|---------|
| `root` | the outermost slot address to read first; a base such as a module global or a heap pointer |
| `offsets` | the offset applied after each dereference; elements are signed (`int64_t`) — negative offsets occur when the pointer value is stored above the matched field in memory |

Evaluation convention (matches the CLI `mem batch` consumer):

```
addr = root;
for off in offsets:
    addr = *(uint64_t*)addr + off;
// final addr == the scanned target value (within max_offset)
```

## See Also

- [Result](RESULT.md)
- [ValueType / BreakpointType](ENUMS.md)
- [deeptrace::hook_set](../Modules/HOOK.md#deeptracehook_set)
- [deeptrace::script_status](../Modules/SCRIPT.md#deeptracescript_status)
- [deeptrace::pointer_map_snapshot](../Modules/POINTERSCAN.md#deeptracepointer_map_snapshot)
- [GettingStarted](../GettingStarted.md)

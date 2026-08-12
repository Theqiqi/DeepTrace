# Result Enum

The return type of every deeptrace API. Defined in `deeptrace.h` (via `domain/types.h`).

## Syntax

```cpp
enum class Result {
    Ok = 0, Error, InvalidArg, NotAttached, NoSuchProcess, AccessDenied,
    ReadFault, WriteFault, NotFound, Timeout, NotSupported, AlreadyExists,
    NotExecutable, BadFormat
};
```

Use `deeptrace::result_message(r)` to get a human-readable description string.

## Enum Value Descriptions

| Enum value | Trigger conditions |
|------------|--------------------|
| `Ok` | Operation succeeded. |
| `Error` | Generic failure: a system call returned failure (`TerminateThread` failed, `FreeLibrary` missing prerequisite address, `LoadLibraryA` address missing, injection returned base address 0), output file could not be opened, value formatting failed, **shellcode thread creation failed, `script_alloc`/`hook_set` persist-failure rollback, `script_alloc_near` found no free region within ±2 GB, `hook_clear` write-back failure**, etc. |
| `InvalidArg` | Invalid argument: `pid==0`, `buf==nullptr`, `size==0`, empty `name`, `out`/`out_pid`/`out_base` pointers are `nullptr`, `count==0` or `>10000`, `addr==0`, hardware breakpoint `type>2` or `length∉{1,2,4,8}`, `end<start`, range/size over the limit, empty pattern, **empty shellcode bytes, duplicate script symbol name, `pointer_map_snapshot` with `target==0`/`max_offset==0`/`max_level==0`, `hook_set` with `addr==0` or `newmem==0`, null `out_mask` for `session_permissions`**. |
| `NotAttached` | An API requiring a session (target process) was called without a prior `attach()` (`session_permissions`, `script_*`, `hook_*`, `shellcode_alloc/run/free`, `pointer_map_*` included). |
| `NoSuchProcess` | The process for the given pid does not exist (`OpenProcess` returned NULL). |
| `AccessDenied` | Insufficient privileges: attaching to the target process, suspending/resuming/terminating processes or threads, debug attach, or injection was refused by the system. |
| `ReadFault` | Remote read failed or read byte count was incomplete (`memory_dump`/`memory_readval` require a full read, `disasm` failed to read the first block, `hook_set` could not read the patch region). |
| `WriteFault` | Remote write failed or written byte count was incomplete (inject path/payload writes, underlying `memory_write` failure, `hook_set` patch write, `hook_clear` original-bytes write-back). |
| `NotFound` | Target not found: module name did not match, breakpoint/hardware breakpoint does not exist, watch index out of range, inject record not found, `module_dump` target module does not exist, **`shellcode_run`/`shellcode_free` address has no shellcode record, `script_symbol` name not registered, `hook_clear` target not hooked, `pointer_map_snapshot` anchor module not loaded**. |
| `Timeout` | Wait timed out: `dll_inject` waited more than 15 seconds for the target thread to load the DLL, **`shellcode_free` waited 5 seconds for the run thread to finish (memory is NOT freed)**. |
| `NotSupported` | Reserved; not used by the current implementation. |
| `AlreadyExists` | Duplicate operation: `debug_attach` while already in a debug session, or setting a breakpoint/hardware breakpoint at the same address again. |
| `NotExecutable` | Reserved; not used by the current implementation. |
| `BadFormat` | Input format error: `asm_assemble`/`asm_assemble_labels` instruction could not be assembled or an undefined label/symbol is referenced, `pattern_scan` pattern is malformed (non-hex/invalid wildcard). |

## Usage Example

```cpp
#include "deeptrace.h"
#include <iostream>

int main() {
    uint32_t pid = 1234;
    deeptrace::Result r = deeptrace::attach(pid);
    if (r != deeptrace::Result::Ok) {
        std::cout << "attach failed: " << deeptrace::result_message(r) << "\n";
        return 1;
    }
    deeptrace::detach();
    return 0;
}
```

## See Also

- [deeptrace::result_message](../Modules/PROCESS.md#deeptraceresult_message)
- [GettingStarted](../GettingStarted.md)

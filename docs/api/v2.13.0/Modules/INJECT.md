# Module: Injection

DLL injection and shellcode injection. Injection executes via a remote thread (`CreateRemoteThreadEx`) and requires the target process to allow creating remote threads and writing memory (the `attach` fallback rights already include these capabilities). Injection records are persisted to `%TEMP%/deeptrace_<pid>/injects.dat`.

## deeptrace::dll_inject

### Syntax

```cpp
Result dll_inject(const std::string& path, InjectInfo& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `std::string&` | full DLL path (ASCII; an absolute path is recommended) |
| `out` | `InjectInfo&` | output parameter, injection result (base/thread/state) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | injection succeeded; DLL loaded in the target |
| `Result::InvalidArg` | `path` is empty |
| `Result::NotAttached` | no attached session |
| `Result::WriteFault` | writing the path into the target failed |
| `Result::Timeout` | waiting for the DLL to load exceeded 15 seconds |
| `Result::Error` | could not resolve `LoadLibraryA` or the target reported a load failure (path missing, bitness mismatch, etc.) |
| `Result::AccessDenied` | no create-remote-thread/write-memory permission |

### Description

Remotely allocates memory in the target process, writes the DLL path, creates a thread calling `LoadLibraryA`, and waits up to 15 seconds for the module base. On success the DLL's `DllMain` has already executed; `out.remote_base` is the module base and `out.thread_id` the executing thread. Injecting into a 64-bit target requires a 64-bit DLL. The record is written to `injects.dat`, queryable with `dll_list` and unloadable with `dll_eject`. Protected targets return `AccessDenied`.

Prerequisites: `attach(pid)` done; target not protected. Postconditions: DLL loaded; record persisted.

### Example

```cpp
deeptrace::InjectInfo info;
deeptrace::dll_inject("C:\\tools\\myhack.dll", info);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::dll_eject](#deeptracedll_eject)
- [deeptrace::dll_list](#deeptracedll_list)

---

## deeptrace::dll_eject

### Syntax

```cpp
Result dll_eject(const std::string& path_or_addr);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `path_or_addr` | `const std::string&` | DLL path from an injection record, or a module base starting with `0x` |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | unload requested and record deleted |
| `Result::NotAttached` | no attached session |
| `Result::NotFound` | no record matches this DLL/base in the injection records |
| `Result::Error` | could not resolve the `FreeLibrary` address |

### Description

Unloads a previously injected DLL: creates a remote thread calling `FreeLibrary` and deletes the `injects.dat` record. Matching: when the argument starts with `0x`/`0X`, it matches by module base; otherwise by exact path. Note the `FreeLibrary` call is issued asynchronously — returning `Ok` does not mean the unload thread has finished.

Prerequisites: `attach(pid)` done. Postconditions: DLL unload requested; record deleted.

### Example

```cpp
deeptrace::dll_eject("C:\\tools\\myhack.dll");
// or deeptrace::dll_eject("0x7ff600001000");
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::dll_inject](#deeptracedll_inject)

---

## deeptrace::dll_list

### Syntax

```cpp
Result dll_list(std::vector<InjectInfo>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `out` | `std::vector<InjectInfo>&` | output parameter, DLL records injected by this library |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | query succeeded |
| `Result::NotAttached` | no session |

### Description

Lists all DLL records in `injects.dat` injected by this library. `running` indicates whether the DLL is still actually loaded in the target (compared live against the module list); when the session has no attached handle, `running` is false. Note this list only contains DLLs injected by this library, not modules the target loaded itself (use `module_list` for module queries).

Prerequisites: `attach(pid)` done (handle optional; state fields not refreshed without it). Postconditions: none.

### Example

```cpp
std::vector<deeptrace::InjectInfo> list;
deeptrace::dll_list(list);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::dll_inject](#deeptracedll_inject)
- [deeptrace::module_list](MODULE.md#deeptracemodule_list)

---

## deeptrace::dll_status

### Syntax

```cpp
Result dll_status(std::vector<InjectInfo>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `out` | `std::vector<InjectInfo>&` | output parameter, injected DLL state list |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | query succeeded |
| `Result::NotAttached` | no session |

### Description

An alias of `dll_list` with identical behavior. Kept to semantically distinguish "querying records" from "querying state".

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
std::vector<deeptrace::InjectInfo> list;
deeptrace::dll_status(list);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::dll_list](#deeptracedll_list)

---

## deeptrace::shellcode_inject

### Syntax

```cpp
Result shellcode_inject(const std::vector<uint8_t>& bytes, InjectInfo& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `bytes` | `const std::vector<uint8_t>&` | shellcode machine-code bytes, non-empty |
| `out` | `InjectInfo&` | output parameter, injection result (allocation address/thread) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | executable memory allocated and remote thread started |
| `Result::InvalidArg` | `bytes` is empty |
| `Result::NotAttached` | no attached session |
| `Result::WriteFault` | payload write failed |
| `Result::AccessDenied` | no allocate-executable-memory/create-thread permission |

### Description

Allocates `PAGE_EXECUTE_READWRITE` memory in the target process, writes the shellcode bytes, and immediately creates a remote thread to execute them. `out.remote_base` is the allocation address and `out.thread_id` the executing thread. **Does not wait for the execution result** (shellcode usually has no return convention). The record is written to `injects.dat`; `shellcode_status` can query whether the thread is still running. `asm_assemble` can generate the payload.

Prerequisites: `attach(pid)` done. Postconditions: shellcode executed in the target; record persisted.

### Example

```cpp
std::vector<uint8_t> code = {0x48, 0x31, 0xC0, 0xC3};  // xor rax,rax; ret
deeptrace::InjectInfo info;
deeptrace::shellcode_inject(code, info);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::shellcode_inject_at](#deeptraceshellcode_inject_at)
- [deeptrace::asm_assemble](ASM.md#deeptraceasm_assemble)

---

## deeptrace::shellcode_inject_at

### Syntax

```cpp
Result shellcode_inject_at(uintptr_t addr, const std::vector<uint8_t>& bytes,
                           InjectInfo& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | write-and-execute address inside the target process; must not be 0 |
| `bytes` | `const std::vector<uint8_t>&` | shellcode machine-code bytes |
| `out` | `InjectInfo&` | output parameter, injection result |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | written and remote thread started |
| `Result::InvalidArg` | `bytes` is empty or `addr == 0` |
| `Result::NotAttached` | no attached session |
| `Result::WriteFault` | write failed (address not writable/read-only page) |
| `Result::AccessDenied` | no create-remote-thread permission |

### Description

Writes shellcode to a **specified address** in the target process (no self-allocation) and starts a remote thread from that address. Before writing, ensure the target address is writable and is valid executable memory (e.g. reserved with `VirtualAllocEx` and adjusted to executable attributes, or a cave in a module). Suitable for reusing a fixed buffer or avoiding changes to the target's memory layout.

Prerequisites: `attach(pid)` done; target address writable. Postconditions: shellcode executed in the target; record persisted.

### Example

```cpp
deeptrace::shellcode_inject_at(0x140001000, code, info);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::shellcode_inject](#deeptraceshellcode_inject)

---

## deeptrace::shellcode_status

### Syntax

```cpp
Result shellcode_status(std::vector<InjectInfo>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `out` | `std::vector<InjectInfo>&` | output parameter, shellcode injection records and run state |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | query succeeded |
| `Result::NotAttached` | no session |

### Description

Lists the shellcode injection records in `injects.dat`; `running` indicates whether the corresponding remote thread is still running (checked via `GetExitCodeThread`; `STILL_ACTIVE` means running). Threads that have ended have `running` false, but the record is kept.

Prerequisites: `attach(pid)` done (handle optional; state fields not refreshed without it). Postconditions: none.

### Example

```cpp
std::vector<deeptrace::InjectInfo> list;
deeptrace::shellcode_status(list);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::shellcode_inject](#deeptraceshellcode_inject)
- [deeptrace::shellcode_alloc](#deeptraceshellcode_alloc)

---

## deeptrace::shellcode_alloc

### Syntax

```cpp
Result shellcode_alloc(const std::vector<uint8_t>& bytes, InjectInfo& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `bytes` | `const std::vector<uint8_t>&` | shellcode machine-code bytes, non-empty |
| `out` | `InjectInfo&` | output parameter, allocation result (address/size) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | executable memory allocated and bytes written; **nothing executed yet** |
| `Result::InvalidArg` | `bytes` is empty |
| `Result::NotAttached` | no attached session |
| `Result::WriteFault` | payload write failed (allocation is freed on this path) |
| `Result::Error` | `VirtualAllocEx` failed |
| `Result::AccessDenied` | no allocate-executable-memory right |

### Description

Allocates `PAGE_EXECUTE_READWRITE` memory in the target process and writes `bytes` into it, but — unlike `shellcode_inject` — **does not create a thread**. This is the "prepare" stage of a decoupled shellcode lifecycle: `shellcode_alloc` → `shellcode_run` (trigger once, repeatable) → `shellcode_free` (release). The allocation is recorded in `injects.dat` (`kind=shellcode`, `running=false`), so `shellcode_run`/`shellcode_free` can find it by address and `shellcode_status` shows it. Useful when the payload must be written first and executed later, or executed many times at a fixed address (e.g. a small routine called by a hook or by repeated `thread_create_at`).

Prerequisites: `attach(pid)` done. Postconditions: memory allocated and written, record persisted, nothing executed.

### Example

```cpp
std::vector<uint8_t> code = {0x48, 0x31, 0xC0, 0xC3};  // xor rax,rax; ret
deeptrace::InjectInfo info;
deeptrace::shellcode_alloc(code, info);
// info.remote_base is the address to run/free later
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::shellcode_run](#deeptraceshellcode_run)
- [deeptrace::shellcode_free](#deeptraceshellcode_free)

---

## deeptrace::shellcode_run

### Syntax

```cpp
Result shellcode_run(uintptr_t addr, InjectInfo& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | the allocation address returned by `shellcode_alloc` (must match a recorded shellcode record) |
| `out` | `InjectInfo&` | output parameter, run result (address/thread id) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | remote thread started at `addr`; `out.thread_id` set, `out.running = true` |
| `Result::NotAttached` | no attached session |
| `Result::NotFound` | no shellcode record matches `addr` |
| `Result::AccessDenied` | no `PROCESS_CREATE_THREAD` right |
| `Result::Error` | `CreateRemoteThreadEx` failed (record unchanged) |

### Description

Executes the shellcode previously prepared by `shellcode_alloc` at the recorded address: creates a remote thread whose entry is `addr`. **Repeatable** — each call spawns a new thread, so the same payload can be triggered multiple times (idempotent payloads only; the payload must tolerate re-entry). The record's `thread_id` is updated to the latest run thread and persisted. No wait for completion.

Prerequisites: `attach(pid)` done; `addr` from a prior `shellcode_alloc` (or `shellcode_inject`, whose records share the same store). Postconditions: a remote thread is running the payload; record updated.

### Example

```cpp
deeptrace::InjectInfo run_info;
deeptrace::shellcode_run(addr, run_info);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::shellcode_alloc](#deeptraceshellcode_alloc)
- [deeptrace::shellcode_free](#deeptraceshellcode_free)

---

## deeptrace::shellcode_free

### Syntax

```cpp
Result shellcode_free(uintptr_t addr);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | the allocation address of the shellcode record to release |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | memory released and record removed |
| `Result::NotAttached` | no attached session |
| `Result::NotFound` | no shellcode record matches `addr` |
| `Result::Timeout` | the run thread is still running after a 5-second wait; memory is **not** freed (avoiding use-after-free crashes) |
| `Result::Error` | record-save failed (memory **not** freed, keeping the record consistent) or `VirtualFreeEx` failed (leak, but no dangling record) |

### Description

Releases the memory allocated by `shellcode_alloc` and removes its record — the "cleanup" stage of the lifecycle. Safety-first ordering: if the record has a run thread, it waits up to 5 seconds for that thread to exit (`Timeout` and no free if it is still running, to never free memory under executing code); it saves the store **before** freeing so a save failure leaves the record pointing at still-valid memory; only then `VirtualFreeEx`. A thread that already ended is treated as finished.

Prerequisites: `attach(pid)` done; no still-running thread of the payload (else `Timeout`). Postconditions: on `Ok`, memory freed and record gone.

### Example

```cpp
if (deeptrace::shellcode_free(addr) == deeptrace::Result::Ok) {
    // cleanly released
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::shellcode_alloc](#deeptraceshellcode_alloc)
- [deeptrace::shellcode_run](#deeptraceshellcode_run)

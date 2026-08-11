# Module: Debug

Debug session (attach/detach), pause/resume, single-stepping, breakpoints (software/hardware/page guard), and register access. The debug session builds on the process session: **`attach(pid)` first, then `debug_attach()`**.

Breakpoint records are persisted to `%TEMP%/deeptrace_<pid>/breaks.dat`; they remain visible and clearable via `debug_status` after reopening a session on the same target process.

## deeptrace::debug_attach

### Syntax

```cpp
Result debug_attach();
```

### Parameters

None.

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | entered debug mode |
| `Result::NotAttached` | `attach(pid)` not done |
| `Result::AlreadyExists` | already in debug mode |
| `Result::AccessDenied` | no debug privileges (administrator/SeDebugPrivilege required) |

### Description

Calls `DebugActiveProcess` on the session target process to enter debug mode. In debug mode, debugger-dependent capabilities such as single-stepping and hardware breakpoints are available. Windows requires the debugger to call `DebugActiveProcessStop` before exiting (the library handles this in `detach`/`debug_detach`); otherwise the debuggee process is terminated along with the debugger. Debug mode is an enhanced state of the process session: `debug_detach` only exits debug mode and keeps the process session.

Prerequisites: `attach(pid)` done. Postconditions: entered debug mode; call `debug_detach` or `detach` before finishing.

### Example

```cpp
deeptrace::attach(pid);
if (deeptrace::debug_attach() == deeptrace::Result::Ok) {
    // ... debug operations ...
    deeptrace::debug_detach();
}
deeptrace::detach();
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::debug_detach](#deeptracedebug_detach)

---

## deeptrace::debug_detach

### Syntax

```cpp
Result debug_detach();
```

### Parameters

None.

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | exited debug mode |
| `Result::NotAttached` | not in debug mode |
| `Result::Error` | `DebugActiveProcessStop` failed |

### Description

Ends debug mode (does not close the process session). On failure it returns `Error` but the library still clears the debug state flag, avoiding repeated retry attempts. After exiting debug mode the debuggee process continues running independently; breakpoints (INT3) that were not cleared will cause target anomalies — clear breakpoints before finishing.

Prerequisites: `debug_attach()` done. Postconditions: debug mode exited; process session kept.

### Example

```cpp
deeptrace::debug_detach();
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::debug_attach](#deeptracedebug_attach)
- [deeptrace::breakpoint_clear](#deeptracebreakpoint_clear)

---

## deeptrace::debug_pause

### Syntax

```cpp
Result debug_pause();
```

### Parameters

None.

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | all target threads suspended |
| `Result::NotAttached` | no attached session |

### Description

Suspends all threads of the session target process (equivalent to `suspend_process(session.pid)` but acting on the session target). Used to freeze the target for memory modification or breakpoint debugging. Resume with `debug_resume`.

Prerequisites: `attach(pid)` done. Postconditions: target paused; must be resumed with `debug_resume`.

### Example

```cpp
deeptrace::debug_pause();
// ... modify the target ...
deeptrace::debug_resume();
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::debug_resume](#deeptracedebug_resume)

---

## deeptrace::debug_resume

### Syntax

```cpp
Result debug_resume();
```

### Parameters

None.

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | all target threads resumed |
| `Result::NotAttached` | no attached session |

### Description

Resumes the session target process threads suspended by `debug_pause`. Must pair with the suspend call.

Prerequisites: target suspended by `debug_pause`. Postconditions: target resumes execution.

### Example

```cpp
deeptrace::debug_resume();
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::debug_pause](#deeptracedebug_pause)

---

## deeptrace::debug_step

### Syntax

```cpp
Result debug_step(uint32_t tid, uintptr_t* out_rip);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `tid` | `uint32_t` | target thread ID; 0 = session first thread |
| `out_rip` | `uintptr_t*` | optional, RIP value after the step; pass `nullptr` to ignore |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | step completed; `*out_rip` is the next instruction address |
| `Result::NotAttached` | `attach(pid)` not done |
| `Result::AccessDenied` | debug attach failed (insufficient privileges) |

### Description

Single-steps one instruction of the target thread (sets the trap flag and waits for `EXCEPTION_SINGLE_STEP`). If not yet in debug mode, it automatically runs a one-shot "attach → step → detach" flow (the CLI's non-interactive mode is designed for this); when already in debug mode it steps directly. `tid=0` means the first thread of the target process. After the step, `*out_rip` reveals the current execution position; combined with `disasm_at` you can trace the code flow.

Prerequisites: `attach(pid)` done. Postconditions: the target executed one instruction.

### Example

```cpp
uintptr_t rip = 0;
if (deeptrace::debug_step(0, &rip) == deeptrace::Result::Ok) {
    // rip is the next instruction address
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::debug_step_over](#deeptracedebug_step_over)

---

## deeptrace::debug_step_over

### Syntax

```cpp
Result debug_step_over(uint32_t tid, uintptr_t* out_rip);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `tid` | `uint32_t` | target thread ID; 0 = session first thread |
| `out_rip` | `uintptr_t*` | optional, RIP value after the step |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | step-over completed |
| `Result::NotAttached` | `attach(pid)` not done |
| `Result::AccessDenied` | debug attach failed |

### Description

"Step over": if the current instruction is a near call, it sets a temporary software breakpoint at the return address, runs until it hits, then restores — overall it does not enter the function body; otherwise it behaves like `debug_step`. It also supports the one-shot attach flow without a debug session. Suitable for skipping function calls while stepping line by line.

Prerequisites: `attach(pid)` done. Postconditions: the target executed to the next line of the current function.

### Example

```cpp
uintptr_t rip = 0;
deeptrace::debug_step_over(0, &rip);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::debug_step](#deeptracedebug_step)

---

## deeptrace::debug_continue

### Syntax

```cpp
Result debug_continue(uint32_t timeout_ms, ContinueInfo& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `timeout_ms` | `uint32_t` | wait timeout in milliseconds; the call returns `Ok` with no stop reason when the timeout elapses |
| `out` | `ContinueInfo&` | output parameter, stop reason (breakpoint hit / other exception / process exit / timeout) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | the debuggee was resumed and the call returned — inspect `out` (hit / exited / exit_code / exception / address / rip / tid) to tell the stop reason |
| `Result::NotAttached` | no debug session (requires `debug_attach()`) |
| `Result::AccessDenied` | insufficient privileges to wait on debug events |
| `Result::Error` | `WaitForDebugEvent`/`ContinueDebugEvent` failed |

### Description

Resumes the debuggee (equivalent to a debugger's "run/free run") and waits for the next debug event for up to `timeout_ms` milliseconds. Four stop reasons are reported through `ContinueInfo`:

1. **Software breakpoint hit** — a self-set software breakpoint (`breakpoint_set`) was executed: the library restores the original byte, single-steps the breakpoint instruction, re-arms the INT3, and reports the breakpoint address in `out.address` together with the post-instruction RIP in `out.rip` (`out.hit = true`). The target stays paused.
2. **Other exception** — a hardware breakpoint, page guard, or unhandled exception occurred: the exception code and address are reported (`out.hit = true`, `out.exception`, `out.address`); the exception is **not** consumed, and the target stays paused.
3. **Process exit** — the debuggee exited: `out.exited = true` and `out.exit_code` is the exit code.
4. **Timeout** — no event within `timeout_ms`: `Ok` with `out.hit = false` and `out.exited = false`.

Behavior notes: the system loader breakpoint (`EXCEPTION_BREAKPOINT` raised at attach) is skipped internally, and detach events are drained, so a fresh `debug_continue` reliably waits for a user-set breakpoint; RIP rollback (reporting the instruction before the breakpoint) applies only to self-set software breakpoints. Outside debug mode the function returns `NotAttached`; breakpoint-hit condition filtering and automatic consumption of single-step exceptions are not supported.

Prerequisites: `debug_attach()` done. Postconditions: the debuggee ran until the stop reason or timeout; the target remains paused on a hit.

### Example

```cpp
deeptrace::ContinueInfo info;
if (deeptrace::debug_continue(5000, info) == deeptrace::Result::Ok) {
    if (info.hit) {
        std::cout << "stopped: exception=0x" << std::hex << info.exception
                  << " addr=" << info.address << " rip=" << info.rip << "\n";
    } else if (info.exited) {
        std::cout << "target exited with code " << info.exit_code << "\n";
    } else {
        std::cout << "timeout\n";
    }
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::breakpoint_set](#deeptracebreakpoint_set)
- [deeptrace::debug_pause](#deeptracedebug_pause)
- [deeptrace::debug_resume](#deeptracedebug_resume)
- [ContinueInfo](../Types/STRUCTS.md#continueinfo--debug-continue-stop-reason)

---

## deeptrace::breakpoint_set

### Syntax

```cpp
Result breakpoint_set(uintptr_t addr, BreakpointInfo& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | breakpoint address; must not be 0 |
| `out` | `BreakpointInfo&` | output parameter, breakpoint info (original byte etc.) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | breakpoint set and persisted |
| `Result::InvalidArg` | `addr == 0` |
| `Result::NotAttached` | no attached session |
| `Result::AlreadyExists` | a software breakpoint already exists at this address |
| `Result::ReadFault` | target address unreadable (cannot save the original byte) |
| `Result::WriteFault` | target address not writable (cannot write 0xCC) |

### Description

Sets a software breakpoint at `addr` in the target process: reads and saves the original byte, rewrites it to `0xCC` (INT3), and records it in `%TEMP%/deeptrace_<pid>/breaks.dat`. The target triggers a single-step exception when execution reaches this address. Persistence means the breakpoint can be restored/cleared after reopening a session. Clear with `breakpoint_clear`. Note: the target code has been modified after setting the breakpoint; detaching from debug without clearing breakpoints can cause target anomalies.

Prerequisites: `attach(pid)` done; the code at `addr` is readable and writable. Postconditions: target code rewritten and record persisted.

### Example

```cpp
deeptrace::BreakpointInfo bp;
if (deeptrace::breakpoint_set(0x140001000, bp) == deeptrace::Result::Ok) {
    // bp.original_byte is the original byte
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::breakpoint_clear](#deeptracebreakpoint_clear)
- [deeptrace::hw_breakpoint_set](#deeptracehw_breakpoint_set)

---

## deeptrace::breakpoint_clear

### Syntax

```cpp
Result breakpoint_clear(uintptr_t addr);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | breakpoint address |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | breakpoint removed |
| `Result::NotAttached` | no attached session |
| `Result::NotFound` | no software breakpoint record at this address |
| `Result::WriteFault` | restoring the original byte failed |

### Description

Clears the software breakpoint at `addr`: writes the original byte from the persisted record back to the target memory and deletes the record. Returns `NotFound` if there is no record for this address. Returns `WriteFault` if the restore failed (the record is kept; retry possible).

Prerequisites: `attach(pid)` done. Postconditions: target code restored to its original state.

### Example

```cpp
deeptrace::breakpoint_clear(0x140001000);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::breakpoint_set](#deeptracebreakpoint_set)

---

## deeptrace::hw_breakpoint_set

### Syntax

```cpp
Result hw_breakpoint_set(uintptr_t addr, uint32_t type, uint32_t length);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | breakpoint address; must not be 0 |
| `type` | `uint32_t` | trigger type: 0=execute, 1=write, 2=read/write |
| `length` | `uint32_t` | monitored length; only 1/2/4/8 bytes allowed |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | hardware breakpoint set on the DR registers of all threads |
| `Result::InvalidArg` | `addr == 0`, `type > 2`, or `length ∉ {1,2,4,8}` |
| `Result::NotAttached` | no attached session |
| `Result::AlreadyExists` | a hardware breakpoint already exists at this address |
| `Result::Error` | no free DR slot (max 4) |

### Description

Sets a hardware breakpoint using the x64 debug registers DR0-DR3, **without modifying target code** (works on read-only/protected code pages too — the key advantage over software breakpoints). The type/length are encoded per Intel DR7 semantics and applied to all target threads. At most 4 hardware breakpoints; no free slot returns `Error`. Records are persisted to `breaks.dat`. Clear with `hw_breakpoint_clear`. Setting does not require a prior `debug_attach` (the CLI's `debug hbreak` works without it), but needs administrator privileges to access the target thread contexts; the exception raised by a DR breakpoint needs a debugger to handle — without a debug session the target may crash outright, so using it together with `debug_attach` is recommended.

Prerequisites: `attach(pid)` and `debug_attach()` done. Postconditions: target thread DR registers modified.

### Example

```cpp
deeptrace::hw_breakpoint_set(0x140001000, 0 /*execute*/, 1);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::hw_breakpoint_clear](#deeptracehw_breakpoint_clear)

---

## deeptrace::hw_breakpoint_clear

### Syntax

```cpp
Result hw_breakpoint_clear(uintptr_t addr);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | hardware breakpoint address |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | hardware breakpoint cleared |
| `Result::NotAttached` | no attached session |
| `Result::NotFound` | no hardware breakpoint record at this address |

### Description

Clears the hardware breakpoint at `addr`: resets the corresponding DR slot on all threads and deletes the persisted record.

Prerequisites: `attach(pid)` done. Postconditions: DR registers reset.

### Example

```cpp
deeptrace::hw_breakpoint_clear(0x140001000);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::hw_breakpoint_set](#deeptracehw_breakpoint_set)

---

## deeptrace::guard_set

### Syntax

```cpp
Result guard_set(uintptr_t addr, size_t size);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | start address of the target region |
| `size` | `size_t` | region size (rounded up to page alignment) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | page guard set |
| `Result::NotAttached` | no attached session |
| `Result::Error` | `VirtualProtectEx` failed |

### Description

Sets the `PAGE_GUARD` attribute (along with `PAGE_EXECUTE_READWRITE`) on the region starting at `addr` in the target process, causing one guard exception on access to the region. Suitable for monitoring/intercepting reads and writes to specific data. Guards are one-shot: after triggering, the system clears the attribute and it must be re-set. Clear with `guard_clear`.

Prerequisites: `attach(pid)` done. Postconditions: the target region carries the PAGE_GUARD attribute.

### Example

```cpp
deeptrace::guard_set(0x140001000, 0x1000);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::guard_clear](#deeptraceguard_clear)

---

## deeptrace::guard_clear

### Syntax

```cpp
Result guard_clear(uintptr_t addr, size_t size);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | start address of the target region |
| `size` | `size_t` | region size |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | guard removed |
| `Result::NotAttached` | no attached session |
| `Result::Error` | `VirtualProtectEx` failed |

### Description

Restores the region set by `guard_set` to `PAGE_EXECUTE_READWRITE` (clearing the PAGE_GUARD bit). After setting guards multiple times, call this function to restore so the target's later accesses are not intercepted.

Prerequisites: `attach(pid)` done. Postconditions: the target region restored to normal attributes.

### Example

```cpp
deeptrace::guard_clear(0x140001000, 0x1000);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::guard_set](#deeptraceguard_set)

---

## deeptrace::debug_status

### Syntax

```cpp
Result debug_status(DebugStatus& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `out` | `DebugStatus&` | output parameter, current debug/session state |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | query succeeded |

### Description

Returns the current session and debug state: whether attached (debug mode or process session), the session pid, and the software/hardware breakpoint counts (based on persisted records). **Does not require a session**. Suitable for showing state at a flow entry or for consistency checks.

Prerequisites: none. Postconditions: none.

### Example

```cpp
deeptrace::DebugStatus st;
deeptrace::debug_status(st);
std::cout << "attached=" << st.attached << " bp=" << st.breakpoint_count << "\n";
```

### Header

```cpp
#include "deeptrace.h"
```

---

## deeptrace::registers_get

### Syntax

```cpp
Result registers_get(std::vector<RegisterInfo>& out, uint32_t tid);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `out` | `std::vector<RegisterInfo>&` | output parameter, register name/value list |
| `tid` | `uint32_t` | target thread ID; 0 = session first thread |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | read succeeded |
| `Result::NotAttached` | no attached session |

### Description

Reads the full register set of the target thread (general-purpose registers, RIP, RSP, EFLAGS, and debug registers etc.) into `out`. Register reads do not require debug mode (based on `GetThreadContext`). `tid=0` means the first thread. Typical uses: observing register state after a single step, analyzing calling-convention argument passing.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
std::vector<deeptrace::RegisterInfo> regs;
deeptrace::registers_get(regs, 0);
for (const auto& r : regs) {
    std::cout << r.name << " = " << r.value << "\n";
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::register_get](#deeptraceregister_get)

---

## deeptrace::register_get

### Syntax

```cpp
Result register_get(const std::string& name, uint64_t* out_value, uint32_t tid);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `const std::string&` | register name (e.g. `"rax"`, `"rip"`, `"eflags"`) |
| `out_value` | `uint64_t*` | output parameter, register value |
| `tid` | `uint32_t` | target thread ID; 0 = session first thread |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | read succeeded |
| `Result::InvalidArg` | `out_value == nullptr` |
| `Result::NotAttached` | no attached session |

### Description

The single-register version of `registers_get`, returning one register's value by name. Register names are case-insensitive (e.g. `"RAX"` equals `"rax"`). An unknown register name returns `Result::Error` (caused by an underlying parse failure). Suitable for quickly sampling one key register.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
uint64_t rax = 0;
deeptrace::register_get("rax", &rax, 0);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::registers_get](#deeptraceregisters_get)

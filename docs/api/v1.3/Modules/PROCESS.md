# Module: Process & Session

Session management is the entry point for all target-process operations. `attach` establishes a session, `detach` closes it; query/control functions that operate by pid (`process_info`, `suspend_process`, `resume_process`, `terminate_process`) do not require a session. `result_message` is a general-purpose error-code description utility.

## deeptrace::result_message

### Syntax

```cpp
const char* result_message(Result r);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `r` | `Result` | any error-code enum value |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `const char*` | human-readable English description of the error code; unknown values return `"Unknown"` |

### Description

Converts a `Result` enum value into a human-readable static string, used for logs and error messages. This function never fails, allocates no memory, and requires no session. The CLI's error output is based on this function. Typical use: capture any non-`Ok` value returned by an API and print the reason, combined with `Result` checks to drive flow branches.

Prerequisites: none. Postconditions: none.

### Example

```cpp
if (deeptrace::attach(pid) != deeptrace::Result::Ok) {
    // the caller decides what to print; the description text can be printed directly
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [Types/RESULT.md](../Types/RESULT.md)

---

## deeptrace::enumerate_processes

### Syntax

```cpp
Result enumerate_processes(std::vector<ProcessInfo>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `out` | `std::vector<ProcessInfo>&` | output parameter, filled with the current list of all system processes |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | enumeration succeeded; `out` is filled |
| `Result::Error` | system snapshot creation failed |

### Description

Enumerates all processes on the system and writes them to `out`; each entry contains pid, image name, parent pid, and thread count. No `attach` or administrator privileges required. Typical scenario: list processes so the user can pick a target, then `attach` to it. This call takes a single system snapshot; processes started after the snapshot will not appear in the list.

Prerequisites: none. Postconditions: none.

### Example

```cpp
std::vector<deeptrace::ProcessInfo> procs;
if (deeptrace::enumerate_processes(procs) == deeptrace::Result::Ok) {
    for (const auto& p : procs) {
        std::wcout << p.pid << L"  " << p.name << L"\n";
    }
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::attach](#deeptraceattach)

---

## deeptrace::attach

### Syntax

```cpp
Result attach(uint32_t pid);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `pid` | `uint32_t` | target process ID; must not be 0 |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | session established; target-process operations are now available |
| `Result::InvalidArg` | `pid == 0` |
| `Result::NoSuchProcess` | the target process does not exist |
| `Result::AccessDenied` | insufficient privileges to open a handle to the target process |

### Description

Opens a handle to the target process and establishes the global session (recording both pid and handle). The library first tries `PROCESS_ALL_ACCESS`; on failure it falls back to a combination of query/read-write/create-thread/suspend-resume rights to support read-only scenarios. After the session is established, the memory, module, thread, debug, resolve, watch, and inject APIs become callable; otherwise these APIs return `NotAttached`. Only one session exists per process at a time; a repeated `attach` replaces the old session. Process 0 is the system idle process and can never be attached, so it is explicitly rejected.

Prerequisites: none. Postconditions: session active; should be closed via `detach`.

### Example

```cpp
deeptrace::Result r = deeptrace::attach(1234);
if (r == deeptrace::Result::Ok) {
    // ... memory/debug operations ...
    deeptrace::detach();
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::detach](#deeptracedetach)
- [deeptrace::session_pid](#deeptracesession_pid)
- [deeptrace::debug_attach](DEBUG.md#deeptracedebug_attach)

---

## deeptrace::detach

### Syntax

```cpp
Result detach();
```

### Parameters

None.

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | session closed (whether or not previously in debug mode) |

### Description

Closes the current session: if in debug mode, first calls `DebugActiveProcessStop` to end the debug session (Windows requires this — a debugger that exits without calling it causes the debuggee process to be terminated along with it, so detaching first is mandatory), then closes the process handle and clears the pid. This function always succeeds. The CLI calls it automatically after every command to guarantee the target process is not terminated unexpectedly.

Prerequisites: none (safe even when not attached). Postconditions: session closed; all session-dependent APIs return `NotAttached`.

### Example

```cpp
deeptrace::attach(pid);
// ... operations ...
deeptrace::detach();
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::attach](#deeptraceattach)
- [deeptrace::debug_detach](DEBUG.md#deeptracedebug_detach)

---

## deeptrace::process_info

### Syntax

```cpp
Result process_info(uint32_t pid, ProcessInfo& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `pid` | `uint32_t` | target process ID (may be 0, returns the system process) |
| `out` | `ProcessInfo&` | output parameter, process information |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | query succeeded |
| `Result::NoSuchProcess` | the process does not exist |
| `Result::AccessDenied` | no query permission |

### Description

Queries a single process's image name, parent, and thread count by pid, writing to `out`. **Does not require a session**, suitable for confirming target info before `attach`. Unlike `enumerate_processes`, this function opens and queries the target process live, so results are not affected by snapshot timing.

Prerequisites: none. Postconditions: none.

### Example

```cpp
deeptrace::ProcessInfo info;
if (deeptrace::process_info(pid, info) == deeptrace::Result::Ok) {
    std::wcout << L"parent=" << info.parent_pid << L" threads=" << info.thread_count << L"\n";
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::enumerate_processes](#deeptraceenumerate_processes)

---

## deeptrace::suspend_process

### Syntax

```cpp
Result suspend_process(uint32_t pid);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `pid` | `uint32_t` | target process ID |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | all threads suspended |
| `Result::InvalidArg` | `pid == 0` |
| `Result::NoSuchProcess` | the process does not exist |
| `Result::AccessDenied` | no `PROCESS_SUSPEND_RESUME` right |

### Description

Suspends all threads of the target process, pausing its execution. **Does not require a session**. In debugger scenarios it is commonly used together with memory modification or breakpoint setup. Resume with `resume_process`. Note: the target stops responding while suspended; if the caller is itself the target process, this deadlocks.

Prerequisites: none. Postconditions: target process paused; must be resumed with `resume_process`.

### Example

```cpp
deeptrace::suspend_process(pid);
// ... modify memory ...
deeptrace::resume_process(pid);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::resume_process](#deeptraceresume_process)

---

## deeptrace::resume_process

### Syntax

```cpp
Result resume_process(uint32_t pid);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `pid` | `uint32_t` | target process ID |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | all threads resumed |
| `Result::InvalidArg` | `pid == 0` |
| `Result::NoSuchProcess` | the process does not exist |
| `Result::AccessDenied` | no suspend/resume right |

### Description

Resumes threads suspended by `suspend_process`. **Does not require a session**. Each thread's suspend count is decremented by one; threads only truly resume when the count reaches zero, so resume calls must pair one-to-one with suspend calls.

Prerequisites: the target process was suspended. Postconditions: the target process resumes execution.

### Example

```cpp
deeptrace::resume_process(pid);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::suspend_process](#deeptracesuspend_process)

---

## deeptrace::terminate_process

### Syntax

```cpp
Result terminate_process(uint32_t pid, uint32_t exit_code);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `pid` | `uint32_t` | target process ID |
| `exit_code` | `uint32_t` | process exit code (e.g. 0 for normal) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | termination requested |
| `Result::InvalidArg` | `pid == 0` |
| `Result::NoSuchProcess` | the process does not exist |
| `Result::AccessDenied` | no `PROCESS_TERMINATE` right |

### Description

Forcibly terminates the target process with exit code `exit_code`. **Does not require a session**. This operation is **irreversible** and performs no graceful cleanup (no DLL unload hooks etc.); confirm the target before calling. Critical system processes may return `AccessDenied`.

Prerequisites: none. Postconditions: target process terminated.

### Example

```cpp
deeptrace::terminate_process(pid, 0);
```

### Header

```cpp
#include "deeptrace.h"
```

---

## deeptrace::session_pid

### Syntax

```cpp
Result session_pid(uint32_t* out_pid);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `out_pid` | `uint32_t*` | output parameter, current session target pid (0 when not attached) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | query succeeded |
| `Result::InvalidArg` | `out_pid == nullptr` |

### Description

Returns the current global session's target pid, writing 0 when not attached. Used to confirm the current operation target in complex flows, or to check whether a session exists.

Prerequisites: none. Postconditions: none.

### Example

```cpp
uint32_t cur = 0;
deeptrace::session_pid(&cur);
if (cur != 0) { /* a session exists */ }
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::attach](#deeptraceattach)

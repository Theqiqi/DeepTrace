# Module: Threads

Thread enumeration and thread-level control. `thread_list` requires an `attach` to the target process; `thread_suspend`/`thread_resume`/`thread_terminate` operate directly by tid and do not require a session.

## deeptrace::thread_list

### Syntax

```cpp
Result thread_list(std::vector<ThreadInfo>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `out` | `std::vector<ThreadInfo>&` | output parameter, thread list of the session target process |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | enumeration succeeded |
| `Result::NotAttached` | no attached session |

### Description

Enumerates all threads of the session target process (tid, priority, entry address). In debugging scenarios it is commonly used to pick the thread to single-step or read registers from (the tid-0-means-first-thread convention is described in `debug_step`, `registers_get`).

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
std::vector<deeptrace::ThreadInfo> threads;
deeptrace::thread_list(threads);
for (const auto& t : threads) {
    std::cout << "tid=" << t.tid << " prio=" << t.priority << "\n";
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::registers_get](DEBUG.md#deeptraceregisters_get)

---

## deeptrace::thread_suspend

### Syntax

```cpp
Result thread_suspend(uint32_t tid);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `tid` | `uint32_t` | target thread ID; must not be 0 |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | suspended |
| `Result::InvalidArg` | `tid == 0` |
| `Result::AccessDenied` | no `THREAD_SUSPEND_RESUME` right |
| `Result::NoSuchProcess` | thread does not exist |

### Description

Suspends a single thread (suspend count +1). **Does not require a session**. Suspending a thread can pause one specific execution flow, commonly used to freeze a worker or game-logic thread in multi-threaded targets. The suspend count must be decremented with a paired `thread_resume`.

Prerequisites: none. Postconditions: the thread pauses execution.

### Example

```cpp
deeptrace::thread_suspend(tid);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::thread_resume](#deeptracethread_resume)

---

## deeptrace::thread_resume

### Syntax

```cpp
Result thread_resume(uint32_t tid);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `tid` | `uint32_t` | target thread ID; must not be 0 |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | resumed |
| `Result::InvalidArg` | `tid == 0` |
| `Result::AccessDenied` | no suspend/resume right |
| `Result::NoSuchProcess` | thread does not exist |

### Description

Resumes a thread suspended by `thread_suspend`. **Does not require a session**. The thread does not resume until the suspend count reaches zero; resume must pair one-to-one with the suspend call.

Prerequisites: the thread was suspended. Postconditions: the thread resumes execution.

### Example

```cpp
deeptrace::thread_resume(tid);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::thread_suspend](#deeptracethread_suspend)

---

## deeptrace::thread_terminate

### Syntax

```cpp
Result thread_terminate(uint32_t tid, uint32_t exit_code);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `tid` | `uint32_t` | target thread ID; must not be 0 |
| `exit_code` | `uint32_t` | thread exit code |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | terminated |
| `Result::InvalidArg` | `tid == 0` |
| `Result::Error` | `TerminateThread` system call failed |
| `Result::AccessDenied` | no `THREAD_TERMINATE` right |
| `Result::NoSuchProcess` | thread does not exist |

### Description

Forcibly terminates the target thread (based on `TerminateThread`). **Does not require a session**. This operation is **irreversible** and performs no thread cleanup (does not release the stack/DLL locks), which can make the target process unstable; terminating the main thread usually exits the process. Confirm before calling.

Prerequisites: none. Postconditions: thread terminated.

### Example

```cpp
deeptrace::thread_terminate(tid, 0);
```

### Header

```cpp
#include "deeptrace.h"
```

# Module: Script Engine (AA-style)

Script symbols, per-PID-persisted allocations, and script enable-state records. These APIs back the CLI's `script` command group (the AA/CETrainer-style script engine) and are the building blocks for the `alloc`/`registersymbol`/`dealloc` and `enable`/`disable` concepts: a script allocates named remote memory buffers, binds them to symbol names, optionally hooks code, and persists its enable state across re-attaches.

Records are persisted to `%TEMP%/deeptrace_<pid>/scripts.dat` (symbol/hook/enable records) alongside `injects.dat`.

## deeptrace::script_alloc

### Syntax

```cpp
Result script_alloc(const std::string& name, size_t size, const std::string& owner,
                    uintptr_t* out_addr);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `const std::string&` | script symbol name (non-empty ASCII); unique per session |
| `size` | `size_t` | allocation size in bytes; must be > 0 |
| `owner` | `const std::string&` | owning script path (`""` = unattached); stored in the record for `script_status` grouping |
| `out_addr` | `uintptr_t*` | output parameter, allocated remote address |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | memory allocated and symbol registered |
| `Result::InvalidArg` | empty name / non-ASCII name / `size == 0` / null `out_addr` / duplicate symbol name |
| `Result::NotAttached` | no attached session |
| `Result::Error` | `VirtualAllocEx` failed, or record-save failed (allocation rolled back / freed) |
| `Result::AccessDenied` | no allocate-memory right |

### Description

Allocates `PAGE_EXECUTE_READWRITE` remote memory of `size` bytes and binds it to the script symbol `name`. The mapping is persisted per PID, so after a re-attach the same name resolves to the recorded address (via `script_symbol` or the CLI's symbol-aware commands). Duplicate symbol names are rejected with `InvalidArg`. On record-save failure the allocation is freed before returning (rollback), so no dangling memory is left.

Prerequisites: `attach(pid)` done. Postconditions: memory allocated; symbol record persisted.

### Example

```cpp
uintptr_t buf = 0;
deeptrace::script_alloc("newmem", 2048, "my_script.aa", &buf);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::script_alloc_near](#deeptracescript_alloc_near)
- [deeptrace::script_symbol](#deeptracescript_symbol)
- [deeptrace::script_free](#deeptracescript_free)

---

## deeptrace::script_alloc_near

### Syntax

```cpp
Result script_alloc_near(const std::string& name, size_t size, uintptr_t anchor,
                         const std::string& owner, uintptr_t* out_addr);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `const std::string&` | script symbol name (non-empty ASCII); unique per session |
| `size` | `size_t` | allocation size in bytes; must be > 0 |
| `anchor` | `uintptr_t` | anchor address; the allocation must land within ±2 GB of it |
| `owner` | `const std::string&` | owning script path (`""` = unattached) |
| `out_addr` | `uintptr_t*` | output parameter, allocated remote address |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | memory allocated within the window and symbol registered |
| `Result::InvalidArg` | empty name / non-ASCII name / `size == 0` / null `out_addr` / duplicate symbol name |
| `Result::NotAttached` | no attached session |
| `Result::Error` | no free region within ±2 GB of the anchor, `VirtualAllocEx` refused every candidate, or record-save failed (rollback) |
| `Result::AccessDenied` | no allocate-memory right |

### Description

Like `script_alloc`, but constrains the allocation to the **±2 GB window** around `anchor` — the range reachable by a 32-bit PC-relative displacement (`jmp`/`call` rel32, `mov [rip+disp32]`). This makes RIP-relative code generated for the script (e.g. a `jmp newmem` trampoline assembled at a hook point) always encodable, eliminating out-of-range displacement failures. The search prefers the free region **closest to the anchor** (upward pass first, then downward). On failure it returns `Error` and **never falls back to arbitrary placement** (which would silently reintroduce the out-of-range problem). Record/save/rollback semantics mirror `script_alloc`.

Prerequisites: `attach(pid)` done. Postconditions: memory allocated near the anchor; symbol record persisted.

### Example

```cpp
uintptr_t buf = 0;
deeptrace::script_alloc_near("newmem", 2048, hook_point, "my_script.aa", &buf);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::script_alloc](#deeptracescript_alloc)
- [deeptrace::asm_assemble_labels](ASM.md#deeptraceasm_assemble_labels)

---

## deeptrace::script_free

### Syntax

```cpp
Result script_free(const std::string& name);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `const std::string&` | the script symbol name to release |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | memory freed and symbol record removed |
| `Result::NotAttached` | no attached session |
| `Result::NotFound` | no symbol record matches `name` |

### Description

Releases the remote memory bound to `name` and removes the symbol record (the `dealloc` counterpart of `script_alloc`). Idempotent in the sense that freeing an unknown name reports `NotFound` without side effects.

Prerequisites: `attach(pid)` done. Postconditions: on `Ok`, memory freed and symbol gone.

### Example

```cpp
deeptrace::script_free("newmem");
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::script_alloc](#deeptracescript_alloc)

---

## deeptrace::script_enable

### Syntax

```cpp
Result script_enable(const std::string& path);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `const std::string&` | script file path used as identity |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | enable state recorded (or already enabled — idempotent) |
| `Result::NotAttached` | no attached session |

### Description

Persists the enabled state of a script path for the current PID. Setting an already-enabled path is a no-op success (idempotent). The record is the persistence layer of the CLI's `script enable`; it does not itself apply hooks or run code — the CLI applies the script's actions and records their outcomes in the same store.

Prerequisites: `attach(pid)` done. Postconditions: enable record persisted.

### Example

```cpp
deeptrace::script_enable("C:\\cheats\\money.aa");
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::script_disable](#deeptracescript_disable)
- [deeptrace::script_status](#deeptracescript_status)

---

## deeptrace::script_disable

### Syntax

```cpp
Result script_disable(const std::string& path);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `path` | `const std::string&` | script file path used as identity |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | disable state recorded (or already disabled — idempotent) |
| `Result::NotAttached` | no attached session |

### Description

Removes the persisted enable record of a script path for the current PID. Disabling an unknown/disabled path is a no-op success (idempotent). The CLI's `script disable` restores the script's `[DISABLE]` block (e.g. `hook_clear` / `script_free`) and then calls this to clear the state.

Prerequisites: `attach(pid)` done. Postconditions: enable record removed.

### Example

```cpp
deeptrace::script_disable("C:\\cheats\\money.aa");
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::script_enable](#deeptracescript_enable)
- [deeptrace::script_status](#deeptracescript_status)

---

## deeptrace::script_status

### Syntax

```cpp
Result script_status(std::vector<ScriptInfo>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `out` | `std::vector<ScriptInfo>&` | output parameter, per-script records (path/state/hooks/allocs) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | query succeeded; `out` filled (possibly empty) |
| `Result::NotAttached` | no attached session |

### Description

Lists the persisted script records for the current PID: each `ScriptInfo` carries the script path, its enable state (`"enabled"`/`"disabled"`), the hooks it registered (`HookInfo` list), and its allocations (symbol → address pairs). Used by the CLI's `script status` to show the current script landscape.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
std::vector<deeptrace::ScriptInfo> scripts;
deeptrace::script_status(scripts);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::script_enable](#deeptracescript_enable)
- [Types/STRUCTS.md](../Types/STRUCTS.md#scriptinfo-script-record)

---

## deeptrace::script_symbol

### Syntax

```cpp
Result script_symbol(const std::string& name, uintptr_t* out_addr);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `const std::string&` | script symbol name to look up (non-empty ASCII) |
| `out_addr` | `uintptr_t*` | output parameter, the recorded address of the symbol |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | symbol found; `out_addr` written |
| `Result::InvalidArg` | bad name or `out_addr == nullptr` |
| `Result::NotAttached` | no attached session |
| `Result::NotFound` | no symbol record matches `name` for the current PID |

### Description

Read-only lookup of a script symbol's recorded address for the current session. Does **not** modify the per-PID record. This is the API behind the CLI's symbol-aware address resolution (`mem read sunObjPtr` resolves the script symbol first) and lets external tools read values through the "artificial pointers" a script registered.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
uintptr_t addr = 0;
if (deeptrace::script_symbol("sunObjPtr", &addr) == deeptrace::Result::Ok) {
    uint64_t obj = 0;
    deeptrace::memory_read(addr, &obj, sizeof obj, nullptr);
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::script_alloc](#deeptracescript_alloc)
- [deeptrace::script_status](#deeptracescript_status)

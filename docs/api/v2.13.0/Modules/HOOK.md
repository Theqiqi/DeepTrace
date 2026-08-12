# Module: Hooks

Inline code hooks: patch a target instruction with a 5-byte `jmp` to an injected buffer, and restore the original bytes later. These are the code-patching primitives of the script engine's `[ENABLE]`/`[DISABLE]` blocks (a script's `hook` keyword maps to `hook_set` with the `alloc`'d `newmem` buffer).

Hook records are persisted per PID in `%TEMP%/deeptrace_<pid>/scripts.dat`.

## deeptrace::hook_set

### Syntax

```cpp
Result hook_set(uintptr_t addr, uintptr_t newmem, const std::string& owner,
                HookInfo& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | target address whose first 5 bytes will be patched; must not be 0 |
| `newmem` | `uintptr_t` | jump destination (the injected code buffer); must not be 0 |
| `owner` | `const std::string&` | owning script path (`""` = unattached) |
| `out` | `HookInfo&` | output parameter, hook record (target/newmem/original bytes) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | patch applied; `out.orig_bytes` holds the true original bytes |
| `Result::InvalidArg` | `addr == 0` or `newmem == 0` |
| `Result::NotAttached` | no attached session |
| `Result::ReadFault` | could not read the 5 bytes at `addr` |
| `Result::WriteFault` | patch write failed |
| `Result::AccessDenied` | no memory-write right |
| `Result::Error` | record-save failed (original bytes written back — rollback) |

### Description

Writes `E9 <rel32>` (`jmp newmem`, rel32 = `newmem − (addr+5)`) over the first 5 bytes at `addr`, saving the original bytes first. **Idempotent**: re-setting an already-hooked target does not re-read the (already patched) region — the saved original bytes and owner are kept, so `hook_clear` always restores the true originals. On record-save failure the original bytes are written back before returning, so the target never carries an unrecorded patch. The `out.orig_bytes` field always reflects the true original bytes (never the patch). Typical use: hook a game function to route execution into a script-injected `newmem` routine (which runs the custom logic and `jmp`s back).

Prerequisites: `attach(pid)` done; `addr` writable (typically code cave or function prologue). Postconditions: target redirects to `newmem`; record persisted.

### Example

```cpp
uintptr_t hook_point = 0x140007D00;   // resolved target
uintptr_t newmem = 0;                 // from script_alloc
deeptrace::HookInfo hook;
if (deeptrace::hook_set(hook_point, newmem, "my_script.aa", hook) ==
    deeptrace::Result::Ok) {
    // execution now jumps into newmem at hook_point
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::hook_clear](#deeptracehook_clear)
- [deeptrace::asm_assemble_labels](ASM.md#deeptraceasm_assemble_labels)
- [deeptrace::script_alloc](SCRIPT.md#deeptracescript_alloc)
- [Types/STRUCTS.md](../Types/STRUCTS.md#hookinfo-hook-record)

---

## deeptrace::hook_clear

### Syntax

```cpp
Result hook_clear(uintptr_t addr);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | the hooked target address to restore |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | original bytes written back and record removed |
| `Result::NotAttached` | no attached session |
| `Result::NotFound` | `addr` has no hook record |
| `Result::WriteFault` | write-back failed (record kept so it can be retried) |

### Description

Restores the original bytes of a hooked target (the bytes saved at `hook_set` time) and removes its record — the `[DISABLE]` counterpart of `hook_set`. If the write-back fails, the record is **kept** so a later retry can complete the restore. Clearing a non-hooked address reports `NotFound`.

Prerequisites: `attach(pid)` done. Postconditions: on `Ok`, target code restored and record gone.

### Example

```cpp
if (deeptrace::hook_clear(hook_point) == deeptrace::Result::Ok) {
    // original code restored
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::hook_set](#deeptracehook_set)

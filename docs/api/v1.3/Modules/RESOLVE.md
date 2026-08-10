# Module: Resolution

Address resolution and pattern scanning. `pattern_scan` requires an `attach` to the target process.

## deeptrace::resolve_base

### Syntax

```cpp
Result resolve_base(const std::string& name, uintptr_t* out_base);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `const std::string&` | module name or full path (ASCII) |
| `out_base` | `uintptr_t*` | output parameter, module base address |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | succeeded |
| `Result::InvalidArg` | `out_base == nullptr` |
| `Result::NotAttached` | no attached session |
| `Result::NotFound` | module not loaded |

### Description

An alias of `module_base` with identical semantics and return values (see [deeptrace::module_base](MODULE.md#deeptracemodule_base)). This interface is kept to semantically distinguish "resolving a module base" from "module management". Combined with base+offset or with `pattern_scan`, target symbol addresses can be precisely located.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
uintptr_t base = 0;
deeptrace::resolve_base("game.exe", &base);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::module_base](MODULE.md#deeptracemodule_base)
- [deeptrace::pattern_scan](#deeptracepattern_scan)

---

## deeptrace::pattern_scan

### Syntax

```cpp
Result pattern_scan(const std::string& pattern, std::vector<uintptr_t>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `pattern` | `const std::string&` | pattern: space-separated hex bytes; `??` means any byte |
| `out` | `std::vector<uintptr_t>&` | output parameter, all hit addresses (may be empty) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | scan completed (hit list written to `out`, possibly empty) |
| `Result::InvalidArg` | `pattern` is empty |
| `Result::BadFormat` | pattern contains invalid characters (non-hex, incomplete wildcard, etc.) |
| `Result::NotAttached` | no attached session |

### Description

Scans all committed, readable, non-`PAGE_GUARD` memory regions of the target process for the pattern and returns every hit address. Pattern format example: `"48 8B ?? ?? 00"` (`??` matches any single byte). The scan proceeds in 1 MiB chunks and handles cross-chunk hits; large processes may take seconds to tens of seconds. Typical uses: locating function addresses (signatures stable across version updates), finding global data.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
std::vector<uintptr_t> hits;
deeptrace::pattern_scan("48 8B 05 ?? ?? ?? ??", hits);
for (auto h : hits) { /* h is a hit address */ }
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::memory_regions](MEMORY.md#deeptracememory_regions)

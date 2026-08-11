# Module: Memory

Remote memory read/write capabilities. All functions require an `attach` to the target process, otherwise they return `NotAttached`.

## deeptrace::memory_read

### Syntax

```cpp
Result memory_read(uintptr_t addr, void* buf, size_t size, size_t* out_read);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | start address inside the target process |
| `buf` | `void*` | output buffer (caller-allocated, at least `size` bytes) |
| `size` | `size_t` | number of bytes requested; must be non-zero |
| `out_read` | `size_t*` | optional, actual number of bytes read; pass `nullptr` to ignore |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | read call succeeded (may be partial; trust `*out_read`) |
| `Result::InvalidArg` | `buf == nullptr` or `size == 0` |
| `Result::NotAttached` | no attached session |
| `Result::ReadFault` | target memory unreadable |

### Description

Reads up to `size` bytes from `addr` in the target process into the local `buf`. Unlike `memory_dump`, this function does not force a full read: the actual length read is written to `out_read` (the underlying `ReadProcessMemory` typically fails wholesale on unreadable pages; partial reads only occur in scenarios such as page-boundary crossings). Suitable for reading structs/variables of known size or regions of uncertain readability. Out-of-range addresses or unreadable pages return `ReadFault` instead of crashing.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
uint8_t buf[16];
size_t got = 0;
if (deeptrace::memory_read(0x140001000, buf, sizeof buf, &got) == deeptrace::Result::Ok) {
    // buf[0..got) contains the data read
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::memory_write](#deeptracememory_write)
- [deeptrace::memory_dump](#deeptracememory_dump)

---

## deeptrace::memory_write

### Syntax

```cpp
Result memory_write(uintptr_t addr, const void* buf, size_t size, size_t* out_written);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | start address inside the target process |
| `buf` | `const void*` | local data to write |
| `size` | `size_t` | number of bytes to write; must be non-zero |
| `out_written` | `size_t*` | optional, actual number of bytes written |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | write call succeeded (may be partial) |
| `Result::InvalidArg` | `buf == nullptr` or `size == 0` |
| `Result::NotAttached` | no attached session |
| `Result::WriteFault` | target memory not writable (read-only page/out of range) |

### Description

Writes `size` bytes of local data to `addr` in the target process. The target memory must be writable (e.g. `PAGE_READWRITE`); writing a read-only code section or code page returns `WriteFault`. Typical uses: modifying global variables, game values, bypassing checks, etc. Note that writing to the target's code section usually requires adjusting page protection first (see the `guard_clear`/`ProtectRegion` capabilities).

Prerequisites: `attach(pid)` done; the target address is writable. Postconditions: target memory modified.

### Example

```cpp
uint32_t v = 0xCAFEBABE;
size_t written = 0;
deeptrace::memory_write(0x140001000, &v, sizeof v, &written);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::memory_read](#deeptracememory_read)

---

## deeptrace::memory_dump

### Syntax

```cpp
Result memory_dump(uintptr_t addr, size_t size, std::vector<uint8_t>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | start address inside the target process |
| `size` | `size_t` | number of bytes to read, range 1 ~ 64 MiB |
| `out` | `std::vector<uint8_t>&` | output parameter, complete byte sequence |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | fully read `size` bytes |
| `Result::InvalidArg` | `size == 0` or `size > 64 MiB` |
| `Result::NotAttached` | no attached session |
| `Result::ReadFault` | incomplete read (region contains unreadable pages) |

### Description

Reads `size` bytes starting at `addr` in the target process into `out` in one call. Unlike `memory_read`, it requires a **full read**: any incompleteness returns `ReadFault` without outputting partial data. Suitable for dumping contiguous memory regions, module data, or hex viewing. The 64 MiB cap prevents oversized single allocations; larger regions should be read in chunks or planned with `memory_regions`.

Prerequisites: `attach(pid)` done; the target region is contiguously readable. Postconditions: none.

### Example

```cpp
std::vector<uint8_t> bytes;
if (deeptrace::memory_dump(0x140001000, 0x100, bytes) == deeptrace::Result::Ok) {
    // bytes.size() == 0x100
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::memory_read](#deeptracememory_read)
- [deeptrace::memory_regions](#deeptracememory_regions)

---

## deeptrace::memory_regions

### Syntax

```cpp
Result memory_regions(std::vector<MemoryRegion>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `out` | `std::vector<MemoryRegion>&` | output parameter, list of the target process's memory regions |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | enumeration succeeded |
| `Result::NotAttached` | no attached session |

### Description

Enumerates all committed/reserved memory regions of the target process (base, size, protection, state, type) for memory layout analysis, pattern-scan planning (`pattern_scan` uses this interface internally), or protection strategy design. `MemoryRegion.protection` is a combination of `PAGE_*` bits; `state` distinguishes `MEM_COMMIT`/`MEM_RESERVE`.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
std::vector<deeptrace::MemoryRegion> regions;
deeptrace::memory_regions(regions);
for (const auto& rg : regions) {
    if (rg.state == MEM_COMMIT) { /* handle committed regions */ }
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::pattern_scan](RESOLVE.md#deeptracepattern_scan)

---

## deeptrace::memory_readval

### Syntax

```cpp
Result memory_readval(uintptr_t addr, ValueType type, std::string& out_text);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | start address inside the target process |
| `type` | `ValueType` | value type (determines read length and formatting) |
| `out_text` | `std::string&` | output parameter, formatted value text |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | read and formatted successfully |
| `Result::NotAttached` | no attached session |
| `Result::ReadFault` | could not fully read the bytes required for this type |
| `Result::Error` | value formatting failed (theoretically unreachable) |

### Description

Reads a typed value at the target address per `ValueType` and formats it as text: integer types output `0x`-prefixed hex (e.g. `0x11223344`), float types output decimal. The read length is the type's fixed size (see [Types/ENUMS.md](../Types/ENUMS.md)); anything short returns `ReadFault`. This function is a shortcut for "view the current value of some variable in the target"; the CLI's `mem readval` command is based on it.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
std::string text;
if (deeptrace::memory_readval(addr, deeptrace::ValueType::Dword, text) == deeptrace::Result::Ok) {
    // text == "0x11223344"
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::memory_read](#deeptracememory_read)
- [ValueType](../Types/ENUMS.md)

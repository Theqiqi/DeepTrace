# Module: Disassembly

x64 disassembly of the target process's memory. Requires an `attach` to the target process.

## deeptrace::disasm_at

### Syntax

```cpp
Result disasm_at(uintptr_t addr, uint32_t count, std::vector<Instruction>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `addr` | `uintptr_t` | start address inside the target process |
| `count` | `uint32_t` | number of instructions to disassemble, range 1 ~ 10000 |
| `out` | `std::vector<Instruction>&` | output parameter, instruction list (address/machine code/text) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | disassembly succeeded (may be fewer than `count` instructions; trust `out.size()`) |
| `Result::InvalidArg` | `count == 0` or `count > 10000` |
| `Result::NotAttached` | no attached session |
| `Result::ReadFault` | start memory unreadable |

### Description

Disassembles up to `count` instructions starting at `addr`. Internally it pre-reads `count × 15` bytes (the longest x64 instruction) and decodes instruction by instruction; it stops early on undecodable bytes or memory boundaries, returning the decoded portion. Each `Instruction` contains the instruction address, machine-code bytes, and pure-ASCII disassembly text. Typical uses: viewing the instructions after `*out_rip` after a single step, HOOK-point location, code analysis.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
std::vector<deeptrace::Instruction> insns;
if (deeptrace::disasm_at(0x140001000, 10, insns) == deeptrace::Result::Ok) {
    for (const auto& i : insns) {
        std::cout << i.text << "\n";
    }
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::disasm_range](#deeptracedisasm_range)

---

## deeptrace::disasm_range

### Syntax

```cpp
Result disasm_range(uintptr_t start, uintptr_t end, std::vector<Instruction>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `start` | `uintptr_t` | disassembly start address |
| `end` | `uintptr_t` | disassembly end address (exclusive); `end >= start`, range cap 64 MiB |
| `out` | `std::vector<Instruction>&` | output parameter, instruction list |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | disassembly succeeded |
| `Result::InvalidArg` | `end < start` or range over 64 MiB |
| `Result::NotAttached` | no attached session |
| `Result::ReadFault` | range start memory unreadable |

### Description

Disassembles all decodable instructions in the `[start, end)` address range. Compared to `disasm_at`, it suits whole-section analysis with known boundaries (e.g. an entire function body). An oversized range (>64 MiB) returns `InvalidArg` to prevent runaway.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
std::vector<deeptrace::Instruction> insns;
deeptrace::disasm_range(func_addr, func_addr + 0x200, insns);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::disasm_at](#deeptracedisasm_at)

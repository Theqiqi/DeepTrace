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
- [deeptrace::disasm_buffer](#deeptracedisasm_buffer)

---

## deeptrace::disasm_buffer

### Syntax

```cpp
Result disasm_buffer(const uint8_t* data, size_t size, uintptr_t base_addr,
                     uint32_t count, std::vector<Instruction>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `data` | `const uint8_t*` | local byte buffer to disassemble (e.g. a `.bin` file read into memory); may be `nullptr` when `size == 0` |
| `size` | `size_t` | number of bytes in the buffer |
| `base_addr` | `uintptr_t` | virtual address reported for `data[0]`; pass `0` for files where addresses are irrelevant |
| `count` | `uint32_t` | maximum number of instructions to decode, range 1 ~ 10000 |
| `out` | `std::vector<Instruction>&` | output parameter, instruction list (address/machine code/text) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | decoding finished (may produce fewer than `count` instructions; trust `out.size()`) |
| `Result::InvalidArg` | `count == 0`, `count > 10000`, or (`data == nullptr` and `size > 0`) |

### Description

Disassembles a **local byte buffer** — the session-free counterpart of `disasm_at`. No `attach` is needed; the input comes from the caller's own memory (e.g. a shellcode `.bin` file, an injected payload, or captured bytes). Decoding stops at the first undecodable byte or after `count` instructions. `base_addr` lets the caller map instruction addresses onto a virtual layout (a hook buffer at a known address); pass `0` to start addresses from 0. This is the primitive behind the CLI `disasm file` command.

Prerequisites: none (usable offline). Postconditions: none.

### Example

```cpp
// disassemble a local bin file payload
std::vector<uint8_t> buf = {0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00, 0xC3};
std::vector<deeptrace::Instruction> insns;
if (deeptrace::disasm_buffer(buf.data(), buf.size(), 0x140001000, 16, insns)
    == deeptrace::Result::Ok) {
    for (const auto& i : insns) std::cout << i.text << "\n";
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::disasm_at](#deeptracedisasm_at)
- [deeptrace::disasm_range](#deeptracedisasm_range)

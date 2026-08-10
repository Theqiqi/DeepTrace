# Module: Assembly

x64 assembler (based on Keystone). **Does not require a session**; usable standalone.

## deeptrace::asm_assemble

### Syntax

```cpp
Result asm_assemble(const std::string& code, std::vector<uint8_t>& out,
                    std::string* out_text);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `code` | `const std::string&` | assembly instruction text; multiple statements separated by `;` or newlines |
| `out` | `std::vector<uint8_t>&` | output parameter, assembled machine-code bytes |
| `out_text` | `std::string*` | optional, hex text of the machine code; pass `nullptr` to ignore |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | all statements assembled successfully |
| `Result::BadFormat` | at least one instruction could not be assembled |

### Description

Assembles x64 assembly text into machine code. Multi-statement input is supported, with statements separated by `;` or newlines; empty statements are skipped automatically. If any statement fails to assemble, the whole call returns `BadFormat` (with `out` empty). Typical uses: generating Shellcode bytes for `shellcode_inject`, or constructing jump/patch bytes.

Prerequisites: none. Postconditions: none.

### Example

```cpp
std::vector<uint8_t> bytes;
if (deeptrace::asm_assemble("mov rax, 1; ret", bytes, nullptr) == deeptrace::Result::Ok) {
    // bytes == { 48 C7 C0 01 00 00 00, C3 }
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::shellcode_inject](INJECT.md#deeptraceshellcode_inject)

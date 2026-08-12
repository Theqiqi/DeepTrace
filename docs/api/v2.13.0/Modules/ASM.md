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
- [deeptrace::asm_assemble_labels](#deeptraceasm_assemble_labels)

---

## deeptrace::asm_assemble_labels

### Syntax

```cpp
Result asm_assemble_labels(const std::string& code, uintptr_t base_addr,
                           const std::map<std::string, uintptr_t>& symbols,
                           std::vector<uint8_t>& out, std::string* out_text);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `code` | `const std::string&` | multi-line assembly text with `label:` definitions and label/symbol references; statements separated by `;` or newlines |
| `base_addr` | `uintptr_t` | base address of the first assembled byte; used to compute PC-relative displacements (e.g. `jmp`/`call` rel32 to labels and `mov [sym], reg` absolute addressing) |
| `symbols` | `const std::map<std::string, uintptr_t>&` | external symbol table (name → absolute address); labels defined in `code` are merged on top of it; referenced names not found in either → `BadFormat` |
| `out` | `std::vector<uint8_t>&` | output parameter, assembled machine-code bytes |
| `out_text` | `std::string*` | optional, human-readable disassembly of the result; pass `nullptr` to ignore |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | all statements assembled successfully |
| `Result::BadFormat` | unknown instruction, undefined label/symbol reference, or syntax error (error text reports line number and symbol name) |

### Description

Assembles a **multi-line** assembly routine with local label definitions and forward/backward references, unlike `asm_assemble` which handles only linear instruction text. Labels are recognized as `name:` lines; every reference to a label or to a name from the `symbols` table is re-encoded so that branch displacements and absolute operands are resolved against `base_addr` and the symbol addresses. This is the primitive behind the CLI script engine's `[ENABLE]` blocks (alloc + hook + `jmp newmem` trampolines): the engine passes the script symbols (e.g. an `alloc`'d buffer address) as `symbols` and the hook point as `base_addr`. Assembly is local-only — no session is required.

Prerequisites: none. Postconditions: none.

### Example

```cpp
std::map<std::string, uintptr_t> syms;
syms["newmem"] = 0x140100000;   // an alloc'd buffer
std::vector<uint8_t> bytes;
std::string code =
    "newmem:;\n"
    "  mov rax, 1;\n"
    "  ret;\n";
if (deeptrace::asm_assemble_labels(code, 0x140001000, syms, bytes, nullptr)
    == deeptrace::Result::Ok) {
    // bytes: 48 C7 C0 01 00 00 00  C3  (label newmem folded in)
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::asm_assemble](#deeptraceasm_assemble)
- [deeptrace::hook_set](HOOK.md#deeptracehook_set)
- [deeptrace::script_alloc](SCRIPT.md#deeptracescript_alloc)

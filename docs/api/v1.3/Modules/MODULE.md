# Module: Module Queries

Queries information about the modules (EXE/DLL) loaded by the target process. All functions require an `attach` to the target process. Module name matching rules: exact match on module name or full path (case-insensitive); the extension may be omitted (e.g. `kernel32` matches `kernel32.dll`).

## deeptrace::module_list

### Syntax

```cpp
Result module_list(std::vector<ModuleInfo>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `out` | `std::vector<ModuleInfo>&` | output parameter, module list (base/size/name/path) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | enumeration succeeded |
| `Result::NotAttached` | no attached session |

### Description

Enumerates all modules loaded by the target process. Module info is commonly used to locate a game/app base address, verify whether injection succeeded (`dll_list` also uses module enumeration to decide whether a DLL is still loaded), and resolve dependencies.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
std::vector<deeptrace::ModuleInfo> mods;
deeptrace::module_list(mods);
for (const auto& m : mods) {
    std::wcout << m.name << L" @ " << m.base << L"\n";
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::module_base](#deeptracemodule_base)
- [deeptrace::module_exports](#deeptracemodule_exports)

---

## deeptrace::module_find

### Syntax

```cpp
Result module_find(const std::string& name, ModuleInfo& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `const std::string&` | module name or full path (ASCII, case-insensitive, extension optional) |
| `out` | `ModuleInfo&` | output parameter, matching module information |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | module found |
| `Result::InvalidArg` | `name` is empty |
| `Result::NotAttached` | no attached session |
| `Result::NotFound` | the target process has not loaded this module |

### Description

Finds a module by name and returns its full information (base/size/name/path). Matching rules are described at the top of this module page. Typical use: get the main module info of a game, or confirm whether a DLL is loaded (unrelated to injection records — this only checks real load state).

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
deeptrace::ModuleInfo m;
if (deeptrace::module_find("kernel32.dll", m) == deeptrace::Result::Ok) {
    // m.base is the kernel32 base address
}
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::module_list](#deeptracemodule_list)

---

## deeptrace::module_base

### Syntax

```cpp
Result module_base(const std::string& name, uintptr_t* out_base);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `const std::string&` | module name or full path (ASCII) |
| `out_base` | `uintptr_t*` | output parameter, module base address |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | succeeded; `*out_base` is the base address |
| `Result::InvalidArg` | `out_base == nullptr` |
| `Result::NotAttached` | no attached session |
| `Result::NotFound` | module not loaded |

### Description

A convenience wrapper over `module_find` returning only the base address. Base + offset is the common way to compute target addresses (global variables, function addresses); combined with `disasm_at` and `pattern_scan`, signature-based location can be completed.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
uintptr_t base = 0;
deeptrace::module_base("game.exe", &base);
uintptr_t g_health = base + 0x123456;
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::resolve_base](RESOLVE.md#deeptraceresolve_base)

---

## deeptrace::module_exports

### Syntax

```cpp
Result module_exports(const std::string& name, std::vector<ExportInfo>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `const std::string&` | module name or full path (ASCII) |
| `out` | `std::vector<ExportInfo>&` | output parameter, export symbol list (name/address) |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | parsing succeeded |
| `Result::NotAttached` | no attached session |
| `Result::NotFound` | module not loaded or not a PE module |

### Description

Parses the PE export table of a module in the target process and returns exported function names with their absolute addresses. Used to locate the real address of an API (bypassing the IAT or calling it directly in the target), compute HOOK targets, etc. Only works for export-bearing modules (DLLs, some EXEs); modules without an export table return an empty list with a result of `Ok`.

Prerequisites: `attach(pid)` done. Postconditions: none.

### Example

```cpp
std::vector<deeptrace::ExportInfo> exps;
if (deeptrace::module_exports("ntdll.dll", exps) == deeptrace::Result::Ok) {
    for (const auto& e : exps) {
        if (e.name == "NtQueryInformationProcess") { /* e.address */ }
    }
}
```

### Header

```cpp
#include "deeptrace.h"
```

---

## deeptrace::module_dump

### Syntax

```cpp
Result module_dump(const std::string& name, const std::string& output_file,
                   std::string* out_hex);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `name` | `const std::string&` | module name or full path (ASCII) |
| `output_file` | `const std::string&` | output file path (writes binary); empty string outputs hex text instead |
| `out_hex` | `std::string*` | optional, hex text of the module contents; pass `nullptr` to ignore |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | dump succeeded (written to file or `*out_hex`) |
| `Result::NotAttached` | no attached session |
| `Result::NotFound` | module not loaded |
| `Result::Error` | output file could not be opened |

### Description

Reads out the module image contents: when `output_file` is specified, writes it to disk as binary (handy for saving a modified module for offline analysis); otherwise writes the contents as hex text into `out_hex`. Reads are chunked by 1 MiB; it stops early on unreadable pages (still returning `Ok`, based on what was actually read). Modules can reach tens of MiB; watch memory usage when dumping large modules.

Prerequisites: `attach(pid)` done. Postconditions: `output_file` created or overwritten.

### Example

```cpp
// save to a file
deeptrace::module_dump("game.exe", "C:\\temp\\game.bin", nullptr);
// or get hex text
std::string hex;
deeptrace::module_dump("game.exe", "", &hex);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::memory_dump](MEMORY.md#deeptracememory_dump)

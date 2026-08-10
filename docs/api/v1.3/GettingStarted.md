# Quick Start

This page walks you through deeptrace's core flow with a complete example: enumerate processes → attach to the target → read memory → detach. The example can be copied, compiled, and run directly.

## 1. Project Setup

- Environment: Windows x64, MSVC (Visual Studio 2022), C++20
- Header: `deeptrace/include/deeptrace.h`
- Library: `deeptrace/out/lib/<Debug|Release>/deeptrace.lib`
- Link dependencies: deeptrace is a static library; assembly/disassembly rely on Keystone/Capstone, which are **not merged into the library**, so consumers must link them explicitly:
  - `deeptrace/out/build/<lowercase-config>/third_party/keystone/lib/keystone.lib`
  - `deeptrace/out/lib/<Debug|Release>/capstone.lib`

### Build Command (MSVC)

```bat
cl /nologo /std:c++20 /EHsc /MDd /I deeptrace\include getting_started.cpp ^
   deeptrace\out\lib\Debug\deeptrace.lib ^
   deeptrace\out\build\debug\third_party\keystone\lib\keystone.lib ^
   deeptrace\out\lib\Debug\capstone.lib /link /out:getting_started.exe
```

> Recommended: build the library first with the project's own script: `deeptrace\script\build_debug.bat`
> (artifacts are produced in `deeptrace/out/lib/Debug/deeptrace.lib`).

## 2. Usage Flow

```cpp
#include "deeptrace.h"
#include <iostream>

int main() {
    // 1. enumerate processes, let the user choose a target
    std::vector<deeptrace::ProcessInfo> procs;
    if (deeptrace::enumerate_processes(procs) != deeptrace::Result::Ok) {
        std::cerr << "failed to enumerate processes\n";
        return 1;
    }
    uint32_t pid = 0;
    for (const auto& p : procs) {
        std::wcout << p.pid << L"  " << p.name << L"\n";
        if (pid == 0) pid = p.pid;  // demo: pick the first process
    }
    if (pid == 0) return 1;

    // 2. attach to the target process (establish a session)
    if (deeptrace::attach(pid) != deeptrace::Result::Ok) {
        std::cerr << "attach failed (may need administrator privileges)\n";
        return 1;
    }

    // 3. read target memory
    uint32_t val = 0;
    size_t got = 0;
    if (deeptrace::memory_read(0x140000000, &val, sizeof val, &got) ==
        deeptrace::Result::Ok && got == sizeof val) {
        std::cout << "read value: 0x" << std::hex << val << "\n";
    }

    // 4. detach the session (required: prevents debuggee termination in debug mode)
    deeptrace::detach();
    return 0;
}
```

## 3. Flow Essentials

| Stage | API | Description |
|-------|-----|-------------|
| Enumerate | `enumerate_processes` | no session needed; lists candidate targets |
| Attach | `attach(pid)` | establishes the global session; prerequisite for later operations |
| Read | `memory_read` / `memory_readval` / `memory_dump` | requires an attached session |
| Detach | `detach()` | closes the session; automatically detaches safely when in debug mode |

## 4. Next Steps

- Read/write and scanning: [Examples/read_write_memory.md](Examples/read_write_memory.md)
- Debug breakpoints: [Examples/debug_breakpoints.md](Examples/debug_breakpoints.md)
- Full API list: [README.md](README.md)

# deeptrace Static Library API Reference (v2.13.0)

> This document is the API reference for the **deeptrace static library** (process memory operations / debugging utility library), written for its callers.
> Callers include: the CLI (deeptrace_cli), other developers, AI tools and integrators.
> Corresponding code version: deeptrace library **v2.13.0** (git tag v2.13.0). Public header: `deeptrace/include/deeptrace.h`.

## 1. Overview

deeptrace is a Windows x64 process-operation static library providing process enumeration/attach, remote memory read/write, module queries, thread operations, debugging (breakpoints/single-step/registers), disassembly, assembly, pattern scanning, watches, DLL/shellcode injection, an AA-style script engine (named allocations / hooks / enable-state), and pointer-chain reverse scanning. All capabilities are exposed through a **single public header** using only standard C++ types (no `windows.h` types), making it easy to integrate and wrap for other languages.

- Language standard: C++20
- Platform: Windows x64 (depends on WinAPI; Linux/macOS not supported)
- Header: `#include "deeptrace.h"` (located at `deeptrace/include/`)
- Linking: `deeptrace/out/lib/<Debug|Release>/deeptrace.lib` (Debug=/MDd, Release=/MT static runtime)
- Dependencies: assembly/disassembly rely on Keystone and Capstone; the static library does **not merge dependencies**, so consumers must additionally link `keystone.lib` and `capstone.lib` (link commands in [GettingStarted](GettingStarted.md)); debugging/injection rely on system debug APIs (administrator privileges required)

## 2. API Group Overview

**74 public functions** (73 `Result`-returning + `result_message`), **3 enums**, **16 structs** in total.

| Module | Doc | Functions | Count |
|--------|-----|-----------|-------|
| Process & session | [Modules/PROCESS.md](Modules/PROCESS.md) | `result_message`, `enumerate_processes`, `attach`, `detach`, `process_info`, `suspend_process`, `resume_process`, `terminate_process`, `session_pid`, `session_permissions` | 10 |
| Memory | [Modules/MEMORY.md](Modules/MEMORY.md) | `memory_read`, `memory_write`, `memory_dump`, `memory_regions`, `memory_readval` | 5 |
| Module | [Modules/MODULE.md](Modules/MODULE.md) | `module_list`, `module_find`, `module_base`, `module_exports`, `module_dump` | 5 |
| Thread | [Modules/THREAD.md](Modules/THREAD.md) | `thread_list`, `thread_suspend`, `thread_resume`, `thread_terminate`, `thread_create_at` | 5 |
| Debug | [Modules/DEBUG.md](Modules/DEBUG.md) | `debug_attach`, `debug_detach`, `debug_pause`, `debug_resume`, `debug_step`, `debug_step_over`, `debug_continue`, `breakpoint_set`, `breakpoint_clear`, `hw_breakpoint_set`, `hw_breakpoint_clear`, `guard_set`, `guard_clear`, `debug_status`, `registers_get`, `register_get` | 16 |
| Disassembly | [Modules/DISASM.md](Modules/DISASM.md) | `disasm_at`, `disasm_range`, `disasm_buffer` | 3 |
| Assembly | [Modules/ASM.md](Modules/ASM.md) | `asm_assemble`, `asm_assemble_labels` | 2 |
| Resolution | [Modules/RESOLVE.md](Modules/RESOLVE.md) | `resolve_base`, `pattern_scan` | 2 |
| Watch | [Modules/WATCH.md](Modules/WATCH.md) | `watch_list`, `watch_add`, `watch_remove`, `watch_refresh`, `watch_clear` | 5 |
| Injection | [Modules/INJECT.md](Modules/INJECT.md) | `dll_inject`, `dll_eject`, `dll_list`, `dll_status`, `shellcode_inject`, `shellcode_inject_at`, `shellcode_alloc`, `shellcode_run`, `shellcode_free`, `shellcode_status` | 10 |
| Script engine | [Modules/SCRIPT.md](Modules/SCRIPT.md) | `script_alloc`, `script_alloc_near`, `script_free`, `script_enable`, `script_disable`, `script_status`, `script_symbol` | 7 |
| Hooks | [Modules/HOOK.md](Modules/HOOK.md) | `hook_set`, `hook_clear` | 2 |
| Pointer-chain scan | [Modules/POINTERSCAN.md](Modules/POINTERSCAN.md) | `pointer_map_snapshot`, `pointer_map_rescan` | 2 |

Data type documentation:

| Type | Doc |
|------|-----|
| `Result` (14 error codes) | [Types/RESULT.md](Types/RESULT.md) |
| `ValueType`, `BreakpointType` | [Types/ENUMS.md](Types/ENUMS.md) |
| `ProcessInfo`, `MemoryRegion`, `ModuleInfo`, `ExportInfo`, `ThreadInfo`, `RegisterInfo`, `BreakpointInfo`, `WatchEntry`, `Instruction`, `DebugStatus`, `ContinueInfo`, `InjectInfo`, `HookInfo`, `ScriptInfo`, `PointerScanConfig`, `PointerChain` | [Types/STRUCTS.md](Types/STRUCTS.md) |

## 3. Prerequisites for Calling (Dependency Analysis)

### 3.1 Session Lifecycle (Core Prerequisite)

The library maintains a **single global session** (only one attach target per process):

```
enumerate_processes / process_info (query by pid)   ← no session needed
        │
        ▼
 attach(pid)     ──►   session operations (memory/module/thread/disasm/inject/
                       script/hook/pointer-scan/…)
        │
        ▼
 debug_attach()  ──►   debug operations (breakpoints/single-step/run-to-breakpoint/registers/guard)
        │
        ▼
 debug_detach()  /  detach()   ──►   close the session
```

- Every API that needs a target process requires `attach(pid)` first; otherwise it returns `Result::NotAttached`.
- Exceptions (operate by pid or offline, no session needed): `enumerate_processes`, `process_info`, `suspend_process`, `resume_process`, `terminate_process`, `asm_assemble`, `asm_assemble_labels`, `disasm_buffer`, `result_message`.
- The debug session nests on top of the process session: `debug_attach()` requires a prior `attach()`.
- `debug_continue` resumes the debuggee in debug mode and returns `Ok` on software-breakpoint hit / other exception / process exit (stop reason in `ContinueInfo`), or `Result::Timeout` when no event arrives within the wait budget (see [DEBUG.md](Modules/DEBUG.md#deeptracedebug_continue)).
- `debug_step` / `debug_step_over` support a one-shot **attach → single-step → detach** flow without a debug session (the CLI's non-interactive usage).
- `session_permissions` reports the actual PROCESS_* access mask granted by the last attach (see [PROCESS.md](Modules/PROCESS.md#deeptracesession_permissions)).
- The script engine and hooks ride the same session: `script_alloc`/`hook_set` are the `[ENABLE]`-block primitives; `script_free`/`hook_clear` the `[DISABLE]`-block ones.
- Pointer-chain scan (`pointer_map_snapshot`/`pointer_map_rescan`) requires an attach; the two-phase snapshot → (re-locate value) → rescan workflow filters false positives across game restarts.

### 3.2 Privilege Requirements

| Operation | Privilege |
|-----------|-----------|
| Attaching other processes (`attach`) | needs `PROCESS_ALL_ACCESS` or basic query/read-write rights; processes at the same privilege level can be attached, lower-privilege processes may give `AccessDenied` |
| Debugging (`debug_attach` and breakpoints/single-step/registers) | needs administrator privileges (SeDebugPrivilege), otherwise `AccessDenied` |
| Injection (`dll_inject` / `shellcode_*`) | the target must allow creating remote threads and writing memory; restricted targets return `AccessDenied` |
| Script alloc / hooks (`script_alloc*`, `hook_set`) | needs memory allocation/write rights (usually present with the attach fallback rights); check `session_permissions` if a downgraded attach is suspected |

### 3.3 State Persistence

Breakpoints, injection records, script records, and watch entries are persisted to a state directory keyed by the target PID and can be restored after the process restarts:

```
%TEMP%/deeptrace_<pid>/
├── breaks.dat    # software/hardware breakpoint records
├── injects.dat   # DLL/shellcode injection records
├── scripts.dat   # script symbols / hooks / enable records (v2.3.0+)
└── watch.dat     # watch entries
```

### 3.4 Thread Safety

- The library uses a **single global session** and is designed for single-threaded use (the CLI is a non-interactive single-threaded model).
- Concurrent calls from multiple threads mutate the global session state — undefined behavior; if concurrency is truly needed, callers must add their own locking.
- Note: `pointer_map_snapshot`/`pointer_map_rescan` use an internal thread pool for the memory sweep; this is transparent to the caller (the calls still return normally on the calling thread).

### 3.5 General Conventions

- Address types are 64-bit `uintptr_t`; `out_*` output parameters may be `nullptr` to ignore (except those with defaults).
- All functions return `Result`; success is `Result::Ok`; error code meanings in [Types/RESULT.md](Types/RESULT.md).
- String types: wide-char process/module names use `std::wstring`; ASCII content such as parsing/descriptions uses `std::string`.
- Disassembly/assembly target the x64 instruction set.
- Shellcode/script buffers are allocated `PAGE_EXECUTE_READWRITE`; offsets in `PointerChain` are signed (`int64_t`).

## 4. Quick Start

See [GettingStarted.md](GettingStarted.md) — a complete example from scratch (compilable and runnable directly).

## 5. Complete Examples

- [Examples/session_lifecycle.md](Examples/session_lifecycle.md) — session lifecycle (enumerate → attach → query → detach)
- [Examples/read_write_memory.md](Examples/read_write_memory.md) — remote memory read/write and pattern scanning
- [Examples/debug_breakpoints.md](Examples/debug_breakpoints.md) — debug session and software breakpoints
- [Examples/pointer_chain_scan.md](Examples/pointer_chain_scan.md) — pointer-chain reverse scan (snapshot → rescan)

## 6. Change History

See [CHANGELOG.md](CHANGELOG.md).

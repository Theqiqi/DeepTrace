# Developer Documentation — Analysis Phase (v2.1.0)

> This file is the output of stage 1 of `.flow/developer_docs_development_process.md`:
> 1.1 Code analysis (code structure analysis + architecture highlights)
> 1.2 Reader analysis
> 1.3 Documentation requirements analysis

---

## 1.1 Code Analysis

### 1.1.1 Project Composition

This repository contains two independent CMake projects:

| Project | Directory | Program type | Responsibility |
|---------|-----------|--------------|----------------|
| deeptrace | `deeptrace/` | Windows x64 static library | process memory operations: process/memory/module/thread/debug/disassembly/assembly/resolve/watch/inject |
| deeptrace_cli | `cli/` | Windows x64 command-line exe | wraps the deeptrace public APIs as command-line commands, pure ASCII output |

There is also an experimental directory `sandbox/` (independent CMake project, used to verify third-party libraries/environment, not part of the deliverable).

### 1.1.2 deeptrace Static Library Layering (Four Layers)

```
deeptrace/src/
├── domain/          data layer: public data structures + enums (types only, no logic)
│   └── types.h
├── algorithm/       algorithm layer: pure computation, no I/O, no WinAPI
│   ├── hex.{h,cpp}      hex encode/decode
│   ├── scan.{h,cpp}     AOB pattern matching (pure byte stream)
│   └── format.{h,cpp}   number/byte formatting
├── infrastructure/  atomic layer: minimal WinAPI wrappers + third-party engine adapters, subdirectories by capability
│   ├── process/     OpenProcess / snapshot / suspend / resume / terminate
│   ├── memory/      Read/WriteProcessMemory / VirtualQueryEx
│   ├── module/      module snapshot / PE export parsing
│   ├── thread/      thread snapshot / Suspend / Resume / Terminate
│   ├── debug/       debugger (attach/pause/single-step/registers/breakpoint writes)
│   ├── inject/      VirtualAllocEx / remote thread / LoadLibrary path
│   ├── disassembly/ disassembly (internal implementation: Capstone)
│   └── assembly/    assembly encoding (internal implementation: Keystone)
└── service/         interface layer: composes domain+algorithm+infrastructure, implements public APIs
    ├── session.{h,cpp}  session management (attached pid/handle, state directory path)
    ├── store.{h,cpp}    state file read/write (breakpoint/watch/inject record persistence)
    ├── process/memory/module/thread/debug/disasm/resolve/watch/inject/asm
    └── ...              one service file per public API
```

- **Namespaces**: public APIs live in the `deeptrace` namespace; internal implementations (algorithm/infrastructure/session/store) live in `deeptrace::internal`.
- **Public header**: `deeptrace/include/deeptrace.h` is the only header consumers may include (56 public APIs); public types are in `deeptrace/include/domain/types.h`. The public header must not expose windows.h types; it uses standard C++ types only.
- **Dependency direction** (no cross-layer calls):
  ```
  service → algorithm + infrastructure + domain
  infrastructure → domain + WinAPI (errors uniformly converted to Result)
  algorithm → domain (pure computation)
  domain → none
  ```
  The algorithm layer must not call infrastructure; service must not bypass infrastructure to call WinAPI directly.
- **Third-party engines**: Keystone (assembly) and Capstone (disassembly) ship as source under `deeptrace/third_party/`; CMake trims the X86 backend and builds them in-tree; the static library does not merge dependencies, so consumers (the CLI) must explicitly link `keystone.lib` / `capstone.lib`.

### 1.1.3 cli Three-Layer Architecture

```
cli/src/
├── main.cpp          entry: init → parse_args → execute → exit code; exception fallback
├── command/          command parsing layer: global options (-p/-h/-v) + command routing + parameter validation
│   ├── commands.{h,cpp}  command table (group/subcommand/parameter specs) + help text
│   ├── parser.{h,cpp}    getopt global options + command routing + parameter validation
│   └── request.h          CommandRequest struct
├── interface/        interface call layer: command request → deeptrace API calls → result structures
│   ├── executor.{h,cpp}  dispatches to each command executor function
│   ├── cmd.h              internal declarations
│   └── cmd_*.cpp         split by command group (process/memory/module/thread/debug/disasm/resolve/watch/inject/asm/shellcode)
└── printing/         command printing layer: result structures → ASCII text output
    └── printer.{h,cpp}   pure formatting (tables/hex/errors/help/version)
```

- **Namespaces**: `deeptrace_cli` (public); internal helpers in `deeptrace_cli::internal`.
- **Dependency direction** (one-way):
  ```
  main → command → interface → deeptrace public APIs
                    ↓
               printing (pure formatting)
  ```
  printing does not depend on command/interface and does not call deeptrace.
- **Boundaries** (cli production code): no platform headers such as windows.h/tlhelp32.h; no third-party libraries; no blocking input (getchar/scanf/cin); all output pure ASCII.

### 1.1.4 Cross-Project Dependencies

```
cli (independent CMake project)
  ├── include path → ../../deeptrace/include (public header, no install intermediate layer)
  └── find_library → ../../deeptrace/out/lib/<config>/deeptrace.lib
                     + keystone.lib (../../deeptrace/out/build/<lowercase-config>/third_party/keystone/lib)
                     + capstone.lib (../../deeptrace/out/lib/<config>)
```

### 1.1.5 Build System (Identical for Both Projects)

- CMake + CMakePresets.json (Ninja generator) + MSVC (cl.exe, via vcvars64)
- Debug: CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDebugDLL (/MDd), triplet x64-windows
- Release: MultiThreaded (/MT), triplet x64-windows-static (static runtime, single file, no DLLs)
- vcpkg manifest (`vcpkg.json`): gtest (test-only dependency)
- Scripts: `script/build_debug.bat` / `build_release.bat` (Windows) + `*_wsl.sh` (WSL → cmd.exe bridge)
- Artifacts: exe → `out/bin/<config>/`, lib → `out/lib/<config>/`
- Packaging: `cli/script/package.bat` (builds deeptrace+cli Release → zip archive to `cli/out/dist/`)

### 1.1.6 Test System

| Project | Level | Content |
|---------|-------|---------|
| deeptrace | unit | `deeptrace_unit_test.exe` (hex/scan/disasm/asm/format, gtest) |
| deeptrace | integration | `deeptrace_integration_test.exe` (real target process chaining multiple APIs) |
| deeptrace | target | `deeptrace_target.exe` (ASLR disabled, known values at known addresses, prints PID line) |
| deeptrace | dll | `testdll.dll` (companion DLL for inject tests) |
| cli | unit | `deeptrace_cli_unit_test.exe` (parser/printer/executor) |
| cli | integration | `deeptrace_cli_integration_test.exe` (parse→execute→deeptrace API full chain) |
| cli | target | `deeptrace_target.exe` (for e2e, same as deeptrace target) |
| cli | e2e | `test_cli_e2e.py` (Python launches real exes and asserts command-line behavior, independent of CMake) |

- The test target program does not link deeptrace and has ASLR disabled, guaranteeing deterministic addresses.
- Integration tests need a real target process; e2e needs Debug build artifacts + testdll.dll.

### 1.1.7 State Persistence Conventions

Breakpoint/watch/inject state survives across CLI invocations; state files live under `%TEMP%/deeptrace_<pid>/`:
- `breakpoints.dat` (original bytes of software/hardware breakpoints)
- `watch.dat` (watch entries)
- `inject.dat` (injected DLL/shellcode records)

Persistence is implemented by the deeptrace service layer (session.cpp provides the state directory path, store.cpp reads/writes).

---

## 1.2 Reader Analysis

| Reader type | What they want to know | Documentation focus |
|-------------|------------------------|---------------------|
| **New to the project** | project structure, how to build and run, core concepts | onboarding guide (README/BUILDING), concepts (ARCHITECTURE) |
| **Contributors** | where the extension points are, how to write new modules, test requirements | extension guide (EXTENDING), testing guide (TESTING) |
| **Maintainers** | design decisions, known limitations, performance considerations | architecture overview (ARCHITECTURE), technical decision records (DESIGN_DECISIONS) |

The audience is **developers**, not end users. End-user documentation (if any) is separate from this doc set.

---

## 1.3 Documentation Requirements Analysis

| Document | Audience | Content |
|----------|----------|---------|
| README.md | every developer joining the project | one-line intro of both projects, build, quick start (≤50 lines) |
| BUILDING.md | new to the project | environment requirements (MSVC+Ninja+CMake+vcpkg), Debug/Release build steps, WSL bridge, common build issues |
| ARCHITECTURE.md | new to the project, maintainers | deeptrace four layers + cli three layers, module diagram, data flow, cross-project dependencies, state persistence |
| TESTING.md | contributors | how to run unit/integration/E2E tests, writing new tests, target program explanation |
| EXTENDING.md | contributors | extension points (add command/add API/replace engine/add algorithm), complete examples |
| DESIGN_DECISIONS.md | maintainers | technical decision records (layering, engine selection, runtime, state persistence, etc.) |
| CHANGELOG.md | maintainers | documentation version change history |

All function-level explanations **link to the API documentation** (`docs/api/v2.1.0/`) instead of being duplicated here.

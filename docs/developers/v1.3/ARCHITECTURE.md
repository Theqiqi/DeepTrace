# Architecture Overview (ARCHITECTURE)

> Audience: developers new to the project, maintainers.
> Design basis: design/v1.0, v1.1, v1.2 (the code is the source of truth).
> For function-level API details see the [API Documentation](../../api/v1.3/README.md).

## 1. Overview

```
┌─────────────────────────────────────────────────────────────┐
│  cli (independent CMake project)                             │
│                                                             │
│  main.cpp                                                    │
│    │                                                         │
│    ├─ command/     command parsing layer (argv → CommandRequest) │
│    ├─ interface/   interface call layer (CommandRequest → deeptrace API) │
│    └─ printing/    command printing layer (results → ASCII text) │
│         │                                                     │
│         │  include ../../deeptrace/include + find_library     │
│         ▼                                                     │
│  deeptrace (independent CMake project, static library)        │
│                                                             │
│  service/        interface layer: composition, public APIs + session/persistence │
│    │                                                         │
│    ├─ algorithm/  algorithm layer: pure computation (hex/AOB/format) │
│    ├─ infrastructure/  atomic layer: WinAPI wrappers + engine adapters │
│    │   (process/memory/module/thread/debug/inject/           │
│    │    disassembly/Capstone + assembly/Keystone)            │
│    └─ domain/     data layer: public types (types only, no logic) │
└─────────────────────────────────────────────────────────────┘
```

## 2. The deeptrace Static Library: Four Layers

### 2.1 Data Layer domain/ `deeptrace` namespace

- **Responsibility**: define all public data structures and enums (`Result`/`ValueType`/`BreakpointType` + `ProcessInfo`/`MemoryRegion`/`ModuleInfo`/`ThreadInfo`/`RegisterInfo`/`BreakpointInfo`/`WatchEntry`/`Instruction`/`DebugStatus`/`InjectInfo` etc.).
- **Forbidden**: any logic, any I/O.
- The public header `deeptrace/include/domain/types.h` and `src/domain/types.h` have identical content and must be kept in sync.

### 2.2 Algorithm Layer algorithm/ `deeptrace::internal` namespace

| File | Capability |
|------|------------|
| hex.{h,cpp} | hex encode/decode |
| scan.{h,cpp} | AOB pattern matching (pure byte stream) |
| format.{h,cpp} | number/byte formatting |

- **Responsibility**: pure computation; input and output are in-memory data (byte streams/strings).
- **Forbidden**: WinAPI, I/O, process read/write, impure functions; depends only on domain.
- Depends only on data layer types.

### 2.3 Atomic Layer infrastructure/ `deeptrace::internal` namespace

Subdirectories by capability:

```
infrastructure/
├── process/     OpenProcess / process snapshot / suspend / resume / terminate
├── memory/      Read/WriteProcessMemory / VirtualQueryEx
├── module/      module snapshot / PE export parsing
├── thread/      thread snapshot / Suspend / Resume / Terminate
├── debug/       debugger (attach/pause/single-step/registers/breakpoint writes)
├── inject/      VirtualAllocEx / remote thread / LoadLibrary path
├── disassembly/ disassembly (internal implementation: Capstone 5.0.9)
└── assembly/    assembly encoding (internal implementation: Keystone 0.9.2)
```

- **Responsibility**: minimal WinAPI wrappers (one syscall per wrapper) + third-party engine adaptation; errors are uniformly converted to `deeptrace::Result`.
- **Forbidden**: composing business flows, persistence, state across multiple calls.
- Engine adapter files (disasm/asmenc) expose only pure-function interfaces; swapping engines does not affect upper layers.

### 2.4 Interface Layer service/ `deeptrace` (public APIs) and `deeptrace::internal` (session/store)

| File | Responsibility |
|------|----------------|
| session.{h,cpp} | session management: attached pid/handle, `state_dir()` state directory path |
| store.{h,cpp} | state file read/write (breakpoints/watch/inject records) |
| process/memory/module/thread/debug/disasm/resolve/watch/inject/asm | implementations of each public API |

- **Responsibility**: compose domain + algorithm + infrastructure to implement the 55 public APIs; session management; breakpoint/watch/inject state persistence; `result_message` error semantics.
- **Forbidden**: calling WinAPI directly (must go through infrastructure), inlining algorithms.
- service public functions live in the `deeptrace` namespace (internal helpers session/store live in `deeptrace::internal`).

### 2.5 Dependency Direction (No Cross-Layer Calls)

```
service → algorithm + infrastructure + domain
infrastructure → domain + WinAPI
algorithm → domain
domain → none
```

- The algorithm layer must not call infrastructure.
- service must not bypass infrastructure to call WinAPI directly.

## 3. The cli Three Layers

### 3.1 main.cpp

- **Responsibility**: init → `parse_args` → `execute` → exit code; `std::exception` fallback returns 1.
- **Forbidden**: business logic, direct deeptrace calls.

### 3.2 Command Parsing Layer command/

| File | Responsibility |
|------|----------------|
| commands.{h,cpp} | command table (group/subcommand/parameter specs) + help text |
| parser.{h,cpp} | getopt global options (-p/-h/-v) + command routing + parameter validation |
| request.h | `CommandRequest` struct |

- **Responsibility**: parsing and validation, constructing a CommandRequest.
- **Forbidden**: executing business logic, calling deeptrace, printing business results (parameter errors are allowed).

### 3.3 Interface Call Layer interface/

| File | Responsibility |
|------|----------------|
| executor.{h,cpp} | dispatches commands to their executor functions |
| cmd.h | internal declarations (`deeptrace_cli::internal`) |
| cmd_*.cpp | split by command group (process/memory/module/thread/debug/disasm/resolve/watch/inject/asm/shellcode) |

- **Responsibility**: call deeptrace public APIs based on the CommandRequest and hand result structures to the printing layer.
- **Forbidden**: direct WinAPI, reimplementing capabilities deeptrace already provides, formatting output.

### 3.4 Command Printing Layer printing/

- **Responsibility**: pure formatting (process table/region table/module table/register table/hex dump/errors/help/version), `printer` namespace.
- **Forbidden**: calling deeptrace, depending on command/interface, business logic.
- Output constraints: pure ASCII; non-printable wide chars are replaced with `?`; addresses `0x%016llX`; bytes `%02X` uppercase.

### 3.5 Dependency Direction

```
main → command → interface → deeptrace public APIs
                    ↓
               printing (independent, depends only on deeptrace public types)
```

## 4. Data Flow (Command Lifecycle)

```
argv → parser parses (global options + command routing + parameter validation)
     → CommandRequest
     → executor dispatches → cmd_xxx() calls deeptrace public APIs
     → Result + result structures
     → printer formats → stdout / stderr
     → exit code (0 success / 1 execution failure / 2 usage error)
```

Session convention: after main parses the pid it calls `deeptrace::attach(pid)` first, then `deeptrace::detach()` after the command finishes; breakpoint/watch/inject state is persisted via state files and survives across CLI invocations.

## 5. Cross-Project Dependencies

```
cli (independent CMake project)
  ├── include path → ../../deeptrace/include (public header, no install intermediate layer)
  ├── find_library(DEEPTRACE_LIB)  → ../../deeptrace/out/lib/<config>/deeptrace.lib
  ├── find_library(KEYSTONE_LIB)   → ../../deeptrace/out/build/<lowercase-config>/third_party/keystone/lib
  └── find_library(CAPSTONE_LIB)   → ../../deeptrace/out/lib/<config>/capstone.lib
```

- deeptrace is a static library; third-party deps (keystone/capstone) are not propagated to the link line automatically — **the CLI must link them explicitly** (handled in cli/src/CMakeLists.txt).
- The public header exposes only standard C++ types; cli production code must not include platform headers like windows.h.

## 6. State Persistence

```
%TEMP%/deeptrace_<pid>/
├── breakpoints.dat   # original bytes of software/hardware breakpoints (DR0-DR3 slots)
├── watch.dat         # watch entries
└── inject.dat        # injected DLL/shellcode records (kind=dll|shellcode)
```

- Implemented by the service layer (session.cpp provides the path, store.cpp reads/writes, ASCII `|`-separated line format).
- Purpose: breakpoint/watch/inject state survives across CLI processes (session = single CLI process, but state files persist across processes).
- Cleanup: state is overwritten as needed when re-operating on the same pid; test cases must clean up themselves to avoid state pollution.

## 7. Core Concepts

| Concept | Description |
|---------|-------------|
| Session | holds the target process handle after `attach(pid)`; released by `detach()`; `debug_attach()` enters debug mode, `debug_detach()` exits debug but stays attached |
| Breakpoint | software breakpoint (writes 0xCC, saves original bytes) / hardware breakpoint (DR0-DR3) / page guard |
| Watch | description + address + type; `watch_refresh`/`watch_list` read target memory and show live values |
| Injection | DLL (LoadLibrary path + remote thread) / shellcode (VirtualAllocEx + remote thread); `dll_list`/`shellcode_status` query runtime status |
| Disassembly/Assembly | disasm_at/range call Capstone; asm_assemble calls Keystone (X86 backend) |

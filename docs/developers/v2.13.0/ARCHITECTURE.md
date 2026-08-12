# Architecture Overview (ARCHITECTURE)

> Audience: developers new to the project, maintainers.
> Design basis: design/v1.0.0 … v2.13.0 (the code is the source of truth).
> For function-level API details see the [API Documentation](../../api/v2.13.0/README.md).

## 1. Overview

```
┌─────────────────────────────────────────────────────────────┐
│  cli (independent CMake project)                             │
│                                                             │
│  main.cpp                                                    │
│    │                                                         │
│    ├─ command/     command parsing layer (argv → CommandRequest) │
│    ├─ interface/   interface call layer (CommandRequest → deeptrace API) │
│    │   (cmd_*.cpp by group + script/batch/json/ptrscan helpers) │
│    └─ printing/    command printing layer (results → ASCII text) │
│         │                                                     │
│         │  include ../../deeptrace/include + find_library     │
│         ▼                                                     │
│  deeptrace (independent CMake project, static library)        │
│                                                             │
│  service/        interface layer: composition, public APIs + session/persistence │
│    │                                                         │
│    ├─ algorithm/  algorithm layer: pure computation (hex/AOB/format/pointer_scan) │
│    ├─ infrastructure/  atomic layer: WinAPI wrappers + engine adapters + threadpool │
│    │   (process/memory/module/thread/debug/inject/           │
│    │    disassembly/Capstone + assembly/Keystone + threadpool)│
│    └─ domain/     data layer: public types (types only, no logic) │
└─────────────────────────────────────────────────────────────┘
```

## 2. The deeptrace Static Library: Four Layers

### 2.1 Data Layer domain/ `deeptrace` namespace

- **Responsibility**: define all public data structures and enums (`Result`/`ValueType`/`BreakpointType` + `ProcessInfo`/`MemoryRegion`/`ModuleInfo`/`ThreadInfo`/`RegisterInfo`/`BreakpointInfo`/`WatchEntry`/`Instruction`/`DebugStatus`/`ContinueInfo`/`InjectInfo`/`HookInfo`/`ScriptInfo`/`PointerScanConfig`/`PointerChain` etc.).
- **Forbidden**: any logic, any I/O.
- The public header `deeptrace/include/domain/types.h` and `src/domain/types.h` have identical content and must be kept in sync.

### 2.2 Algorithm Layer algorithm/ `deeptrace::internal` namespace

| File | Capability |
|------|------------|
| hex.{h,cpp} | hex encode/decode |
| scan.{h,cpp} | AOB pattern matching (pure byte stream) |
| format.{h,cpp} | number/byte formatting |
| pointer_scan.{h,cpp} | reverse pointer scan primitives (v2.12.0): `scan_pointers_to`/`scan_pointers_to_any` (non-aligned qword sweep, ±max_offset match, signed int64 offsets), `eval_chain` |

- **Responsibility**: pure computation; input and output are in-memory data (byte streams/strings).
- **Forbidden**: WinAPI, I/O, process read/write, impure functions; depends only on domain.
- Depends only on data layer types.

### 2.3 Atomic Layer infrastructure/ `deeptrace::internal` namespace

Subdirectories by capability:

```
infrastructure/
├── process/     OpenProcess / process snapshot / suspend / resume / terminate
├── memory/      Read/WriteProcessMemory / VirtualQueryEx / RemoteAlloc(Near)
├── module/      module snapshot / PE export parsing
├── thread/      thread snapshot / Suspend / Resume / Terminate
├── debug/       debugger (attach/pause/single-step/registers/breakpoint writes)
├── inject/      VirtualAllocEx / remote thread / LoadLibrary path
├── disassembly/ disassembly (internal implementation: Capstone 5.0.9)
├── assembly/    assembly encoding (internal implementation: Keystone 0.9.2)
└── threadpool/  self-built thread pool (v2.12.0, pure std::thread; enqueue→wait→reuse)
```

- **Responsibility**: minimal WinAPI wrappers (one syscall per wrapper) + third-party engine adaptation; errors are uniformly converted to `deeptrace::Result`.
- **Forbidden**: composing business flows, persistence, state across multiple calls.
- Engine adapter files (disasm/asmenc) expose only pure-function interfaces; swapping engines does not affect upper layers.
- `RemoteAllocNear` (v2.7.0) allocates within ±2 GB of an anchor (RIP-relative rel32 range), scanning free regions closest to the anchor first — the primitive behind `script_alloc_near`.
- The threadpool is a plain `std::thread` pool with `enqueue`/`wait`/`pending` semantics; the pointer scan reuses it across levels.

### 2.4 Interface Layer service/ `deeptrace` (public APIs) and `deeptrace::internal` (session/store)

| File | Responsibility |
|------|----------------|
| session.{h,cpp} | session management: attached pid/handle, granted access mask, `state_dir()` state directory path |
| store.{h,cpp} | state file read/write (breakpoints/watch/inject/script records) |
| process/memory/module/thread/debug/disasm/resolve/watch/inject/asm | implementations of each public API |
| script.{h,cpp} | script engine (v2.3.0+): `script_alloc`/`script_alloc_near`/`script_free`/`script_enable`/`script_disable`/`script_status`/`script_symbol` — named remote allocations persisted per-PID, idempotent enable/disable |
| hook.{h,cpp} | code hooks (v2.3.0): `hook_set` (5-byte E9 jmp, saves original bytes, idempotent, rollback on persist failure) / `hook_clear` (restore) |
| asm.{h,cpp} | assembly incl. `asm_assemble_labels` (v2.3.0): multi-line assembly with labels + external symbols; v2.5.0 extended to arbitrary symbol references (`mov [sym],reg` via RIP-relative rewrite, immediate verification against disassembly) |
| disasm.{h,cpp} | disassembly incl. `disasm_buffer` (v2.13.0): session-free local-buffer decoding |
| resolve.{h,cpp} | resolution incl. `pointer_map_snapshot`/`pointer_map_rescan` (v2.12.0): reverse-walk pointer-chain scan with module anchoring, max_results capping, threadpool acceleration |

- **Responsibility**: compose domain + algorithm + infrastructure to implement the 74 public APIs; session management; breakpoint/watch/inject/script state persistence; `result_message` error semantics.
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
| parser.{h,cpp} | getopt global options (-p/-h/-v) + command routing + parameter validation (incl. symbol-shaped addresses since v2.6.0) |
| request.h | `CommandRequest` struct |

- **Responsibility**: parsing and validation, constructing a CommandRequest.
- **Forbidden**: executing business logic, calling deeptrace, printing business results (parameter errors are allowed).

### 3.3 Interface Call Layer interface/

| File | Responsibility |
|------|----------------|
| executor.{h,cpp} | dispatches commands to their executor functions; `resolve_addr` (v2.6.0: numeric-address-then-symbol resolution via `script_symbol`) |
| cmd.h | internal declarations (`deeptrace_cli::internal`) |
| cmd_*.cpp | split by command group (process/memory/module/thread/debug/disasm/resolve/watch/inject/asm/shellcode/script) |
| script.{h,cpp} | debug-script layer: JSON-subset parser + step validation (op table covering every debug capability) |
| cmd_debug_run.cpp | debug-script session executor: one invocation = one session (attach → debug_attach → steps → cleanup → detach) |
| cmd_script.cpp | AA-style script engine executor (v2.3.0): parse `[ENABLE]`/`[DISABLE]` blocks, alloc/label/registersymbol/db/hook/createThread keywords, execute enable/disable idempotently; `script check` (v2.4.0) validates syntax + hook structure + assembly without side effects |
| batch.{h,cpp} | batch locator engine (v2.9.0): JSON locator list (pointer chains / module+offset / symbol+offset / absolute) → final addresses → read/write |
| json.{h,cpp} | shared minimal JSON parser (v2.12.0, extracted from batch) |
| ptrscan.{h,cpp} | `resolve ptrscan` config parsing/validation (v2.12.0) |

- **Responsibility**: call deeptrace public APIs based on the CommandRequest and hand result structures to the printing layer.
- **Forbidden**: direct WinAPI, reimplementing capabilities deeptrace already provides, formatting output.

### 3.4 Command Printing Layer printing/

- **Responsibility**: pure formatting (process table/region table/module table/register table/hex dump/errors/help/version/pointer chains/batch tables incl. CSV/JSON export since v2.10.0), `printer` namespace.
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

Session convention: after main parses the pid it calls `deeptrace::attach(pid)` first, then `deeptrace::detach()` after the command finishes; breakpoint/watch/inject/script state is persisted via state files and survives across CLI invocations. Commands that need **no session** (conversion layer: `asm file`, `hex2bin`, `bin2hex`, `disasm file`, `script check`) are excluded from the `-p` auto-attach.

**Debug exception**: the debug group has a single entry `debug run <script.json>` since v2.1.0. It does **not** follow the single-command convention — one invocation runs a complete scripted debug session (attach → `debug_attach` → steps → cleanup → `debug_detach` → detach) with all session state kept in memory only; the debuggee always survives (breakpoints/guards are cleaned up before detaching). Standalone debug commands (step/break/registers/…) were removed because, without a session, their semantics are wrong (fake single-step, residual 0xCC, misleading register/status reads).

**Script engine flow** (`script run`, v2.3.0): parse the `.aa` file into `[ENABLE]`/`[DISABLE]` blocks → `exec_enable` resolves module+offset anchors, allocates named memory (`script_alloc`/`script_alloc_near`), assembles the enable code with labels/symbols (`asm_assemble_labels`), applies hooks (`hook_set`) and optionally creates threads; `exec_disable` reverses (hook_clear/script_free). Enable/disable are **idempotent** — repeated enable skips, repeated disable skips, so re-running never double-applies. Script symbol records are persisted per-PID and resolvable from any command's address argument (`resolve_addr`).

**Pointer-chain scan flow** (`resolve ptrscan`, v2.12.0): snapshot (`pointer_map_snapshot`) reverse-walks from a target value address, finds qword slots whose pointer lands within ±max_offset, recurses up to max_level, anchors chains to a module, caps output at max_results; rescan (`pointer_map_rescan`) re-evaluates saved chains against a new target to intersect away coincidence-based false positives (the classic game-restart workflow). Output chains are formatted `module+off +38 +104` and are directly consumable by `mem batch` (search → verify loop).

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
├── inject.dat        # injected DLL/shellcode records (kind=dll|shellcode)
└── scripts.dat       # script symbol records / hook records / enable-state records (v2.3.0+)
```

- Implemented by the service layer (session.cpp provides the path, store.cpp reads/writes, ASCII `|`-separated line format).
- Purpose: breakpoint/watch/inject/script state survives across CLI processes (session = single CLI process, but state files persist across processes).
- Cleanup: state is overwritten as needed when re-operating on the same pid; test cases must clean up themselves to avoid state pollution.

## 7. Core Concepts

| Concept | Description |
|---------|-------------|
| Session | holds the target process handle after `attach(pid)`; released by `detach()`; `debug_attach()` enters debug mode, `debug_detach()` exits debug but stays attached; the granted PROCESS_* access mask is recorded at attach (v2.11.0) and queryable via `session_permissions` |
| Scripted debug session | `debug run <script.json>` — one invocation = one debug session; a JSON array of steps executed in order; first failure stops the session; all session state exists only in memory for that call |
| Script engine | `.aa` files with `[ENABLE]`/`[DISABLE]` blocks: alloc/label/registersymbol/createThread/db + hook keywords; named allocations and hooks persisted per-PID; symbols resolvable from any address argument |
| Pointer-chain scan | snapshot + rescan two-phase reverse-walk; module anchoring; threadpool accelerated; output consumable by `mem batch` |
| Breakpoint | software breakpoint (writes 0xCC, saves original bytes) / hardware breakpoint (DR0-DR3) / page guard |
| Watch | description + address + type; `watch_refresh`/`watch_list` read target memory and show live values |
| Injection | DLL (LoadLibrary path + remote thread) / shellcode (VirtualAllocEx + remote thread); `dll_list`/`shellcode_status` query runtime status; shellcode also supports alloc → run (repeatable) → free lifecycle (v2.2.0) |
| Disassembly/Assembly | disasm_at/range call Capstone; `disasm_buffer` decodes local buffers (v2.13.0); asm_assemble/asm_assemble_labels call Keystone (X86 backend) |

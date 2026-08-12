# Technical Decision Records (DESIGN_DECISIONS)

> Audience: maintainers. Each decision records "background → option comparison → final choice → rationale", not just the conclusion.
> Basis: tech choices and implementation pitfalls from design/v1.0.0~v1.2.0 (design/v1.2.0/deeptrace/00_CHANGELOG.md).

## ADR-01 Why a "static library + separate CLI" two-project setup

- **Background**: capability (process memory operations) and interaction (command line) are products with different lifecycles; the library needs to be called multiple times and reused, and the CLI is just one consumer of the library.
- **Option comparison**:
  - Single project: library and CLI mixed into one CMake target → library cannot be reused or tested independently.
  - Two independent CMake projects: cli references deeptrace's output via `find_library` + include path → library can be delivered independently (design convention: `deeptrace.lib` + `deeptrace.h`, no install intermediate layer).
- **Choice**: two projects. The CLI is the library's **first and currently only** consumer; the acceptance criterion for the library API design is "the CLI can call it cleanly".

## ADR-02 Why deeptrace uses four layers (domain/algorithm/infrastructure/service)

- **Background**: process memory operations involve two essentially different kinds of logic — pure computation (hex/AOB/decoding) and system calls (WinAPI) — which are hard to test and replace when mixed together.
- **Option comparison**:
  - Two layers (interface + implementation): WinAPI and algorithms inlined → algorithms not unit-testable, engines not replaceable (the v1.0.0 hand-written decoder that couldn't be replaced with Capstone was the lesson).
  - Four layers: the algorithm layer is pure computation with no I/O (independently unit-testable); infrastructure only wraps "one system call" per file; service composes and persists.
- **Choice**: four layers. Constraints: the algorithm layer forbids WinAPI/I/O; service forbids direct WinAPI; dependencies point one-way downward. After layering, the v1.2.0 engine replacements (hand-written decoder → Capstone, hand-written encoder → Keystone) were achieved with zero changes to service/public APIs/CLI — this is the payoff of four-layer layering (engine adaptation is confined to infrastructure internals).

## ADR-03 Why cli uses three layers (command/interface/printing)

- **Background**: the CLI needs to map 55 APIs to commands; parsing, calling, and formatting are three independently testable responsibilities.
- **Option comparison**:
  - Single main file: untestable, unextendable.
  - Three layers: command only parses and validates, interface only calls APIs, printing only formats (pure ASCII, independent of the first two layers).
- **Choice**: three layers. Unit tests can cover parser/printer/executor separately; adding a command only requires a cmd_*.cpp + commands table entry.

## ADR-04 Why Capstone for disassembly and Keystone for assembly (source-built)

- **Background**:
  - The hand-written x64 decoder (~26KB subset) had incomplete coverage (SSE/SSE2/REP string instructions, etc.) and "silently stops when it can't decode";
  - The hand-written encoder reported BadFormat for instructions like `add rax,0` (assembly failure bug);
  - The vcpkg capstone port disables all architectures by default in this environment (cs_open returns CS_ERR_ARCH).
- **Option comparison**:
  - vcpkg install: building the keystone port with all architectures takes tens of minutes; the capstone port is unusable in this environment.
  - Source-built into `third_party/`: keystone trimmed with `LLVM_TARGETS_TO_BUILD=X86`; capstone enabling only the X86 backend (`CAPSTONE_ARCHITECTURE_DEFAULT=OFF` + `CAPSTONE_X86_SUPPORT=ON`), tests/cstool/install disabled.
- **Choice**: source-built (per the "medium/large and environment-adaptive libraries are manually downloaded into third_party first" principle). Interface unchanged; zero changes to service/public APIs/CLI.
- **Pitfalls**: keystone bypasses the root CMakeLists and integrates the llvm subdirectory directly (to avoid kstool/fuzz targets conflicting with the /MD replacement); LLVM needs python, using the embedded `third_party/python` on Windows.

## ADR-05 Why disassembly uses `cs_disasm` and not `cs_disasm_iter`

- **Background**: under Capstone 5.0.9 + MSVC, `cs_disasm_iter` + an uninitialized `cs_insn` on the stack immediately faults (0xc0000005) on all decode paths; the sandbox independent verification program with the same source works fine with `cs_disasm`.
- **Option comparison**: `cs_disasm_iter` (caller supplies the cs_insn buffer; crashes in this environment) vs `cs_disasm(count=1)` (internally allocates the insn array; stable).
- **Choice**: uniformly use the `cs_disasm` path, not `cs_disasm_iter` + stack structs (recorded in design/v1.2.0 CHANGELOG; a regression-guard comment is in the disasm source).

## ADR-06 Why Debug=/MDd, Release=/MT

- **Background**: both projects share build conventions; the runtime library must match or LNK2038 occurs.
- **Option comparison**: uniform /MDd (dynamic) → Release artifacts need the VC runtime DLL, inconvenient to distribute; Release with /MT (static) → single file with no DLLs.
- **Choice**: Debug=`/MDd` (x64-windows), Release=`/MT` (x64-windows-static). The vcpkg triplet switches in sync; keystone/capstone keep `BUILD_STATIC_RUNTIME` at its default OFF and follow the preset's `CMAKE_MSVC_RUNTIME_LIBRARY`, consistent with the library.

## ADR-07 Why breakpoint/watch/inject state is persisted to files in %TEMP%

- **Background**: the CLI is a "single-command" process (session = one process invocation), but breakpoints/watches/injections are long-lived cross-command state; the state must survive process exit and keep working on the next CLI call.
- **Option comparison**: memory-resident (cannot cross processes), registry (pollutes the system), `%TEMP%/deeptrace_<pid>/` state files (process-private, no cleanup protocol needed, isolated per pid).
- **Choice**: state files (`breakpoints.dat`/`watch.dat`/`inject.dat`, ASCII `|`-separated lines). Persistence is implemented by the service layer; the algorithm layer is not involved.

## ADR-08 Why the test target program has ASLR disabled

- **Background**: integration/e2e tests need "known values at known addresses", but ASLR randomizes addresses on every launch.
- **Option comparison**: parse addresses at runtime (complex, fragile) vs disable ASLR so addresses are deterministic (simple, assertable).
- **Choice**: the target uses `/DYNAMICBASE:NO /HIGHENTROPYVA:NO` to disable ASLR and prints a banner with the `PID:` line + variable address table (`g_int` etc.). The target does not link deeptrace; it is an independent executable test anchor.

## ADR-09 Why the static library does not merge third-party dependencies (consumers link explicitly)

- **Background**: `target_link_libraries(deeptrace PRIVATE capstone_static)` dependencies do not reach the CLI's link line — the CLI is an independent CMake project referencing deeptrace.lib via find_library, and linking fails with unresolved `cs_disasm`/`cs_free`.
- **Option comparison**: merging capstone/keystone into deeptrace.lib (a static library does not propagate PRIVATE dependencies by nature; would require complex schemes like OBJECT libraries) vs consumers explicitly `find_library(keystone/capstone)` and link (transparent, matches CMake static-library conventions).
- **Choice**: consumers link explicitly (implemented in cli/src/CMakeLists.txt with an explanatory comment — do not remove).

## ADR-10 Why the state directory is `deeptrace_<pid>` with per-pid state file isolation

- **Background**: breakpoint/watch state for the same target process must be unique and stable across invocations; state for different pids must not pollute each other.
- **Option comparison**: a single global file (multi-process conflicts) vs per-pid subdirectories (`%TEMP%/deeptrace_<pid>/`, naturally isolated, directory name contains the pid for traceability).
- **Choice**: `%TEMP%/deeptrace_<pid>/` (implemented by `state_dir()` in session.cpp).

## ADR-11 Why the CLI exposes a single `debug run` entry (one invocation = one debug session)

- **Background**: dynamic debugging is stateful — it depends on an active debug session (attached debuggee, armed breakpoints, paused threads). The CLI is a stateless, non-interactive, one-shot command tool. Exposing debug primitives as standalone commands (v1.3.0/v2.0.0 style `debug step`/`debug break`/`debug registers`) lets each invocation auto-attach/detach around a single operation, which produces wrong semantics.
- **Observed problems (measured on the real target)**: `debug step` run twice returned the identical RIP (a fake step — no session, no real execution); `debug break` left a residual `0xCC` in target memory with no cleanup (pollution that can crash the debuggee when the INT3 is hit without a debugger); `debug registers`/`debug status` read a non-paused context / handle-only state (misleading); `debug attach` attached and detached with nothing in between (meaningless).
- **Option comparison**: (a) keep standalone commands and accept broken semantics; (b) keep the commands but make each one hold a session across calls (state persists outside the call — contradicts the stateless model); (c) converge to a single scripted entry where one call = one complete session with cleanup guaranteed.
- **Choice**: (c) — since v2.1.0 the debug group is a single `debug run <script.json>` entry; the 15 standalone debug commands were removed (parse-time rejection, exit code 2). The library keeps all debug APIs (they power `debug run` and external callers).

## ADR-12 Why debug sessions are scripted (JSON step files) instead of interactive

- **Background**: a real debugging session needs a sequence of operations (set breakpoints → run → inspect registers → step → read/write memory → clear breakpoints). Neither a one-shot CLI flag nor an interactive REPL fits the stateless batch model of deeptrace_cli.
- **Option comparison**: (a) interactive REPL (adds a TUI/state machine to a batch tool); (b) a single `debug run` flag per operation (can't express a multi-step session); (c) a script file: a JSON array of steps executed in one call, session state kept in memory, cleaned up on failure or success.
- **Choice**: (c) — `debug run <script.json>` (v2.0.0). The step table in `cli/src/interface/script.cpp` fully covers the library's debug capabilities (break/clear/hbreak/hclear/guard/unguard/pause/resume/step/next/continue/status/registers/register + read/write/disasm/watch_*); unknown ops/fields/values are rejected at validation time (exit code 2). No control flow (conditions/loops/variables), no cross-call session persistence, and no breakpoint-hit callbacks are supported by design.

## ADR-13 Why the script engine is a CLI-side keyword engine backed by persisted static-library records

- **Background**: users want CE-style `.aa` scripts (alloc/label/registersymbol/createThread/db + `[ENABLE]`/`[DISABLE]` blocks) executed in the target; the library and CLI must share a clean boundary.
- **Option comparison**: (a) implement all script semantics inside the library (a mini language in the lib, hard to evolve); (b) CLI owns parsing/execution and the library exposes only orthogonal primitives (`script_alloc`, `script_alloc_near`, `script_free`, `hook_set`, `hook_clear`, `asm_assemble_labels`, `script_symbol`, `thread_create_at`) with per-PID persisted records.
- **Choice**: (b). The library stays a primitive provider; the CLI's `cmd_script.cpp` parses and drives enable/disable idempotently. Named allocations and enable-state persist per-PID (`scripts.dat`, hooks in `hooks.dat`), so re-attaching later can resolve symbols (`script_symbol`) and `script status` can list the landscape. Idempotent enable/disable (repeated enable skips, repeated disable skips) makes re-running scripts naturally stateless.

## ADR-14 Why pointer-chain search is two-phase (snapshot + rescan)

- **Background**: a single reverse-walk from a value address produces many coincidence-based false positives; after a game restart the value address moves, so static chains are not enough.
- **Option comparison**: (a) snapshot only (fast but noisy); (b) snapshot + rescan: re-evaluate saved chains against the new value address after a restart, keeping only chains whose final address still lands within ±max_offset of it (the Cheat Engine-style workflow); (c) a dedicated pointer-chain command with built-in persistence (heavier, duplicates mem batch).
- **Choice**: (b) — `pointer_map_snapshot` + `pointer_map_rescan` (v2.12.0), exposed to the CLI as `resolve ptrscan <file.json>` with full parameter control (max_offset default 2048, max_level default 5) and default module anchoring. Rescanning intersects away false positives; chains are consumable by `mem batch` for the search→verify loop.

## ADR-15 Why the thread pool is self-built (pure std::thread) instead of a third-party library

- **Background**: the pointer scan needs parallel memory-sweep chunks; adding a heavyweight dependency (TBB/OpenMP) to a static library has build/runtime cost.
- **Option comparison**: third-party pool (extra dep, linking burden on consumers) vs a minimal `std::thread` pool with `enqueue`/`wait`/`pending` semantics (~100 lines, thread count = `hardware_concurrency` or caller-supplied).
- **Choice**: self-built `infrastructure/threadpool` (v2.12.0). Zero third-party dependencies, deterministic lifecycle (join on destruction), unit-tested (`ThreadPool.*`); the scan reuses one pool across levels.

## ADR-16 Why complex interactions are JSON-config-driven instead of dedicated commands

- **Background**: pointer chains, batched reads/writes, and pointer-map scans need structured multi-parameter input; the CLI's flat flags cannot express them cleanly, and dedicated per-feature commands would bloat the command surface.
- **Option comparison**: (a) dedicated commands for each complex feature (pointer-chain read, batch, ptrscan); (b) a JSON config file as the input carrier (`mem batch read|write <file.json>`, `resolve ptrscan <file.json>`), keeping commands explicit and composable; (c) a general-purpose scripting DSL.
- **Choice**: (b) — since v2.9.0. The JSON parser was extracted into a shared `interface/json.{h,cpp}` (v2.12.0) with a prefix-able error message; config errors exit 2 (stderr) consistently; batch output gained CSV/JSON export (`--format`, v2.10.0) for feeding other tools/AI.

## ADR-17 Why the conversion layer (asm/bin/hex) lives in the CLI instead of external Linux tools

- **Background**: asm→bin→hex conversions could be done with nasm/objdump/xxd under WSL, but the project already embeds Keystone/Capstone.
- **Option comparison**: external tools (second syntax/semantics, requires WSL, inconsistent Intel-asm dialect) vs in-CLI commands reusing the embedded engines.
- **Choice**: in-CLI. The conversion layer is pure data (no session): `asm file` (asm→bin, v2.2.0), `hex2bin` (v2.2.0), `bin2hex` (v2.13.0), `disasm file` (bin→asm via the new session-free `disasm_buffer`, v2.13.0). `disasm file`/`bin2hex`/`asm file` are excluded from the `-p` auto-attach (no session needed). The ring asm↔bin↔hex is fully closed offline.

## ADR-18 Why attach permissions are surfaced via a recorded mask (session_permissions)

- **Background**: attach can succeed with a *degraded* access mask (fallback rights), so later memory operations fail obscurely without a clear reason.
- **Option comparison**: probe permissions after attach (extra syscalls) vs record the actual granted mask during attach itself (zero extra probes).
- **Choice**: record at attach (v2.11.0) — the session stores the granted `PROCESS_*` mask; `session_permissions()` reports it and `ps attach` prints a semantic summary (`OK (permissions: read|write|...)`). Access-denied cases still surface `AccessDenied` with the original semantics.

## Known Limitations and Trade-offs

- Windows x64 only; no cross-platform plans (the public header already uses standard types, preserving theoretical portability).
- Breakpoint state files remain in %TEMP% after the target process exits (harmless, but needs manual cleanup).
- Some debug operations (hardware breakpoints/page guards) depend on x64 architecture capabilities; non-x64 targets are unsupported.
- e2e requires a Debug build + testdll.dll; Release packaging contains only deeptrace_cli.exe (no test artifacts).
- Debug scripts intentionally support no control flow (no conditionals/loops/variables) and no breakpoint-hit callbacks; complex debugging scenarios are scripted by the caller (e.g. an AI tool generating the JSON).
- `.aa` script engine keywords are idempotent (enable/disable), but the engine does **not** implement control flow or conditionals either; call-type scripts run the full enable+disable cycle in one call, hook-type scripts persist until disabled.
- `script_alloc_near` never falls back to arbitrary placement: if no free region exists within ±2 GB of the anchor it returns `Error` (the caller must pick another anchor).
- Pointer-chain scans are heuristic: multi-target ambiguity within ±max_offset is resolved to the lowest candidate (documented), and rescan is the intended false-positive filter; `max_results` caps snapshot output by design.

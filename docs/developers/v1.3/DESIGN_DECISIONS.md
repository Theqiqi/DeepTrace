# Technical Decision Records (DESIGN_DECISIONS)

> Audience: maintainers. Each decision records "background → option comparison → final choice → rationale", not just the conclusion.
> Basis: tech choices and implementation pitfalls from design/v1.0~v1.2 (design/v1.2/deeptrace/00_CHANGELOG.md).

## ADR-01 Why a "static library + separate CLI" two-project setup

- **Background**: capability (process memory operations) and interaction (command line) are products with different lifecycles; the library needs to be called multiple times and reused, and the CLI is just one consumer of the library.
- **Option comparison**:
  - Single project: library and CLI mixed into one CMake target → library cannot be reused or tested independently.
  - Two independent CMake projects: cli references deeptrace's output via `find_library` + include path → library can be delivered independently (design convention: `deeptrace.lib` + `deeptrace.h`, no install intermediate layer).
- **Choice**: two projects. The CLI is the library's **first and currently only** consumer; the acceptance criterion for the library API design is "the CLI can call it cleanly".

## ADR-02 Why deeptrace uses four layers (domain/algorithm/infrastructure/service)

- **Background**: process memory operations involve two essentially different kinds of logic — pure computation (hex/AOB/decoding) and system calls (WinAPI) — which are hard to test and replace when mixed together.
- **Option comparison**:
  - Two layers (interface + implementation): WinAPI and algorithms inlined → algorithms not unit-testable, engines not replaceable (the v1.0 hand-written decoder that couldn't be replaced with Capstone was the lesson).
  - Four layers: the algorithm layer is pure computation with no I/O (independently unit-testable); infrastructure only wraps "one system call" per file; service composes and persists.
- **Choice**: four layers. Constraints: the algorithm layer forbids WinAPI/I/O; service forbids direct WinAPI; dependencies point one-way downward. After layering, the v1.2 engine replacements (hand-written decoder → Capstone, hand-written encoder → Keystone) were achieved with zero changes to service/public APIs/CLI — this is the payoff of four-layer layering (engine adaptation is confined to infrastructure internals).

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
- **Choice**: uniformly use the `cs_disasm` path, not `cs_disasm_iter` + stack structs (recorded in design/v1.2 CHANGELOG; a regression-guard comment is in the disasm source).

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

## Known Limitations and Trade-offs

- Windows x64 only; no cross-platform plans (the public header already uses standard types, preserving theoretical portability).
- Breakpoint state files remain in %TEMP% after the target process exits (harmless, but needs manual cleanup).
- Some debug operations (hardware breakpoints/page guards) depend on x64 architecture capabilities; non-x64 targets are unsupported.
- e2e requires a Debug build + testdll.dll; Release packaging contains only deeptrace_cli.exe (no test artifacts).

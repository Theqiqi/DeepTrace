# API Documentation Change Log

## v2.1.0 (2026-08-11)

Incremental update matching code tags `v2.0.0` (deeptrace library) / `v2.1.0` (deeptrace_cli). v1.3.0 → v2.1.0 changes:

**Added**
- New public API `deeptrace::debug_continue` (run the debuggee until a software breakpoint, another exception, process exit, or timeout) — documented in [Modules/DEBUG.md](Modules/DEBUG.md#deeptracedebug_continue).
- New public struct `ContinueInfo` (stop-reason output of `debug_continue`) — documented in [Types/STRUCTS.md](Types/STRUCTS.md#continueinfo-debug-continue-stop-reason).
- API inventory now **56 public functions**, **3 enums**, **12 structs** (55 + 1 function, 11 + 1 struct vs v1.3.0).

**Changed**
- `ContinueInfo.exception` field carries the exception code (final field name `exception`; an intermediate design name `exception_code` was dropped because it collides with the Windows SDK `excpt.h` macro).
- `debug_continue` behavior notes: the system-loader breakpoint (raised at attach inside ntdll) is skipped internally and detach events are drained, so a fresh `debug_continue` reliably waits for a user-set breakpoint; for a self-set software breakpoint the reported RIP is the **post-instruction** RIP (the breakpoint instruction has been executed) — documented in [Modules/DEBUG.md](Modules/DEBUG.md#deeptracedebug_continue).
- README overview updated: 55 → 56 public APIs; session-lifecycle diagram gains the `debug_continue` branch.

**Unchanged**
- All 55 pre-existing public APIs keep their signatures and semantics (backward compatible).
- GettingStarted / all standalone Examples unchanged and still compile (verified against `deeptrace.lib`).

## v1.3.0 (2026-08-09)

First version of the API reference, matching code tag `v1.3.0` (first release after the project was renamed DeepTrace).

**Added**
- Complete API inventory: **55 public functions**, **3 enums** (`Result`/`ValueType`/`BreakpointType`), **11 structs**, zero omissions.
- Documentation structure:
  - `README.md` — API overview, group overview, calling prerequisites (session lifecycle/privileges/persistence/thread safety)
  - `GettingStarted.md` — a complete from-zero example (compilable and runnable directly)
  - `Modules/` — 10 module docs (process & session/memory/module/thread/debug/disassembly/assembly/resolution/watch/injection)
  - `Types/` — `RESULT.md` (14 error-code trigger conditions), `ENUMS.md`, `STRUCTS.md`
  - `Examples/` — 3 complete examples + compilable source + `build_examples.bat`
- Example verification: all 3 examples compile with MSVC C++20 linking `deeptrace.lib`.

**Conventions**
- Each function doc contains: syntax (exactly matching `deeptrace.h`), parameter table, return-value table, behavior description, prerequisites/postconditions, example, header, see also.
- Docs are archived per code version under `docs/api/v<version>/`; later versions update the corresponding directory incrementally.

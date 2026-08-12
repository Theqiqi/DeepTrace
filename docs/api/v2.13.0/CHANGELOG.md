# API Documentation Change Log

## v2.13.0 (2026-08-12)

Incremental update matching code tags `v2.2.0` … `v2.13.0` (deeptrace library). v2.1.0 → v2.13.0 changes:

**Added (18 new public functions, 4 new structs)**
- v2.2.0 — shellcode lifecycle trio:
  - `deeptrace::shellcode_alloc` (allocate + write executable memory, **no** execution) — [Modules/INJECT.md](Modules/INJECT.md#deeptraceshellcode_alloc)
  - `deeptrace::shellcode_run` (trigger the recorded allocation once via a remote thread; repeatable) — [Modules/INJECT.md](Modules/INJECT.md#deeptraceshellcode_run)
  - `deeptrace::shellcode_free` (wait for the run thread to finish, then release the allocation) — [Modules/INJECT.md](Modules/INJECT.md#deeptraceshellcode_free)
- v2.3.0 — script engine / hook / label assembly / arbitrary-address thread creation:
  - `deeptrace::script_alloc` (allocate remote memory bound to a script symbol, persisted per-PID) — [Modules/SCRIPT.md](Modules/SCRIPT.md#deeptracescript_alloc)
  - `deeptrace::script_free` (release the memory and remove the symbol) — [Modules/SCRIPT.md](Modules/SCRIPT.md#deeptracescript_free)
  - `deeptrace::script_enable` / `deeptrace::script_disable` (persist script enable state, idempotent per path) — [Modules/SCRIPT.md](Modules/SCRIPT.md#deeptracescript_enable)
  - `deeptrace::script_status` (list script records with their hooks and allocations) — [Modules/SCRIPT.md](Modules/SCRIPT.md#deeptracescript_status)
  - `deeptrace::hook_set` / `deeptrace::hook_clear` (5-byte `jmp` patch with saved original bytes, idempotent, rollback on persist failure) — [Modules/HOOK.md](Modules/HOOK.md#deeptracehook_set)
  - `deeptrace::asm_assemble_labels` (multi-line assembly with label definitions/references and external symbols) — [Modules/ASM.md](Modules/ASM.md#deeptraceasm_assemble_labels)
  - `deeptrace::thread_create_at` (create a remote thread at an arbitrary address) — [Modules/THREAD.md](Modules/THREAD.md#deeptracethread_create_at)
- v2.6.0 — `deeptrace::script_symbol` (read-only lookup of a script symbol's recorded address) — [Modules/SCRIPT.md](Modules/SCRIPT.md#deeptracescript_symbol)
- v2.7.0 — `deeptrace::script_alloc_near` (allocate within ±2 GB of an anchor for RIP-relative rel32 range; never falls back to arbitrary placement) — [Modules/SCRIPT.md](Modules/SCRIPT.md#deeptracescript_alloc_near)
- v2.11.0 — `deeptrace::session_permissions` (query the actual PROCESS_* access mask granted by the last successful attach) — [Modules/PROCESS.md](Modules/PROCESS.md#deeptracesession_permissions)
- v2.12.0 — pointer-chain reverse scan:
  - `deeptrace::pointer_map_snapshot` (reverse-walk from a target value address, optional module anchoring, thread-pool accelerated) — [Modules/POINTERSCAN.md](Modules/POINTERSCAN.md#deeptracepointer_map_snapshot)
  - `deeptrace::pointer_map_rescan` (re-evaluate saved chains against a new target address to filter coincidence-based false positives) — [Modules/POINTERSCAN.md](Modules/POINTERSCAN.md#deeptracepointer_map_rescan)
- v2.13.0 — `deeptrace::disasm_buffer` (session-free disassembly of a local byte buffer; backs the CLI `disasm file` command) — [Modules/DISASM.md](Modules/DISASM.md#deeptracedisasm_buffer)
- New public structs: `HookInfo` (v2.3.0), `ScriptInfo` (v2.3.0), `PointerScanConfig`, `PointerChain` (v2.12.0) — [Types/STRUCTS.md](Types/STRUCTS.md)
- API inventory now **74 public functions** (73 `Result`-returning + `result_message`), **3 enums**, **16 structs** (56 + 18 functions, 12 + 4 structs vs v2.1.0).

**Changed**
- New documentation modules added: [Modules/SCRIPT.md](Modules/SCRIPT.md) (7 functions), [Modules/HOOK.md](Modules/HOOK.md) (2 functions), [Modules/POINTERSCAN.md](Modules/POINTERSCAN.md) (2 functions).
- Updated module docs for incremental additions: PROCESS (+`session_permissions`), THREAD (+`thread_create_at`), ASM (+`asm_assemble_labels`), DISASM (+`disasm_buffer`), INJECT (+`shellcode_alloc`/`shellcode_run`/`shellcode_free`).
- [Types/RESULT.md](Types/RESULT.md) trigger conditions extended: `InvalidArg` (duplicate script symbol, zero pointer-scan config fields, zero hook addresses), `NotFound` (script symbol not registered, anchored module not loaded, missing shellcode/hook records), `Timeout` (shellcode_free waiting on a still-running remote thread), `Error` (near-allocation window exhausted, persist-failure rollbacks).
- [README.md](README.md) overview updated: 56 → 74 public APIs; group table gains the Script / Hook / Pointer-chain scan modules; session-lifecycle diagram unchanged (the new APIs all ride the existing attach/detach session).
- State persistence extended: `%TEMP%/deeptrace_<pid>/scripts.dat` now carries script symbol / hook / enable records alongside `breaks.dat` / `injects.dat` / `watch.dat`.

**Unchanged**
- All 56 pre-existing public APIs keep their signatures and semantics (backward compatible).
- `GettingStarted.md` and the three v2.1.0 examples are unchanged and still compile (verified against `deeptrace.lib`); a new [Examples/pointer_chain_scan.md](Examples/pointer_chain_scan.md) example was added.

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

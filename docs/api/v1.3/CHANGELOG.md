# API Documentation Change Log

## v1.3 (2026-08-09)

First version of the API reference, matching code tag `v1.3` (first release after the project was renamed DeepTrace).

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

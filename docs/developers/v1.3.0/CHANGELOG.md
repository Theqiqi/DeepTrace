# deeptrace Developer Documentation — Version Change Log

## v1.3.0 (release, 2026-08-10)

First developer documentation version, aligned with code tag `v1.3.0` and the API docs `docs/api/v1.3.0/`.

- **Release content**: full doc set shipped — README/BUILDING/ARCHITECTURE/TESTING/EXTENDING/DESIGN_DECISIONS/ANALYSIS/DESIGN/CHANGELOG
- **Verification**: link check with no dead links; EXTENDING's new-command example (ps list2) compiled and ran successfully; README 47 lines (≤50); code comments synced (deeptrace.h gained API doc pointers)
- **Review**: completeness/accuracy/structure review + reviewer agent recheck, 3 fixes (CHANGELOG list completed with ANALYSIS/DESIGN, ADR-02 wording, EXTENDING service header convention)

## v1.3.0 (initial version)

First developer documentation version, aligned with code tag `v1.3.0` and the API docs `docs/api/v1.3.0/`.

- **Scope**: two projects — the deeptrace static library (deeptrace/) and the deeptrace_cli command-line program (cli/)
- **Document list**:
  - `README.md` — project overview + quick start (entry)
  - `BUILDING.md` — build guide (Debug/Release, WSL bridge, common issues)
  - `ARCHITECTURE.md` — architecture overview (deeptrace four layers + cli three layers, data flow, cross-project dependencies)
  - `TESTING.md` — testing guide (unit/integration/e2e, test target program, writing new tests)
  - `EXTENDING.md` — extension guide (adding commands/APIs/algorithms, engine replacement)
  - `DESIGN_DECISIONS.md` — technical decision records (ADR)
  - `ANALYSIS.md` / `DESIGN.md` — analysis/design phase output (code analysis, reader profiles, doc requirements, structure design)
  - `CHANGELOG.md` — this file
- **Corresponding code versions**: deeptrace library v1.0.0, deeptrace_cli v1.0.0 (55 public APIs, see `docs/api/v1.3.0/`)

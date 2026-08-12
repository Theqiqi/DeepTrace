# deeptrace_cli User Documentation — Version Change Log

## v2.13.0 (2026-08-12)

Incremental update matching code tag `v2.13.0` (deeptrace_cli).

- **Corresponding code version**: deeptrace_cli v2.13.0. New capability areas since v2.1.0:
  - **AA-style script engine** (v2.3.0): `script run <file.aa>` / `script disable <file>` / `script status` / `script check <file>` — CE-style scripts with `[ENABLE]`/`[DISABLE]` blocks, `alloc`/`label`/`registersymbol`/`createThread`/`db`/hook keywords; idempotent enable/disable; `script check` validates syntax without touching the target.
  - **Symbol addressing** (v2.6.0): any `<address>` argument also accepts a script symbol name (`mem read sunObjPtr`).
  - **Batch read/write** (v2.9.0): `mem batch <read|write> <file.json>` executes JSON-defined locators (pointer chains / module+offset / symbol+offset / absolute); **CSV/JSON export** (`--format`, `--out`) added v2.10.0.
  - **Pointer-chain scan** (v2.12.0): `resolve ptrscan <file.json>` — snapshot + rescan workflow, module anchoring, chains feed back into `mem batch`.
  - **Conversion layer** (v2.2.0 / v2.13.0): `asm file`, `hex2bin`, `bin2hex`, `disasm file`, `shellcode injectfile/alloc/run/free/exec` — the asm↔bin↔hex ring is closed offline (no target process needed).
  - **Attach permission summary** (v2.11.0): `ps attach` now prints the granted permissions.
- **Updated documents**: `README.md` (one-line intro kept; command areas list extended), `GETTING_STARTED.md` (version samples bumped to v2.13.0; note that addresses can be symbol names), `USER_MANUAL.md` (new §Shellcode lifecycle, §Script engine, §Batch, §Pointer-chain scan, §Conversion layer; ps attach permission output; disasm file; command quick reference extended), `FAQ.md` (script/batch/ptrscan/conversion questions), `TROUBLESHOOTING.md` (error table rows for script/ptrscan/batch config errors; known limitations updated), `CHANGELOG.md` (this file).
- **Output samples**: captured from real runs of deeptrace_cli.exe (Debug build) against deeptrace_target.exe — script run/status/disable/check, bin2hex, convert, asm file, help text v2.13.0.
- **Verification**: README stays ≤ 10 lines; link/anchor checks pass.
- **Review**: usability/completeness/consistency review + reviewer agent recheck; no open issues.

## v2.1.0 (2026-08-11)

Incremental update matching code tag `v2.1.0` (deeptrace_cli).

- **Corresponding code version**: deeptrace_cli v2.1.0. The debug command group now has a **single entry `debug run <script.json>`**; all 15 standalone debug sub-commands (`debug attach/detach/pause/resume/step/next/break/clear/hbreak/hclear/guard/unguard/status/registers/register`) were removed and now report `Error: unknown command` (exit code 2).
- **Updated documents**: `USER_MANUAL.md` — §5 Debugging rewritten around `debug run` with the full script step table and a real script example; command summary table updated; `TROUBLESHOOTING.md` — removed-command error row + `debug run` usage hints; `FAQ.md` — breakpoint/watch cleanup answer now uses a `debug run` script; `CHANGELOG.md` (this file).
- **Output samples**: `debug run` script-session output from real runs of deeptrace_cli.exe (Debug build) against deeptrace_target.exe; script example mirrors the repository fixture `cli/test/scripts/debug_session.json`.
- **Verification**: README stays ≤ 10 lines; link/anchor checks pass.
- **Review**: usability/completeness/consistency review + reviewer agent recheck; no open issues.

## v1.3.0 (release, 2026-08-10)

First user documentation version, aligned with code tag `v1.3.0`, the API docs `docs/api/v1.3.0/`, and the developer docs `docs/developers/v1.3.0/`.

- **Release content**: full doc set shipped — README/GETTING_STARTED/USER_MANUAL/FAQ/TROUBLESHOOTING/ANALYSIS/DESIGN/CHANGELOG
- **Output samples**: all command outputs from real runs of deeptrace_cli.exe (Debug build) + deeptrace_target.exe, not fabricated
- **Review**: usability/completeness/consistency review + reviewer agent recheck; 5 fixes (debug detach alone reports NotAttached, FAQ/TROUBLESHOOTING anchors, Error: Error(...) format explanation, getting-started example output softened, debug pause needs no prior attach)
- **Verification**: README 8 lines (≤10); link/anchor checks pass; output samples cross-checked one by one against real captures

## v1.3.0 (initial version)

First user documentation version, aligned with code tag `v1.3.0`, the API docs `docs/api/v1.3.0/`, and the developer docs `docs/developers/v1.3.0/`.

- **Scope**: the deeptrace_cli command-line tool (deeptrace_cli.exe)
- **Document list**:
  - `README.md` — product intro (one line) + quick links
  - `GETTING_STARTED.md` — quick start: download/install, first process listing, first memory read
  - `USER_MANUAL.md` — user manual: per-command-group tasks (ps/mem/module/thread/debug/disasm/resolve/watch/dll/asm/shellcode)
  - `FAQ.md` — frequently asked questions (ordered by frequency)
  - `TROUBLESHOOTING.md` — troubleshooting (error reference table)
  - `CHANGELOG.md` — this file
- **Output samples**: all command outputs from real runs of `deeptrace_cli.exe` (Debug build), captured against the `deeptrace_target.exe` test target
- **Corresponding code version**: deeptrace_cli v1.3.0 (see `deeptrace_cli -h` for the command list)

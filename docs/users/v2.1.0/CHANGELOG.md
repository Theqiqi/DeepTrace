# deeptrace_cli User Documentation — Version Change Log

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

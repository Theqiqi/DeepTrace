# deeptrace_cli User Documentation — Version Change Log

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
- **Corresponding code version**: deeptrace_cli v1.0.0 (see `deeptrace_cli -h` for the command list)

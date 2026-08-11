# Troubleshooting (TROUBLESHOOTING)

> Audience: users with problems. Check the [FAQ](FAQ.md) first, then this reference table.
> All error messages (Error lines) come from real runs.

## 1. Error Message Reference

| On-screen message (excerpt) | Meaning | What to do |
|------------------------------|---------|------------|
| `Error: Missing command. Use -h or --help for help.` | No command entered | Add a command, e.g. `deeptrace_cli ps list`; or `deeptrace_cli -h` for help |
| `Error: unknown command group: 'bogus'` | Command group misspelled | Check the group name (ps/mem/module/thread/debug/disasm/resolve/watch/dll/asm/shellcode) |
| `Error: invalid address: 'zzz'` | Address format is wrong | Use hex addresses starting with `0x`, e.g. `0x14000D000` |
| `Error: NoSuchProcess(99999999)` | Process doesn't exist | Use `ps list` to find the PID again; the process may have exited |
| `Error: NotAttached` | No target process attached/specified | Add `-p <pid>`, or `ps attach <pid>` first; inside a `debug run` session this means a step ran without a valid session (see [User Manual 5](USER_MANUAL.md#5-debugging-debug)) |
| `Error: unknown command: 'step'` (or `break`/`registers`/…) | A debug command that no longer exists (all standalone debug commands were removed in v2.1.0) | Use `debug run <script.json>` — every debug operation is now a step inside a script (see [User Manual 5](USER_MANUAL.md#5-debugging-debug) and [FAQ 6](FAQ.md#6-why-does-debug-step-or-debug-breakdebug-registers-report-error-unknown-command)) |
| `Error: AccessDenied` | Insufficient permissions | Run the command window as administrator; try a normal process |
| `Error: ReadFault` | Address unreadable | Use `mem regions` to find readable regions; check the address |
| `Error: WriteFault` | Address not writable | That memory region is read-only; find a writable region (protection allows writes) |
| `Error: NotFound` | Requested item not found (module/export, etc.) | Check the name spelling; use `module list` to confirm it's loaded |
| `Error: Error(<value>)` | Operation failed without a finer-grained reason (e.g. `thread suspend` on a non-existent tid) | Check the arguments (tid/address) are correct, retry |
| `Error: Timeout` | Operation timed out (e.g. waiting for DLL injection) | Retry; confirm the target isn't suspended/crashed |
| `Error: BadFormat` | Format error (assembly instructions/pattern, etc.) | Check the instruction syntax; separate pattern bytes with spaces |
| `Error: InvalidArg` | Invalid argument value | Check the argument type and range (e.g. type must be byte/word/dword/qword/float/double) |
| `Usage: deeptrace_cli [options] <command> [args...]` | Usage error (exit code 2) | Check the command and arguments; `deeptrace_cli -h` for usage |
| `internal exception: ...` | Internal program exception | Note the error message, make sure you're on the latest version, then report it to the maintainers |

**Exit code quick reference**: `0` success / `1` execution failure / `2` usage error. In scripts, check with `echo %errorlevel%`.

## 2. Known Limitations

- **Windows x64 only**: the target process must be a 64-bit program (32-bit processes are not supported).
- **Target process bitness**: the tool works reliably when both it and the target are 64-bit.
- **Protected processes**: games with anti-cheat/anti-debug protection may refuse reads/writes; this is expected behavior.
- **Memory regions**: `mem write` can only write "writable" regions; some regions (e.g. code sections) are read-only.
- **Hardware breakpoints are limited**: usually 4 (DR0-DR3).
- **State file residue**: after the target exits, record files remain under `%TEMP%\deeptrace_<pid>\` (harmless).
- **One command at a time**: the tool is command-line based; each run executes one command and exits (watch/injection state is kept, see [FAQ item 4](FAQ.md#4-why-do-watches-and-injection-records-persist-across-commands); debug is scripted via `debug run`, see [User Manual 5](USER_MANUAL.md#5-debugging-debug)).

## 3. Recommended: Practice on the Test Program First

The repo ships a test target program `deeptrace_target.exe` with address randomization disabled and fixed memory addresses, ideal for practice:

1. Launch `deeptrace_target.exe` by double-clicking (or from the command line); the window shows:
   ```
   PID: 26128
   g_int       = 0x11223344  @0x14000D000
   g_bytes[0]  = 0xDE @0x14000D018
   ```
   (The PID may differ each time; the addresses are fixed.)
2. Operate on it with the example addresses from this documentation:
   ```
   deeptrace_cli -p <PID shown above> mem read 0x14000D000 4 hex
   ```
   It should output `44 33 22 11` (the bytes of `0x11223344`).
3. When done, terminate it: `deeptrace_cli -p <PID> ps kill`, or just close the window.

> Practice the commands on the test program before operating on real targets to avoid mistakes.

## 4. Verifying Your Installation

```
deeptrace_cli -v        :: should show deeptrace_cli v2.1.0
deeptrace_cli -h        :: should show the command list
deeptrace_cli ps list   :: should show the process table
```

If all three work, the installation and environment are fine; the problem is in the specific command — go back to section 1 and work through the table.

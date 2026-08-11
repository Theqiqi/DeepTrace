# Frequently Asked Questions (FAQ)

> Audience: all users. Ordered by question frequency, highest first.
> Each question: a one-line question + a short answer. Detailed steps are in the [User Manual](USER_MANUAL.md).

## 1. What should I do when I see `'deeptrace_cli' is not recognized as an internal or external command`?

**Cause**: the command prompt can't find the program — you're not in the folder that contains it.
**Fix**: go back to the program's folder and run it again. For example:

```
cd /d C:\Users\you\Downloads\deeptrace_cli
deeptrace_cli -h
```

Or use the full path every time: `C:\Users\you\Downloads\deeptrace_cli\deeptrace_cli.exe -h`.

## 2. What does `Error: NoSuchProcess(1234)` mean?

**Cause**: the specified PID (1234) doesn't exist — the program exited, the PID was noted down wrong, or the PID changed because the program was restarted.
**Fix**: run `deeptrace_cli ps list` again to find the current PID, then retry.
**Note**: Windows can assign a different PID every time a program starts; re-confirm it before each use.

## 3. What should I do when I see `Error: AccessDenied`?

**Cause**: no permission to access the process — the target is a system/protected process, or your privileges are insufficient (64-bit targets, anti-cheat protection, etc.).
**Fix**:
- Try a normal program instead (e.g. Notepad);
- Open the command prompt **as administrator**: search `cmd` in the Start menu → right-click → "Run as administrator", then run the command;
- Game anti-cheat systems block external reads/writes by design; that is normal and not a bug in this tool.

## 4. Why do watches and injection records persist across commands?

**Cause**: by design — watches and injection records are saved to a temp directory (`%TEMP%\deeptrace_<pid>\`) and automatically restored the next time you run a command for the same process. Breakpoints are different: since v2.1.0 they only exist inside a `debug run` script session and are automatically restored/cleaned when that session ends, so nothing to clean up afterwards.
**Fix**: clear them explicitly when you don't need them: `watch clear` / `dll eject`; if you set a breakpoint in a script, add a `clear` step (or just end the session — it cleans up for you).
**Note**: after the target process **exits**, these record files remain in the temp directory; they are harmless and can be deleted manually from `%TEMP%`.

## 5. How do I set a breakpoint or step through code?

**Cause**: the debug commands are script-driven since v2.1.0 — there is no `debug break` or `debug step` command anymore.
**Fix**: write a small script and run it with `debug run <script.json>`, e.g.:

```json
[
  {"op": "break", "addr": "0x14000D000"},
  {"op": "continue", "timeout_ms": "10000"},
  {"op": "registers"},
  {"op": "step"},
  {"op": "clear", "addr": "0x14000D000"}
]
```

```
deeptrace_cli -p 1234 debug run my_script.json
```

Full step list and a real example: [User Manual §5](USER_MANUAL.md#5-debugging-debug).

## 6. Why does `debug step` (or `debug break`/`debug registers`/…) report `Error: unknown command`?

**Cause**: those standalone debug commands were removed in v2.1.0. In the old stateless model each run auto-attached and detached around a single operation, which produced wrong semantics (fake single-step, residual breakpoint bytes left in the target).
**Fix**: use `debug run <script.json>` — one invocation is one complete debug session; every debug operation (break/step/continue/registers/…) is a step inside the script. See [FAQ 5](#5-how-do-i-set-a-breakpoint-or-step-through-code) and [User Manual §5](USER_MANUAL.md#5-debugging-debug).

## 7. What does `Error: ReadFault` mean when reading memory?

**Cause**: the address is unreadable — outside the process's address space, in an unreadable region (e.g. near `0x00000000`), or mistyped.
**Fix**: first use `mem regions` to see which address ranges are readable, then read an address within one; check that the address starts with `0x` and has the right number of digits.
**Tip**: before reading/writing memory, use `mem regions` to confirm the address is inside a readable region.

## 8. I can't find the module/function I need?

**Cause**: `module list` only lists modules already loaded by the target process; a DLL that isn't loaded can't be found; the function name may be wrong in case or the module name incorrect.
**Fix**:
- Use `module list` to confirm the module is loaded (some DLLs load lazily on first use);
- Use `module exports <module>` with the actual module name (e.g. `kernel32.dll` — case-insensitive, but the extension is required).

## 9. `resolve scan` finds nothing?

**Cause**: the pattern is malformed (bytes must be space-separated), the bytes aren't hexadecimal, or the byte sequence doesn't exist in memory.
**Fix**:
- Format check: bytes separated by spaces, e.g. `"DE AD BE EF"`;
- `??` wildcard means any byte: e.g. `"48 8B ?? ?? 00"`;
- Patterns should be at least 4 bytes; the longer the better for precision;
- A pattern that spans pages or unreadable regions can be missed; try a shorter one.

## 10. `dll inject` doesn't succeed?

**Cause**: wrong path format (must use Windows paths), DLL bitness mismatch (a 64-bit process can only load 64-bit DLLs), or a protected target process.
**Fix**:
- Use Windows-style paths: `C:\path\to\test.dll` or `C:/path/to/test.dll`;
- Confirm the DLL is a 64-bit build;
- Use `dll list` to check whether it was already injected.

## 11. `asm assemble` reports `Error: BadFormat`?

**Cause**: the instruction syntax isn't supported or is malformed.
**Fix**:
- Quote the instructions: `deeptrace_cli asm assemble "nop; ret"`;
- Separate multiple instructions with `;`;
- Try an equivalent form (e.g. if `add rax, 0` errors, use `xor eax, eax`).

## 12. I can't understand the command arguments in the help?

**Fix**: the [User Manual](USER_MANUAL.md) explains every command's arguments with examples; you can also run `deeptrace_cli -h` anytime to see the command list.
**Tip**: optional parameters with defaults can be omitted, e.g. `mem read <address>` reads 1 byte in hex by default.

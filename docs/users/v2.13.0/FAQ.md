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

## 13. Why does `script run` say it needs `[ENABLE]` / why did my `[DISABLE]` run when I ran `script run`?

**Cause**: `script run` executes the `[ENABLE]` block only; `script disable <file>` executes the `[DISABLE]` block. Scripts with only one block are fine.
**Fix**: put "apply" steps (alloc/hook/thread) in `[ENABLE]` and "undo" steps (dealloc/unregistersymbol) in `[DISABLE]`. Both are **idempotent** — re-running the same block again is a no-op, so you can safely run `script run` and `script disable` in any order.
**Tip**: use `script check <file>` first to validate syntax without touching the target, and `script status` to see what's currently enabled.

## 14. `script check` reports an error but my script looks right?

**Cause**: `script check` validates syntax + hook structure + assembly feasibility (it actually assembles the code at a placeholder address) without attaching to anything.
**Fix**: read the exact line/symbol reported (e.g. an undefined label, an unknown instruction, or a hook target that has no `jmp` after it). Assembly errors are the most common: quote string arguments, make sure every referenced label/symbol is defined, and that `db` bytes are valid hex.

## 15. `mem batch` or `resolve ptrscan` rejects my JSON?

**Cause**: the config file failed validation — wrong version, missing required fields (`mem batch`: `locators`; `ptrscan`: `target`), unknown locator step kinds, or non-numeric offsets. These are rejected with exit code 2 **before** anything executes, so the target is untouched.
**Fix**: check the error message on stderr; see [User Manual §2.6](USER_MANUAL.md#26-batch-read--write--mem-batch-readwrite-filejson---format-tablecsvjson---out-file) and [§7.3](USER_MANUAL.md#73-pointer-chain-scan--resolve-ptrscan-filejson) for the exact field names.

## 16. `resolve ptrscan` finds nothing — or finds chains that stop working after a restart?

**Cause**: no qword pointer within ±`max_offset` of the target value → zero chains (common for stack-only values); or a snapshot hit was coincidence and doesn't survive the restart.
**Fix**: increase `max_offset`/`max_level`, make sure `module` names a loaded module (else `Error: NotFound`), and use the **rescan** step after a restart (`"rescan": {"target": "0x..."}`) to filter to chains that still reach the new value. The surviving chains are the structurally real ones.

## 17. How do I keep reading a value that moves every restart (games)?

**Cause**: addresses change because of ASLR; a raw address is useless next run.
**Fix**: find a pointer chain once with `resolve ptrscan` (see [FAQ 16](#16-resolve-ptrscan-finds-nothing--or-finds-chains-that-stop-working-after-a-restart)), then put that chain into a `mem batch` locator file — the batch locator re-resolves `module+offset` steps against the new module base each run, so the same locator works every time. After a game update that moves offsets, re-scan and update the locator.

## 18. Can I use the conversion commands (asm file / bin2hex / disasm file) without a target?

**Yes**: the conversion layer is pure data — `asm file`, `hex2bin`, `bin2hex`, `disasm file`, `convert`, and `script check` need **no** `-p` and no attached process. This is by design (v2.13.0).

## 19. What does the permission list after `ps attach` mean?

**Cause**: `ps attach` now prints which process rights were actually granted (`read|write|vm_operate|create_thread|query|...`) — a degraded attach may succeed but lack e.g. `write`, and later `mem write` will then fail with `AccessDenied`.
**Fix**: if the permission you need is missing, re-run the command window **as administrator** and attach again. Protected targets (anti-cheat) legitimately refuse most rights.

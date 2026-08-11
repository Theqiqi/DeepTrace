# User Manual (USER_MANUAL)

> Audience: users with some experience (already read the [Quick Start](GETTING_STARTED.md)).
> This document introduces each feature grouped by command. Each feature covers: when to use it, steps, expected output, and notes.
> Output samples are all from real runs of `deeptrace_cli.exe` (against the test target `deeptrace_target.exe`, fixed addresses).

## 0. General Notes

### Command Format

```
deeptrace_cli [options] <command group> <action> [args...]
```

- **Options** (before the command): `-p <pid>` specifies the target process; `-h` help; `-v` version.
- **Command group + action**: e.g. `ps list` (process-list), `mem read` (memory-read).
- **Addresses**: hexadecimal, prefixed with `0x`, e.g. `0x14000D000`.
- **Tip**: any time you can run `deeptrace_cli -h` to see the full command list.

### Exit Codes

| Exit code | Meaning |
|-----------|---------|
| 0 | Success |
| 1 | Execution failure (e.g. process not found, memory read failed) |
| 2 | Usage error (wrong command, bad arguments) |

### About the "Target Process"

Most commands need a target process first: `-p <pid>`. Process IDs (PIDs) are listed with `ps list`. Without `-p`, the tool operates on the **currently attached process** (usually one you specified earlier with `-p`, or one you attached with `ps attach`).

> Every `deeptrace_cli` run is an independent operation; watches and injection records **persist across commands** (stored in a temp directory, see [FAQ](FAQ.md#4-why-do-watches-and-injection-records-persist-across-commands)). Breakpoints exist only inside a `debug run` session and are automatically restored/cleaned when the session ends (see [Debugging](#5-debugging-debug)).

---

## 1. Processes (ps)

### 1.1 List Processes — `ps list`

- **When to use**: to see which programs are running on the system, or to find the target program's PID.
- **Steps**:
  ```
  deeptrace_cli ps list
  ```
- **Expected output** (real sample, excerpt):
  ```
  PID        NAME                                     THREADS  PPID
  0          [System Process]                         24       0
  4          System                                   361      0
  ```
  One row per process: PID / name / thread count / parent PID.
- **Note**: the list is long; scroll up in the command prompt window, or right-click the window → "Mark" to select and copy.

### 1.2 Attach to a Process — `ps attach <pid>`

- **When to use**: to keep targeting a process (so you can operate on it without `-p` later).
- **Steps**:
  ```
  deeptrace_cli ps attach 1234
  ```
- **Expected output**:
  ```
  OK
  ```
- **Note**: a non-existent PID reports `Error: NoSuchProcess(<pid>)`.

### 1.3 Detach — `ps detach`

- **When to use**: to end the session with the current process.
- **Steps**: `deeptrace_cli ps detach`
- **Expected output**: `OK`

### 1.4 View Process Info — `ps info`

- **When to use**: to confirm the target process and see its details.
- **Steps**: `deeptrace_cli -p 1234 ps info`
- **Expected output** (real sample):
  ```
  PID: 26128
  Name: deeptrace_target.exe
  Threads: 5
  ParentPID: 3592
  ```

### 1.5 Suspend / Resume / Terminate a Process

| Action | Command | Effect |
|--------|---------|--------|
| Suspend | `deeptrace_cli -p 1234 ps suspend` | pauses all threads of the process (frozen) |
| Resume | `deeptrace_cli -p 1234 ps resume` | lets a suspended process continue |
| Terminate | `deeptrace_cli -p 1234 ps kill` | terminates the process (optional exit code `ps kill 0`) |

- **Expected output**: `OK` for all.
- **Note**: `ps kill` **terminates** the target program directly; use with caution.

---

## 2. Memory (mem)

### 2.1 Read Memory — `mem read <address> [size] [format]`

- **When to use**: to view the contents at an address in the target process. Formats: `hex` (hexadecimal, default) / `dec` (decimal) / `bin` (binary) / `ascii` (characters).
- **Steps** (read 4 bytes starting at `0x14000D000`):
  ```
  deeptrace_cli -p 1234 mem read 0x14000D000 4 hex
  ```
- **Expected output** (real sample):
  ```
  44 33 22 11
  ```
  These are the 4 bytes in hexadecimal (2 digits per byte, space-separated).
- **Note**: an unreadable address reports `Error: ReadFault`; insufficient permission reports `Error: AccessDenied` (see [Troubleshooting](TROUBLESHOOTING.md)).

### 2.2 Write Memory — `mem write <address> <value> [format]`

- **When to use**: to modify the target process's memory. Values are hexadecimal by default (e.g. `CAFEBABE` is 4 bytes).
- **Steps** (write, then read back to confirm):
  ```
  deeptrace_cli -p 1234 mem write 0x14000D000 CAFEBABE hex
  deeptrace_cli -p 1234 mem read 0x14000D000 4 hex
  ```
- **Expected output** (real sample):
  ```
  OK
  CA FE BA BE
  ```
- **Note**: writing memory can crash the target program or change its behavior; practice on the test program first.

### 2.3 Hex Dump — `mem dump <address> <size>`

- **When to use**: to see hexadecimal bytes and their character equivalents side by side (like a hex editor).
- **Steps**: `deeptrace_cli -p 1234 mem dump 0x14000D000 16`
- **Expected output** (real sample):
  ```
  0x000000014000D000  44 33 22 11 D0 0F 49 40 88 77 66 55 44 33 22 11  |D3"..I@.wfUD3".|
  ```
  Left is the address, middle is the bytes, right `|...|` shows the printable characters for those bytes (non-printable shown as `.`).

### 2.4 List Memory Regions — `mem regions`

- **When to use**: to view the target process's memory layout (which address ranges are readable/writable) and find usable regions.
- **Steps**: `deeptrace_cli -p 1234 mem regions`
- **Expected output** (real sample, excerpt):
  ```
  BASE               SIZE           PROTECTION STATE
  0x0000000000000000 65536          0x00000001    65536
  0x0000000140000000 4096           0x00000002    4096
  ...
  ```
- **Note**: column meanings: start address / size / protection attributes / state. The values are raw Windows values; regular users can just look at the address ranges.

### 2.5 Read Typed Values — `mem readval <address> <type>`

- **When to use**: when you don't want raw bytes but a typed value directly. Types: `byte` (1 byte) / `word` (2) / `dword` (4) / `qword` (8) / `float` (decimal) / `double` (double-precision decimal).
- **Steps**: `deeptrace_cli -p 1234 mem readval 0x14000D000 dword`
- **Expected output** (real sample):
  ```
  0x11223344
  ```

---

## 3. Modules (module)

### 3.1 List Modules — `module list`

- **When to use**: to see which program files (main program + DLLs) the target process has loaded.
- **Steps**: `deeptrace_cli -p 1234 module list`
- **Expected output** (real sample, excerpt):
  ```
  BASE               SIZE         NAME
  0x0000000140000000 73728        deeptrace_target.exe
  0x00007FFC98D00000 2514944      ntdll.dll
  ...
  ```

### 3.2 Find / Get Base Address — `module find <name>` / `module base <name>`

- **When to use**: to find a module's load address (base address).
- **Steps**: `deeptrace_cli -p 1234 module base deeptrace_target.exe`
- **Expected output** (real sample):
  ```
  0x0000000140000000
  ```

### 3.3 List Export Functions — `module exports <module>`

- **When to use**: to see which functions a DLL exports (often used to find the address of a target function).
- **Steps**: `deeptrace_cli -p 1234 module exports kernel32.dll`
- **Expected output**: a table with two columns: function name / address.

### 3.4 Dump a Module — `module dump <name> [output file]`

- **When to use**: to export a module's contents as hex text, or save it to a file.
- **Steps**: `deeptrace_cli -p 1234 module dump deeptrace_target.exe dump.txt`
- **Expected output**: `OK` (file created), or hex output on screen when no file name is given.

---

## 4. Threads (thread)

### 4.1 List Threads — `thread list`

- **When to use**: to view the target process's threads.
- **Steps**: `deeptrace_cli -p 1234 thread list`
- **Expected output** (real sample):
  ```
  TID        PRIORITY   START
  8124       8          0x0000000000000000
  ...
  ```

### 4.2 Suspend / Resume / Terminate Threads

| Action | Command |
|--------|---------|
| Suspend | `deeptrace_cli -p 1234 thread suspend <tid>` |
| Resume | `deeptrace_cli -p 1234 thread resume <tid>` |
| Terminate | `deeptrace_cli -p 1234 thread kill <tid>` |

- **Expected output**: `OK` for all.
- **Note**: `thread kill` terminates the thread, which can make the program misbehave.

---

## 5. Debugging (debug)

> Since v2.1.0 the debug group has a **single entry: `debug run <script.json>`** — one invocation is one complete debug session. The standalone debug commands of earlier versions (`debug attach/detach/pause/resume/step/next/break/clear/hbreak/hclear/guard/unguard/status/registers/register`) no longer exist; typing one reports `Error: unknown command: '<action>'` (exit code 2).

### 5.1 Why a script?

Debugging is **stateful** — it needs a live debug session (attached debuggee, armed breakpoints, paused threads), while every `deeptrace_cli` run is a single independent command. A script turns "one debug session" into "one command": the tool attaches → enters debug mode → runs your steps in order → cleans everything up → detaches. The target process always survives; software breakpoints and page guards are restored even when a step fails.

### 5.2 Script format

A script is a JSON array of steps. All field values are strings (addresses use `0x`):

```json
[
  {"op": "<operation>", "<field>": "<value>", ...}
]
```

Supported operations:

| op | fields | description |
|----|--------|-------------|
| `status` | — | show debug state (attached / pid / breakpoint counts) |
| `registers` | `tid` (optional) | show all registers of a thread (default: first thread) |
| `register` | `name`, `tid` (optional) | show one register (e.g. `"name": "rip"`) |
| `break` | `addr` | set a software breakpoint |
| `clear` | `addr` | clear a software breakpoint |
| `hbreak` | `addr`, `type`, `length` | set a hardware breakpoint (`type`: 0=execute / 1=write / 2=read-write; `length`: 1/2/4/8) |
| `hclear` | `addr` | clear a hardware breakpoint |
| `guard` | `addr`, `size` | guard a memory page (one-shot access watch) |
| `unguard` | `addr`, `size` | remove the page guard |
| `pause` | — | pause the target |
| `resume` | — | resume the target |
| `step` | `tid` (optional) | single-step one instruction |
| `next` | `tid` (optional) | step over calls |
| `continue` | `timeout_ms` (optional, default `5000`) | run until a breakpoint / process exit / timeout; the stop reason is reported |
| `read` | `addr`, `size`, `format` (optional) | read memory (`format`: hex/dec/bin/ascii, default hex) |
| `write` | `addr`, `bytes` | write memory; `bytes` is space-separated hex (e.g. `"BE BA FE CA"`) |
| `disasm` | `addr`, `count` (optional) | disassemble at an address |
| `watch_add` | `desc`, `addr`, `type` | add a watch |
| `watch_remove` | `index` | remove a watch |
| `watch_list` | — | list watches (with live values) |
| `watch_refresh` | — | refresh watch values |
| `watch_clear` | — | clear all watches |

### 5.3 Example: scripted debug session

Prepare a script file `session.json` (this example mirrors the repository fixture `cli/test/scripts/debug_session.json`; replace `0x14000D000` with the address of a value in the target — e.g. the `g_int` address printed by `deeptrace_target.exe`):

```json
[
  {"op": "status"},
  {"op": "registers"},
  {"op": "read", "addr": "0x14000D000", "size": "4"},
  {"op": "break", "addr": "0x14000D000"},
  {"op": "clear", "addr": "0x14000D000"},
  {"op": "watch_add", "desc": "sc_g", "addr": "0x14000D000", "type": "dword"},
  {"op": "watch_list"},
  {"op": "watch_clear"}
]
```

Run it:

```
deeptrace_cli -p 1234 debug run session.json
```

Expected output (real sample, excerpt):

```
[1] status
attached: yes
pid: 26128
breakpoints: 0
hw_breakpoints: 0
[2] registers
REG      VALUE
rax      0x0000000000000034
...
[3] read
44 33 22 11
[4] break
breakpoint set at 0x000000014000D000 (orig 0x44)
[5] clear
OK
[6] watch_add
OK
[7] watch_list
IDX    DESCRIPTION              ADDRESS            TYPE     VALUE                VALID
0      sc_g                     0x000000014000D000 dword    0x11223344           yes
[8] watch_clear
OK
```

Each step prints `[N] <op>` followed by its result. If a step fails, the session stops there and the target is cleaned up (breakpoints restored, guards removed) before detaching.

### 5.4 Exit codes

| Exit code | Meaning |
|-----------|---------|
| 0 | all steps succeeded |
| 1 | a step failed at runtime (session already cleaned up) |
| 2 | script file / format / validation error (e.g. unknown op, missing required field) |

### 5.5 Running to a breakpoint (`continue`)

To actually stop at a breakpoint, pair `break` with `continue`:

```json
[
  {"op": "break", "addr": "0x14000D000"},
  {"op": "continue", "timeout_ms": "10000"},
  {"op": "registers"}
]
```

When the target reaches the breakpoint address, `continue` stops and reports the stop reason (breakpoint hit / other exception / process exit / timeout); the following steps then run against the paused target.

### 5.6 Notes

- Every address in a script is a hexadecimal string with `0x`, e.g. `"0x14000D000"`.
- The target is never left in a broken state: software breakpoints are restored and page guards removed before the session ends, even on failure.
- Writing memory, breakpoints, and injection can crash or change the target — practice on the test program first.
- Standalone debug commands are gone: `deeptrace_cli -p 1234 debug step` now reports `Error: unknown command: 'step'` — write a script instead.

---

## 6. Disassembly (disasm)

### 6.1 Disassemble — `disasm at <address> [count]` / `disasm range <start> <end>`

- **When to use**: to translate a memory region (machine code) into assembly instructions and understand what the code does.
- **Steps**:
  ```
  deeptrace_cli -p 1234 disasm at 0x14000D018 3
  ```
- **Expected output** (real sample):
  ```
  ADDRESS            BYTES                INSTRUCTION
  0x000000014000D018 DE AD BE EF 48 8B    fisubr word ptr [rbp - 0x74b71042]
  0x000000014000D01E 45 08 90 90 90 90 CC or byte ptr [r8 - 0x336f6f70], r10b
  0x000000014000D025 C3                   ret
  ```
  Each row: address / raw machine code / assembly instruction.
- **Range disassembly**: `deeptrace_cli -p 1234 disasm range 0x14000D000 0x14000D100` (from the start address to the end address).

---

## 7. Resolution (resolve)

### 7.1 Resolve Module Base — `resolve base <module name>`

- **When to use**: same as `module base`; get the module base address.
- **Steps**: `deeptrace_cli -p 1234 resolve base deeptrace_target.exe`
- **Expected output**:
  ```
  0x0000000140000000
  ```

### 7.2 Pattern Scan (AOB) — `resolve scan <pattern>`

- **When to use**: when you don't know the address but know a distinctive byte sequence (e.g. a fixed value `DE AD BE EF`), scan the whole process memory for its occurrences. `??` means any byte.
- **Steps**:
  ```
  deeptrace_cli -p 1234 resolve scan "DE AD BE EF"
  ```
- **Expected output** (real sample):
  ```
  0x000000014000D018
  ```
- **Note**: quote the pattern; separate bytes with spaces; `??` is a wildcard (e.g. `"48 8B ?? ?? 00"`). There may be more than one matching address.

---

## 8. Watch

### 8.1 Add a Watch — `watch add <description> <address> <type>`

- **When to use**: to keep watching the value at an address (types are the same as `mem readval`: byte/word/dword/qword/float/double).
- **Steps**:
  ```
  deeptrace_cli -p 1234 watch add counter 0x14000D000 dword
  ```
- **Expected output**: `OK`.

### 8.2 View / Refresh — `watch list` / `watch refresh`

- **Steps**: `deeptrace_cli -p 1234 watch refresh`
- **Expected output** (real sample):
  ```
  IDX    DESCRIPTION              ADDRESS            TYPE     VALUE                VALID
  0      counter                  0x000000014000D000 dword    0x11223344           yes
  ```
  `VALID: yes` means the value was read successfully; `no` means it can't be read right now (e.g. address unreadable).
- **`watch list`** output is the same as refresh (listing also reads live values).

### 8.3 Remove / Clear — `watch remove <index>` / `watch clear`

- **Steps**: `deeptrace_cli -p 1234 watch remove 0` (removes entry 0) → `OK`; `watch clear` clears everything → `OK`.
- **Note**: the index is the `IDX` column in the table.

---

## 9. DLL Injection (dll)

### 9.1 Inject a DLL — `dll inject <dll path>`

- **When to use**: to make the target process load a DLL (e.g. a game mod/plugin).
- **Steps**:
  ```
  deeptrace_cli -p 1234 dll inject C:\path\to\testdll.dll
  ```
- **Expected output**: `OK` or injection info (path/address/tid).
- **Note**: use Windows-style paths (`C:\...` or `C:/...`). A 64-bit process can only load 64-bit DLLs.

### 9.2 Eject — `dll eject <path or address>`

- **Steps**: `deeptrace_cli -p 1234 dll eject C:\path\to\testdll.dll` → `OK`.

### 9.3 View — `dll list` / `dll status`

- **Steps**: `deeptrace_cli -p 1234 dll list`
- **Expected output** (real sample, empty state):
  ```
  KIND     PATH                                     ADDRESS            TID        RUNNING
  ```
  With injection records, one row per record: `type / path / remote address / tid / running?`.

---

## 10. Assembly (asm)

### 10.1 Assemble — `asm assemble <code> [--hex] [--c-array]`

- **When to use**: to translate assembly instructions into machine code (essential when writing shellcode/patches). Separate multiple instructions with `;`.
- **Steps**:
  ```
  deeptrace_cli asm assemble "nop; ret"
  ```
- **Expected output** (real sample):
  ```
  90C3
  ```
  (`90`=nop, `C3`=ret)
- **--hex output**: hex is the default; `--hex` specifies it explicitly.
- **--c-array output** (real sample):
  ```
  deeptrace_cli asm assemble "nop" --c-array
  unsigned char code[] = { 0x90 };
  ```
  Generates a C byte array directly, handy for pasting into code.
- **Note**: quote the assembly code (`"..."`). Unsupported instructions report `Error: BadFormat`.

---

## 11. Shellcode

### 11.1 Inject Shellcode — `shellcode inject <hex bytes>`

- **When to use**: to inject a snippet of machine code (e.g. `90 90 C3`) into the target process and execute it; the tool allocates memory automatically.
- **Steps**:
  ```
  deeptrace_cli -p 1234 shellcode inject "9090C3"
  ```
- **Expected output**: injection info (address/tid).
- **Note**: shellcode is risky and can crash the target process; verify on the test program first.

### 11.2 Inject at an Address — `shellcode injectat <address> <bytes>`

- **Steps**: `deeptrace_cli -p 1234 shellcode injectat 0x14000D000 "9090C3"`

### 11.3 View Status — `shellcode status`

- **Steps**: `deeptrace_cli -p 1234 shellcode status` → same table format as `dll list` (`KIND` is `shellcode`).

---

## 12. Command Quick Reference

| Command | Purpose |
|---------|---------|
| `ps list` | View processes |
| `ps attach <pid>` / `ps detach` | Attach/detach a process |
| `ps info` / `ps suspend` / `ps resume` / `ps kill` | Process info / suspend / resume / terminate |
| `mem read <addr> [size] [fmt]` | Read memory |
| `mem write <addr> <val> [fmt]` | Write memory |
| `mem dump <addr> <size>` | Hex dump |
| `mem regions` | Memory regions |
| `mem readval <addr> <type>` | Read typed values |
| `module list` / `find` / `base` / `exports` / `dump` | Module operations |
| `thread list` / `suspend` / `resume` / `kill` | Thread operations |
| `debug run <script.json>` | One scripted debug session (breakpoints / step / continue / registers / status + read / write / watch) |
| `disasm at <addr> [n]` / `disasm range <a> <b>` | Disassembly |
| `resolve base <mod>` / `resolve scan <pattern>` | Base address / pattern scan |
| `watch add/list/remove/refresh/clear` | Watches |
| `dll inject/eject/list/status` | DLL injection |
| `asm assemble <code> [--hex] [--c-array]` | Assembly |
| `shellcode inject/injectat/status` | Shellcode injection |

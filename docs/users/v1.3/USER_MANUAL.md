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

> Every `deeptrace_cli` run is an independent operation; breakpoints, watches, and injection records **persist across commands** (stored in a temp directory, see [FAQ](FAQ.md#4-why-do-breakpoints-and-watches-persist-across-commands)).

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

> The debug features put the target process into a "being debugged" state, allowing you to pause, single-step, set breakpoints, and view registers.

### 5.1 Enter Debug Mode — `debug attach`

- **Steps**:
  ```
  deeptrace_cli -p 1234 debug attach
  ```
- **Expected output**: `OK`.
- **Note**: `debug attach` does not terminate the target process (verified); after the command finishes, debugging exits automatically and the target process continues running normally. Protected processes may report `Error: AccessDenied`.
- **Explanation**: every command run is an independent operation; debug sessions are not preserved across commands — so running `debug detach` alone reports `Error: NotAttached` (no debug session currently, which is normal).

### 5.2 Pause / Resume — `debug pause` / `debug resume`

- **When to use**: to stop the process (convenient for editing memory / checking state) or let it continue.
- **Steps**: `deeptrace_cli -p 1234 debug pause` → the process pauses; `debug resume` → it continues.
- **Expected output**: `OK`.
- **Note**: `debug pause`/`debug resume` work directly with `-p`; a prior `debug attach` is not required (pausing is handled automatically).

### 5.3 Single Step — `debug step [tid]` / `debug next [tid]`

- **When to use**: to execute code line by line and observe each instruction. `step` enters function bodies; `next` skips function calls.
- **Steps**: `deeptrace_cli -p 1234 debug step`
- **Expected output**: `OK` (combine with `debug register rip` to see the current execution position change).

### 5.4 Software Breakpoints — `debug break <address>` / `debug clear <address>`

- **When to use**: to pause the program when it reaches an address.
- **Steps**:
  ```
  deeptrace_cli -p 1234 debug break 0x14000D000
  ```
- **Expected output** (real sample):
  ```
  breakpoint set at 0x000000014000D000 (orig 0x44)
  ```
  `orig` is the original byte at the address before the breakpoint replaced it (automatically restored when the breakpoint is cleared).
- **Clearing**: `deeptrace_cli -p 1234 debug clear 0x14000D000` → `OK`.

### 5.5 Hardware Breakpoints — `debug hbreak <address> [type] [length]`

- **When to use**: breakpoints that don't modify memory (types `0`=execute, `1`=write, `2`=read/write). Limited count (usually 4).
- **Steps**: `deeptrace_cli -p 1234 debug hbreak 0x14000D000 0 1`
- **Clearing**: `deeptrace_cli -p 1234 debug hclear <address>`.

### 5.6 Page Guard Breakpoints — `debug guard <address> <size>` / `debug unguard <address> <size>`

- **When to use**: to monitor access to a range of memory.
- **Steps**: `deeptrace_cli -p 1234 debug guard 0x14000D000 16`

### 5.7 Debug Status — `debug status`

- **When to use**: to confirm whether debugging is active and how many breakpoints exist.
- **Steps**: `deeptrace_cli -p 1234 debug status`
- **Expected output** (real sample):
  ```
  attached: yes
  pid: 26128
  breakpoints: 1
  hw_breakpoints: 0
  ```

### 5.8 Registers — `debug registers [tid]` / `debug register <name> [tid]`

- **When to use**: to view CPU registers (key debug information).
- **Steps**: `deeptrace_cli -p 1234 debug registers`
- **Expected output** (real sample, excerpt):
  ```
  REG      VALUE
  rax      0x0000000000000034
  ...
  rip      0x00007FFC98E606E4
  eflags   0x0000000000000246
  ```
- **View one register**: `deeptrace_cli -p 1234 debug register rip` → `rip = 0x00007FFC98E606E4`.

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
| `debug attach` / `detach` / `pause` / `resume` | Debug control |
| `debug step` / `next` | Single step |
| `debug break` / `clear` / `hbreak` / `hclear` / `guard` / `unguard` | Breakpoints |
| `debug status` / `registers` / `register <name>` | Debug status / registers |
| `disasm at <addr> [n]` / `disasm range <a> <b>` | Disassembly |
| `resolve base <mod>` / `resolve scan <pattern>` | Base address / pattern scan |
| `watch add/list/remove/refresh/clear` | Watches |
| `dll inject/eject/list/status` | DLL injection |
| `asm assemble <code> [--hex] [--c-array]` | Assembly |
| `shellcode inject/injectat/status` | Shellcode injection |

# User Documentation — Design Phase (v1.3.0)

> This file is the output of stage 2 of `.flow/user_docs_development_process.md`:
> 2.1 Information architecture design
> 2.2 Documentation structure design
> 2.3 Example design

---

## 2.1 Information Architecture Design

**Organization**: task-first, archived by tier (as recommended by the process).

| Tier | Documents | Organization | Description |
|------|-----------|--------------|-------------|
| L1 Getting started | README / GETTING_STARTED | by task (install → first use) | one straight line through to done |
| L2 Everyday use | USER_MANUAL | by feature/command group | one chapter per command group, task-style steps |
| L3 Reference | FAQ / TROUBLESHOOTING | by problem | frequent problems first |

**Not organized by code modules**: the documentation follows user tasks (view processes/read memory/set breakpoints), not source directories.

## 2.2 Documentation Structure Design

```
docs/users/v1.3.0/
├── README.md             # product intro (one line) + quick links
├── GETTING_STARTED.md    # quick start
│   ├── what you need (Windows x64, deeptrace_cli.exe)
│   ├── download & install (Release zip extraction / Debug build artifacts)
│   ├── first run (open a command window, -h/-v)
│   ├── first task: view processes (ps list)
│   ├── second task: read target process memory (find PID, -p, mem read/readval)
│   └── having trouble? (points to FAQ)
├── USER_MANUAL.md        # user manual (per command group)
│   ├── general: command format, -p/-h/-v, exit codes, address notation
│   ├── ps process: list / attach / detach / info / suspend / resume / kill
│   ├── mem memory: read / write / dump / regions / readval
│   ├── module module: list / find / base / exports / dump
│   ├── thread thread: list / suspend / resume / kill
│   ├── debug debugging: attach / detach / pause / resume / step / next / break / clear / hbreak / hclear / guard / unguard / status / registers / register
│   ├── disasm disassembly: at / range
│   ├── resolve resolution: base / scan
│   ├── watch watch: list / add / remove / refresh / clear
│   ├── dll injection: inject / eject / list / status
│   ├── asm assembly: assemble (--hex / --c-array)
│   └── shellcode: inject / injectat / status
├── FAQ.md                # FAQ (by frequency)
│   ├── why does attach report NoSuchProcess?
│   ├── why does reading memory fail / require administrator?
│   ├── why can't I find the process / address?
│   ├── why do breakpoints/watches persist across commands?
│   ├── why doesn't injection succeed?
│   └── can't remember the command arguments?
├── TROUBLESHOOTING.md    # troubleshooting
│   ├── error message reference (Error: xxx → meaning → what to do)
│   ├── known limitations (Windows x64 only, 64-bit targets, permissions)
│   └── verification method (practice with deeptrace_target.exe)
├── ANALYSIS.md           # analysis phase output
├── DESIGN.md             # this file
└── CHANGELOG.md          # change history
```

Cross-references: USER_MANUAL command chapters link to related chapters; FAQ problems point to the corresponding USER_MANUAL sections; GETTING_STARTED points to the FAQ.

## 2.3 Example Design (realistic scenarios running through the docs)

| Example | Scenario | Used in |
|---------|----------|---------|
| Example A | practice with the test target: launch deeptrace_target.exe, view the process, read its known values | GETTING_STARTED, start of USER_MANUAL |
| Example B | read/write a 4-byte value: mem read 0x14000D000 4 hex → 44 33 22 11; mem write to modify, then read back | USER_MANUAL memory chapter |
| Example C | set and inspect a breakpoint: debug break → debug status → debug clear | USER_MANUAL debugging chapter |
| Example D | AOB scan: resolve scan "DE AD BE EF" finds the g_bytes address | USER_MANUAL resolution chapter |
| Example E | assemble: asm assemble "nop; ret" → 90C3 | USER_MANUAL assembly chapter |

**Example authenticity**: the output of every example was captured from real runs; the output blocks in the docs are taken verbatim from real runs, not fabricated. The test target addresses (0x14000D000 etc.) come from deeptrace_target.exe's fixed base address (ASLR disabled), so the addresses are identical when you launch that target yourself.

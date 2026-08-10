# User Documentation — Analysis Phase (v1.3)

> This file is the output of stage 1 of `.flow/user_docs_development_process.md`:
> 1.1 User profiles
> 1.2 Use case analysis
> 1.3 Documentation tier planning

---

## 1.1 User Profiles

| Factor | Content |
|--------|---------|
| User identity | game modders / reverse engineers, software debuggers, security researchers, AI tool users |
| Skill level | can open a command prompt/PowerShell and type commands; **no programming assumed, no code-reading assumed** |
| Goals | view process lists, read/modify target process memory, view modules and exports, control threads, debug (breakpoints/single-step/registers), disassemble, assemble, AOB scan, watch variables, DLL/shellcode injection |
| Environment | Windows 10/11 x64 (the target process is also a Windows program) |
| Key understanding | users interact with the tool via **command-line arguments**; one command per run; breakpoint/watch/inject state persists across commands |

## 1.2 Use Case Analysis (described with real commands, not code terms)

```
Use case 1: View running processes on the system
  Run deeptrace_cli ps list → see a process table (name/PID/threads/parent PID)

Use case 2: Select a target process
  Find the target program's PID → point at it with the -p <PID> option → view process info with ps info

Use case 3: Read a value from the target process's memory
  Use -p <PID> plus mem read <address> → see hex bytes; mem readval <address> dword shows the value directly

Use case 4: Modify a value in the target process's memory
  mem write <address> <value> → returns OK; read again to confirm the value changed

Use case 5: View modules and exports
  module list lists loaded modules; module base <name> gives the module base address; module exports <module> lists exported functions

Use case 6: Scan memory for a pattern (AOB)
  resolve scan "48 8B ?? ?? 00" → find all matching addresses

Use case 7: Watch a variable change
  watch add <description> <address> <type> → watch refresh / watch list shows the live value

Use case 8: Set a breakpoint and check status
  debug break <address> → debug status shows the breakpoint count → debug clear <address> clears it

Use case 9: View CPU registers
  debug registers shows all registers; debug register rip shows just one

Use case 10: Disassemble a memory region
  disasm at <address> <count> → see address/machine code/assembly side by side

Use case 11: Turn assembly code into machine code
  asm assemble "nop; ret" → get 90C3

Use case 12: Inject a DLL or shellcode
  dll inject <dll path> → dll list shows runtime status; shellcode inject <hex> → shellcode status to check
```

> Note: this product is a **command-line tool**; the user flow is exactly the command input → output inspection described above. Every command that operates on a target process needs `-p <PID>` first.

## 1.3 Documentation Tier Planning

| Tier | Audience | Content | Documents |
|------|----------|---------|-----------|
| **L1 Getting started** | absolute beginners | download/install, how to open a command window, first process listing, first memory read | `GETTING_STARTED.md` |
| **L2 Everyday use** | users with some experience | how to use each command group (ps/mem/module/thread/debug/disasm/resolve/watch/dll/asm/shellcode) | `USER_MANUAL.md` |
| **L3 Reference** | advanced users | FAQ, error message reference, troubleshooting | `FAQ.md`, `TROUBLESHOOTING.md` |

Tiering principle: getting-started docs avoid advanced content (no AOB syntax, no breakpoint types); reference docs don't repeat getting-started steps (they point to the relevant sections).

# Example: Debug Session and Breakpoints

Demonstrates the complete debug session lifecycle: **attach → read registers → enter debug → set breakpoint → single step → clear breakpoint → exit debug → detach**. Source:
[src/debug_breakpoints.cpp](src/debug_breakpoints.cpp).

## API Call Order

| Step | API | Description |
|------|-----|-------------|
| 1 | `attach(pid)` | establish the process session |
| 2 | `registers_get` | read registers (no debug mode needed) |
| 3 | `debug_attach()` | enter debug mode (requires administrator) |
| 4 | `breakpoint_set` | set a software breakpoint (rewrites to 0xCC) |
| 5 | `debug_step` | single step |
| 6 | `breakpoint_clear` | restore the original byte |
| 7 | `debug_detach()` | exit debug mode (target keeps running) |
| 8 | `detach()` | close the process session |

## Code

```cpp
// Full code in src/debug_breakpoints.cpp; key excerpts:
deeptrace::attach(pid);                                        // 1. attach

std::vector<deeptrace::RegisterInfo> regs;
deeptrace::registers_get(regs, 0);                             // 2. registers

if (deeptrace::debug_attach() != deeptrace::Result::Ok) {      // 3. debug attach
    deeptrace::detach();
    return 1;   // requires administrator privileges
}

deeptrace::BreakpointInfo bp;
deeptrace::breakpoint_set(addr, bp);                           // 4. set breakpoint

uintptr_t rip = 0;
deeptrace::debug_step(0, &rip);                                // 5. single step

deeptrace::breakpoint_clear(addr);                             // 6. clear breakpoint
deeptrace::debug_detach();                                     // 7. exit debug
deeptrace::detach();                                           // 8. detach
```

## Build & Run

```bat
build_examples.bat
debug_breakpoints.exe 1234 0x140001000
```

## Tips

- **Always** clear breakpoints and call `debug_detach`/`detach` before exiting; otherwise the target process may be terminated because of a leftover INT3 or an unattached debugger.
- `debug_step` automatically performs a one-shot attach → step → detach flow even when not in debug mode.
- Hardware breakpoints (`hw_breakpoint_set`) do not modify target code; suitable for read-only code sections.

## Related APIs

- [debug_attach](../Modules/DEBUG.md#deeptracedebug_attach)
- [breakpoint_set](../Modules/DEBUG.md#deeptracebreakpoint_set)
- [debug_step](../Modules/DEBUG.md#deeptracedebug_step)
- [registers_get](../Modules/DEBUG.md#deeptraceregisters_get)

# Example: Pointer-Chain Reverse Scan

Demonstrates the two-phase pointer-chain workflow: **attach → pointer_map_snapshot → (re-locate value → pointer_map_rescan)**. Source: [src/pointer_chain_scan.cpp](src/pointer_chain_scan.cpp) (compilable and runnable directly).

This is the classic "pointer scan" use case: you find a value address (e.g. player health) with a pattern scan; that address moves every game restart, but the pointer chain leading to it stays structurally the same. Snapshot finds candidate chains now; rescan filters out coincidental ones after the value moves.

## API Call Order

| Step | API | Description |
|------|-----|-------------|
| 1 | `attach(pid)` | establish a session (prerequisite) |
| 2 | `pointer_map_snapshot(cfg, chains)` | reverse-walk from the value address, optional module anchor |
| 3 | `pointer_map_rescan(base, new_target, cfg, stable)` | keep chains that still resolve to the new address (after a restart) |
| 4 | `detach()` | close the session |

## Code

```cpp
// Full code in src/pointer_chain_scan.cpp; key excerpts:
deeptrace::PointerScanConfig cfg;
cfg.target = value_addr;      // value address found e.g. by pattern_scan
cfg.max_offset = 2048;        // pointer within target +/- 2048 counts
cfg.max_level = 5;            // chain depth
cfg.max_results = 10000;
cfg.module = "Game.exe";      // anchor: only chains rooted inside this module

std::vector<deeptrace::PointerChain> chains;
deeptrace::pointer_map_snapshot(cfg, chains);   // 2. snapshot
// chains[i] == { root, offsets[] }; evaluate:
//   addr = root; for off in offsets: addr = *(qword)addr + off

// 3. after a game restart: re-locate the value, then filter:
std::vector<deeptrace::PointerChain> stable;
deeptrace::pointer_map_rescan(chains, new_value_addr, cfg, stable);
// stable = chains that still reach new_value_addr within +/-max_offset
```

## Build & Run

```bat
build_examples.bat                      rem or directly:
cl /nologo /std:c++20 /EHsc /MDd /I deeptrace\include src\pointer_chain_scan.cpp ^
   deeptrace\out\lib\Debug\deeptrace.lib ^
   deeptrace\out\build\debug\third_party\keystone\lib\keystone.lib ^
   deeptrace\out\lib\Debug\capstone.lib /link /out:pointer_chain_scan.exe

pointer_chain_scan.exe 1234 0x1400D008 Game.exe
rem snapshot: 12 chain(s)
rem   root=0x00007FF64A1AF89C0 +38 +104 +8
```

> Note: attaching to other processes usually requires administrator privileges; the static link dependencies are described in [GettingStarted](../GettingStarted.md).

## Related APIs

- [pointer_map_snapshot](../Modules/POINTERSCAN.md#deeptracepointer_map_snapshot)
- [pointer_map_rescan](../Modules/POINTERSCAN.md#deeptracepointer_map_rescan)
- [pattern_scan](../Modules/RESOLVE.md#deeptracepattern_scan) — locating the initial value address
- [attach](../Modules/PROCESS.md#deeptraceattach)

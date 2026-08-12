# Module: Pointer-Chain Scan

Reverse pointer-chain discovery (v2.12.0): given the address of a value, find chains of pointers that lead to it — the classic "pointer scan" used to make cheat addresses stable across game restarts. Implemented as a thread-pool-accelerated memory sweep with a two-phase snapshot + rescan workflow that filters coincidence-based false positives.

## deeptrace::pointer_map_snapshot

### Syntax

```cpp
Result pointer_map_snapshot(const PointerScanConfig& cfg,
                            std::vector<PointerChain>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `cfg` | `const PointerScanConfig&` | scan configuration: `target` (value address), `max_offset`, `max_level`, `max_results`, `module` (anchor), `thread_count` |
| `out` | `std::vector<PointerChain>&` | output parameter, found chains (root + offset list), sorted by root address; may be empty |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | scan completed (possibly zero chains) |
| `Result::InvalidArg` | `target == 0` / `max_offset == 0` / `max_level == 0` / `out == nullptr` |
| `Result::NotAttached` | no attached session |
| `Result::NotFound` | `cfg.module` (anchor module) is set but not loaded in the target |
| `Result::AccessDenied` | no memory-read right over the scanned regions |

### Description

Reverse-walks from `cfg.target` (the address of a value found e.g. by `pattern_scan`). Level 1 finds every readable memory slot whose stored qword value lands within `target ± cfg.max_offset`; each deeper level treats the previous level's slot addresses as new targets, up to `cfg.max_level` hops. Only chains whose **outermost slot (root)** lies inside the anchor module (`cfg.module`, empty = any readable region) are emitted; output is capped at `cfg.max_results` (anti-false-positive). Offsets are **signed** (`int64_t`) — a pointer stored above its matched field yields a negative offset. Evaluation convention: `addr = root; for off in offsets: addr = *(qword)addr + off` (the CLI `mem batch` consumes chains in exactly this format). The sweep chunks readable committed regions (1 MiB) and scans them in parallel via an internal thread pool (`cfg.thread_count`, 0 = hardware concurrency).

Prerequisites: `attach(pid)` done; `cfg.target` points at a real value in the target. Postconditions: none (read-only scan).

### Example

```cpp
deeptrace::PointerScanConfig cfg;
cfg.target = 0x1400D008;        // value address found by pattern_scan
cfg.module = "Game.exe";        // anchor chains inside the game module
cfg.max_offset = 2048;
cfg.max_level = 5;
cfg.max_results = 10000;

std::vector<deeptrace::PointerChain> chains;
deeptrace::pointer_map_snapshot(cfg, chains);
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::pointer_map_rescan](#deeptracepointer_map_rescan)
- [deeptrace::pattern_scan](RESOLVE.md#deeptracepattern_scan)
- [Types/STRUCTS.md](../Types/STRUCTS.md#pointerscanconfig-pointer-chain-scan-configuration)

---

## deeptrace::pointer_map_rescan

### Syntax

```cpp
Result pointer_map_rescan(const std::vector<PointerChain>& base,
                          uintptr_t new_target,
                          const PointerScanConfig& cfg,
                          std::vector<PointerChain>& out);
```

### Parameters

| Parameter | Type | Description |
|-----------|------|-------------|
| `base` | `const std::vector<PointerChain>&` | chains previously returned by `pointer_map_snapshot` |
| `new_target` | `uintptr_t` | the new target value address (e.g. after the game restarted and addresses moved); must not be 0 |
| `cfg` | `const PointerScanConfig&` | reuse `max_offset` / `thread_count`; `target`/`module`/`max_level` are ignored |
| `out` | `std::vector<PointerChain>&` | output parameter, surviving chains, preserving `base` order; may be empty |

### Return Value

| Return value | Meaning |
|--------------|---------|
| `Result::Ok` | rescan completed (possibly zero surviving chains) |
| `Result::InvalidArg` | `new_target == 0` / `max_offset == 0` / `out == nullptr` |
| `Result::NotAttached` | no attached session |

### Description

Re-evaluates previously found chains against a **new** target address, keeping only chains whose current final address (after walking the full offset list against live memory) lands within `± cfg.max_offset` of `new_target`. This is the second phase of the two-phase workflow: after a game restart the old value address is gone, so you re-locate the value (e.g. re-run `pattern_scan`), then rescan the saved chains against the new address. Chains that survive are very likely **structurally real** (their offsets still resolve to the new value), while coincidental snapshot hits drop out — the standard false-positive filter. Chains are evaluated in parallel (`cfg.thread_count`) with per-level remote reads.

Prerequisites: `attach(pid)` done; `base` from a prior snapshot of the same process layout. Postconditions: none (read-only scan).

### Example

```cpp
// after game restart: re-locate the value, then filter the saved chains
std::vector<deeptrace::PointerChain> stable;
deeptrace::pointer_map_rescan(saved_chains, new_value_addr, cfg, stable);
// stable = chains that still reach the new value address
```

### Header

```cpp
#include "deeptrace.h"
```

### See Also

- [deeptrace::pointer_map_snapshot](#deeptracepointer_map_snapshot)

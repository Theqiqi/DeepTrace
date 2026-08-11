# Extension Guide (EXTENDING)

> Audience: contributors. Explains the extension points and provides complete examples (code + expected output).
> For function-level API details see the [API Documentation](../../api/v2.1.0/README.md); architecture conventions in [ARCHITECTURE.md](ARCHITECTURE.md); testing requirements in [TESTING.md](TESTING.md).

## 1. Extension Point Overview

| Extension type | Location | Files touched |
|----------------|----------|---------------|
| New CLI command | `cli/src/command/commands.cpp` + new/edit `cli/src/interface/cmd_*.cpp` | command table + executor function (+ e2e assertions) |
| New public API | `deeptrace/include/deeptrace.h` + `deeptrace/src/service/` | public header + service implementation (+ integration tests) |
| New algorithm | `deeptrace/src/algorithm/` | pure-computation module + unit tests |
| Engine replacement (assembly/disassembly) | `deeptrace/src/infrastructure/assembly|disassembly/` | infrastructure internal only, zero changes above |

All extensions must ship with tests (see [TESTING.md](TESTING.md)) and update the API documentation (`docs/api/v2.1.0/`) in sync.

---

## 2. Adding a New CLI Command (Complete Example)

Goal: add a `ps list2` command (equivalent to `ps list`, demonstrating how multiple branches are written in one action).

### 2.1 Command Table (cli/src/command/commands.cpp)

Append to the `// ---- ps ----` section of `command_table()`:

```cpp
cmd("ps", "list2", "ps list2", "List all processes (example)", {}),
```

`cmd(group, action, usage, brief, params)` builds one row of command specs; `req()`/`opt()` declare required/optional parameters, type enums are documented in the `ParamSpec::type` comment in commands.h.

### 2.2 Executor Function (cli/src/interface/cmd_process.cpp)

Add a branch in `cmd_ps`:

```cpp
if (req.action == "list2") {          // same implementation as list, shows multi-branch
    std::vector<deeptrace::ProcessInfo> procs;
    Result r = deeptrace::enumerate_processes(procs);
    if (r != Result::Ok) return internal::report_error(r, "");
    printer::print_processes(procs);
    return 0;
}
```

### 2.3 Expected Output

```bat
cli\out\bin\Debug\deeptrace_cli.exe ps list2
:: PID        NAME
:: 1234       deeptrace_target.exe
:: ...        (same as ps list)
```

### 2.4 Conventions & Checks

- `cmd_ps` is a `deeptrace_cli` namespace function declared in `interface/cmd.h` (dispatched by `req.action`).
- Parameters are read from `req.args[i]`; helpers like `internal::to_u32`/`to_u64` live in the `internal` namespace of `interface/executor.cpp`.
- Errors go through `internal::report_error(r, arg)` uniformly; success is printed via `printer::print_*`; direct printf of business results is forbidden.
- Exit codes: 0 success / 1 execution failure / 2 usage error.
- e2e assertions are mandatory (`check(...)` in `cli/test/e2e/test_cli_e2e.py`).

### 2.5 Debug Scripts: Adding a Script Step

Debug capabilities are not exposed as standalone commands anymore (see [ADR-11](DESIGN_DECISIONS.md#adr-11-why-the-cli-exposes-a-single-debug-run-entry-one-invocation--one-debug-session)); adding a debug capability means **adding a step to the script engine**:

1. **Step table** (`cli/src/interface/script.cpp`): declare the op name and its parameters (name / type / required / default) in the ops table, e.g.

   ```cpp
   {"my_op", {{"addr", "address", true, nullptr},
              {"size", "number-pos", false, "8"}}},
   ```

   Parameter types (`address`/`number`/`number-pos`/`hex-bytes`/`value-type`/`hw-type`/`hw-length`/`ascii`/`tid`/`index`/`format`) are validated by the same validator used for CLI parameters; unknown ops/fields/values are rejected at validation time (exit code 2).

2. **Executor** (`cli/src/interface/cmd_debug_run.cpp`): add a branch in the per-step dispatcher that calls the corresponding deeptrace public API and prints the step result (`[N] op ...` + result). All session-level resources (breakpoints/guards) must be tracked so the session cleanup path restores them before detach.

3. **Fixture + tests**: add a script fixture under `cli/test/scripts/` (POST_BUILD deploys it next to the test exe), then integration (`DebugRun*`) + e2e (`check(...)`) assertions; a negative fixture proves unknown ops are rejected.

---

## 3. Adding a New Public API (Layer-by-Layer Change Checklist)

Goal: the complete change surface for a new `deeptrace::foo_bar()`.

### 3.1 Public Header (deeptrace/include/deeptrace.h)

```cpp
// ---- foo ------------------------------------------------------------------
// Description: xxx. Prerequisite: attached. See the API docs for Result error codes.
Result foo_bar(uint32_t param, std::vector<uint32_t>& out);
```

### 3.2 service Implementation (deeptrace/src/service/foo.cpp, new file)

Follow existing conventions: each service has its own header (e.g. `service/process.h`); public APIs are exposed by `include/deeptrace.h`. First create `service/foo.h` declaring the internal signature, then implement:

```cpp
// service/foo.h (library-internal only, not in the public header)
#pragma once
#include "domain/types.h"
namespace deeptrace {
Result foo_bar(uint32_t param, std::vector<uint32_t>& out);
}  // namespace deeptrace
```

```cpp
// service/foo.cpp
#include "service/foo.h"
#include "domain/types.h"

namespace deeptrace {

Result foo_bar(uint32_t param, std::vector<uint32_t>& out) {
    if (param == 0) return Result::InvalidArg;          // parameter validation
    // compose: internal algorithms / infrastructure capabilities, no direct WinAPI
    return Result::Ok;
}

}  // namespace deeptrace
```

service public functions go in the `deeptrace` namespace; session-related helpers (session()/state_dir()) are in `deeptrace::internal`, referenced via `service/session.h`.

### 3.3 Build Registration (deeptrace/src/CMakeLists.txt)

Append `service/foo.h` + `service/foo.cpp` to the `add_library(deeptrace STATIC ...)` list.

### 3.4 Sync Checklist

- If new enums/structs are involved: append to `src/domain/types.h` **and sync** `include/domain/types.h` (the two files must be identical).
- Integration tests: add cases in `deeptrace/test/integration/` (real target).
- API docs: update `docs/api/v1.3.0/` (function signature/parameters/return values/behavior) and record in the CHANGELOG.
- CLI (optional): wrap it as a command per section 2.

---

## 4. Adding a New Algorithm (Pure Computation + Unit Tests)

### 4.1 Implementation (deeptrace/src/algorithm/foo.h/.cpp)

```cpp
// foo.h
#pragma once
#include <cstdint>
#include <vector>
namespace deeptrace::internal {
// Pure computation: no WinAPI, no I/O. Returns false for invalid input.
bool foo_transform(const std::vector<uint8_t>& in, std::vector<uint8_t>& out);
}
```

### 4.2 Registration (deeptrace/src/CMakeLists.txt)

Append `algorithm/foo.h` + `algorithm/foo.cpp` to the `add_library` list.

### 4.3 Unit Test (deeptrace/test/unit/foo_test.cpp)

```cpp
#include <gtest/gtest.h>
#include "algorithm/foo.h"

namespace deeptrace::internal {
TEST(FooTest, Transform) {
    std::vector<uint8_t> in = {1, 2, 3}, out;
    EXPECT_TRUE(foo_transform(in, out));
    ASSERT_EQ(out.size(), 3u);
}
}
```

Add to the `add_executable(deeptrace_unit_test ...)` list in `deeptrace/test/unit/CMakeLists.txt`. Algorithm-layer unit tests do not launch processes; they test pure functions only.

---

## 5. Engine Replacement (Assembly/Disassembly)

Precedent: `design/v1.2.0/deeptrace/00_CHANGELOG.md` (hand-written decoder → Capstone, hand-written encoder → Keystone).

Principles:
- Engine adapter files (`infrastructure/disassembly/disasm.{h,cpp}`, `infrastructure/assembly/asmenc.{h,cpp}`) expose only pure-function interfaces;
- When replacing the implementation, **keep the interface unchanged**; service/public APIs/CLI change zero lines;
- Engines ship as source under `deeptrace/third_party/`, CMake trims the backends; the static library does not merge dependencies, so the CLI must link explicitly;
- When output formats change, update the assertions in `test/unit/disasm_test.cpp` / `asm_test.cpp` and the API doc examples in sync;
- Pitfall: use the `cs_disasm` path uniformly in this environment (not `cs_disasm_iter` + stack structs, see ADR-05 in DESIGN_DECISIONS).

---

## 6. Testing Requirements (Must-Read for Extensions)

| Extension | Required companion tests |
|-----------|--------------------------|
| New command | parser unit tests (if parameters involved) + e2e assertions |
| New API | integration tests (real target) |
| New algorithm | unit tests (pure-function boundaries/conditions/groups) |
| Engine replacement | unit test assertion updates + full regression (Debug/Release) |

Full regression commands are in section 2.4 of [TESTING.md](TESTING.md).

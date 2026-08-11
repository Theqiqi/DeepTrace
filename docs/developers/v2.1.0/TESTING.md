# Testing Guide (TESTING)

> Audience: contributors. Explains how to run and write tests.
> Test framework: GoogleTest (gtest, pulled in via vcpkg manifest, test-only dependency).

## 1. Test System Overview

| Project | Level | Artifact | Content |
|---------|-------|----------|---------|
| deeptrace | unit | `deeptrace_unit_test.exe` | algorithm layer: hex / scan (AOB) / disasm / asm / format |
| deeptrace | integration | `deeptrace_integration_test.exe` | real target process, chains multiple public APIs |
| deeptrace | target | `deeptrace_target.exe` | test target program (ASLR disabled, known values at known addresses) |
| deeptrace | dll | `testdll.dll` | companion DLL for inject tests |
| cli | unit | `deeptrace_cli_unit_test.exe` | three layers: parser / printer / executor |
| cli | integration | `deeptrace_cli_integration_test.exe` | full chain: parse → execute → deeptrace API |
| cli | e2e | `test_cli_e2e.py` | launches real exes and asserts command-line behavior (independent of CMake) |

## 2. Running Tests

All test artifacts are produced after a Debug build. Test exes under `out/bin/Debug/` can be run directly (they auto-launch/clean up the target internally).

### 2.1 Unit Tests

```bat
deeptrace\out\bin\Debug\deeptrace_unit_test.exe
cli\out\bin\Debug\deeptrace_cli_unit_test.exe
```

### 2.2 Integration Tests

```bat
deeptrace\out\bin\Debug\deeptrace_integration_test.exe
cli\out\bin\Debug\deeptrace_cli_integration_test.exe
```

> Integration tests launch `deeptrace_target.exe` (a real child process) and perform process/memory/module/thread/debug/inject operations, then clean up automatically. Do not manually run the target and leave it occupying resources.

### 2.3 e2e Tests

```bash
python3 cli/test/e2e/test_cli_e2e.py
```

- Requires Debug build artifacts: `deeptrace_cli.exe` + `deeptrace_target.exe` + `testdll.dll`
  (testdll.dll is copied to `cli/out/bin/Debug/` automatically by the cli integration test's POST_BUILD step).
- Drives the real binaries and asserts stdout/exit codes (**104 checks** at v2.1.0, incl. `debug run` script-session and removed-command-rejection cases); exit code 0 if all pass, 1 if any fails.
- Runs directly under WSL too (the script calls exes via cmd.exe, converting paths automatically).

### 2.4 Debug Script Tests (fixtures)

`debug run` tests share real JSON script fixtures checked into the repository:

```
cli/test/scripts/
├── debug_session.json    # positive scripted session (status/registers/read/break/clear/watch_*)
├── debug_write.json      # spaced-hex write regression ("BE BA FE CA")
├── debug_bad.json        # negative: unknown op
└── debug_break_only.json # cleanup regression: break set but never cleared → byte restored on detach
```

- The cli integration test's POST_BUILD step copies them next to the test exe (same pattern as `testdll.dll`).
- Scripts use a `%G_INT%` placeholder for the runtime `g_int` address; tests substitute the real address because it is only known at runtime (the fixtures stay committable and the tests still run against a live target).
- Integration coverage: `DebugRunScriptedSession`, `DebugRunWriteSpacedHex`, `DebugRunScriptErrors`, `DebugRunBreakOnlyCleansUp` + removed-command rejection (`DebugSingleCommandsRejected` — exit 2 and the target stays intact).

### 2.5 Full Regression

```bash
deeptrace/out/bin/Debug/deeptrace_unit_test.exe
deeptrace/out/bin/Debug/deeptrace_integration_test.exe
cli/out/bin/Debug/deeptrace_cli_unit_test.exe
cli/out/bin/Debug/deeptrace_cli_integration_test.exe
python3 cli/test/e2e/test_cli_e2e.py
```

Current matrix at v2.1.0 (Debug/Release both green): deeptrace **96 unit + 34 integration**, deeptrace_cli **99 unit + 24 integration**, e2e **104 checks**.

## 3. Test Target Program (deeptrace_target.exe)

Each test tree (deeptrace / cli) has its own target with the same role:

- **Does not link deeptrace**; it is a standalone executable.
- **ASLR disabled** (`/DYNAMICBASE:NO /HIGHENTROPYVA:NO`) so the module base and global variable addresses are deterministic and tests can assert known values at known addresses.
- Prints a banner: `PID: <number>` line + global variable address table (`g_int`/`g_bytes` etc., `@0x...` format).
- Provides thread (`WORKER_TID:`), memory values (e.g. `g_int` holds `0x11223344`), and other test anchors.

When adding integration tests, prefer adding known-value anchors to the target rather than guessing addresses in the test.

## 4. Writing New Tests

### 4.1 Unit Test Template

```cpp
// cli/test/unit/parser_test.cpp style
#include <gtest/gtest.h>
#include "command/parser.h"

namespace deeptrace_cli {
namespace {

TEST(ParserTest, ScenarioDescription) {
    const char* argv[] = {"deeptrace_cli", "ps", "list"};
    ParseResult pr = parse_args(3, const_cast<char**>(argv));
    EXPECT_TRUE(pr.ok);
    EXPECT_EQ(pr.req.group, "ps");
}

}  // namespace
}  // namespace deeptrace_cli
```

- Add new test files to the `add_executable` list in the corresponding `test/unit/CMakeLists.txt`.
- Unit tests **must not launch real processes**; they only test pure logic (parsing/formatting/algorithms).

### 4.2 Integration Test Template

```cpp
// deeptrace/test/integration/target_util.h provides:
//   launch_target() -> pid / address anchors / handle
#include <gtest/gtest.h>
#include "deeptrace.h"
#include "target_util.h"

TEST(ProcessIntegrationTest, ScenarioDescription) {
    Target t = launch_target();
    std::vector<deeptrace::ProcessInfo> list;
    ASSERT_EQ(deeptrace::enumerate_processes(list), deeptrace::Result::Ok);
    // assert that list contains t.pid
}
```

- Integration tests launch a real target and must clean up when finished (terminate the target).
- For breakpoint/watch/inject cases, watch out for state file cleanup under `%TEMP%/deeptrace_<pid>/`.

### 4.3 e2e Tests

Add assertions in `cli/test/e2e/test_cli_e2e.py` following the `check(name, cond, detail)` pattern:

```python
code, out, _ = run_cli(["-p", str(pid), "mem", "read", g_int, "4", "hex"])
check("mem read exit 0", code == 0)
```

New commands/parameters must add e2e assertions (command-line behavior is part of the product contract).

## 5. Testing Requirements

- New features must ship with tests (unit + integration or e2e), otherwise they are not merged.
- Modifying algorithms/engines (e.g. disassembly format) requires updating unit test assertions in sync.
- Tests must not depend on the network, fixed PIDs, or system-specific processes.

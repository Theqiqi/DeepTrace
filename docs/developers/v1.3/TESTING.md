# 测试指南(TESTING)

> 目标读者:贡献者。说明如何运行与编写测试。
> 测试框架:GoogleTest(gtest,经 vcpkg manifest 引入,仅测试依赖)。

## 1. 测试体系总览

| 项目 | 层级 | 产物 | 内容 |
|------|------|------|------|
| deeptrace | unit | `deeptrace_unit_test.exe` | 算法层:hex / scan(AOB)/ disasm / asm / format |
| deeptrace | integration | `deeptrace_integration_test.exe` | 真实 target 进程,串联多个公共 API |
| deeptrace | target | `deeptrace_target.exe` | 测试目标程序(关闭 ASLR,已知地址放已知值) |
| deeptrace | dll | `testdll.dll` | 注入测试的伴生 DLL |
| cli | unit | `deeptrace_cli_unit_test.exe` | 三层:parser / printer / executor |
| cli | integration | `deeptrace_cli_integration_test.exe` | parse → execute → deeptrace API 全链路 |
| cli | e2e | `test_cli_e2e.py` | 启动真实 exe 断言命令行行为(独立于 CMake) |

## 2. 运行测试

所有测试产物在 Debug 构建后生成。`out/bin/Debug/` 下的测试 exe 直接运行即可(内部自动启动/清理 target)。

### 2.1 单元测试

```bat
deeptrace\out\bin\Debug\deeptrace_unit_test.exe
cli\out\bin\Debug\deeptrace_cli_unit_test.exe
```

### 2.2 集成测试

```bat
deeptrace\out\bin\Debug\deeptrace_integration_test.exe
cli\out\bin\Debug\deeptrace_cli_integration_test.exe
```

> 集成测试会启动 `deeptrace_target.exe`(真实子进程)并执行进程/内存/模块/线程/调试/注入操作,结束后自动清理。请勿手动运行 target 后重复占用。

### 2.3 e2e 测试

```bash
python3 cli/test/e2e/test_cli_e2e.py
```

- 需要 Debug 构建产物:`deeptrace_cli.exe` + `deeptrace_target.exe` + `testdll.dll`
  (testdll.dll 由 cli 集成测试的 POST_BUILD 步骤自动复制到 `cli/out/bin/Debug/`)。
- 驱动真实二进制,断言 stdout/退出码(47 项检查);全部通过退出码 0,任一失败退出码 1。
- WSL 下同样直接运行(脚本内部经 cmd.exe 调 exe,路径自动转换)。

### 2.4 回归全量

```bash
deeptrace/out/bin/Debug/deeptrace_unit_test.exe
deeptrace/out/bin/Debug/deeptrace_integration_test.exe
cli/out/bin/Debug/deeptrace_cli_unit_test.exe
cli/out/bin/Debug/deeptrace_cli_integration_test.exe
python3 cli/test/e2e/test_cli_e2e.py
```

## 3. 测试目标程序(deeptrace_target.exe)

两个测试树(deeptrace / cli)各有一个 target,作用相同:

- **不链接 deeptrace**,是一个独立可执行程序。
- **关闭 ASLR**(`/DYNAMICBASE:NO /HIGHENTROPYVA:NO`),保证模块基址与全局变量地址确定,测试可断言已知地址上的已知值。
- 输出 banner:`PID: <number>` 行 + 全局变量地址表(`g_int`/`g_bytes` 等,`@0x...` 格式)。
- 提供线程(输出 `WORKER_TID:`)、内存值(如 `g_int` 存 `0x11223344`)等测试锚点。

新增集成测试时,优先在 target 中增加已知值锚点,而不是在测试里猜测地址。

## 4. 编写新测试

### 4.1 单元测试模板

```cpp
// cli/test/unit/parser_test.cpp 风格
#include <gtest/gtest.h>
#include "command/parser.h"

namespace deeptrace_cli {
namespace {

TEST(ParserTest, 场景描述) {
    const char* argv[] = {"deeptrace_cli", "ps", "list"};
    ParseResult pr = parse_args(3, const_cast<char**>(argv));
    EXPECT_TRUE(pr.ok);
    EXPECT_EQ(pr.req.group, "ps");
}

}  // namespace
}  // namespace deeptrace_cli
```

- 新测试文件加入对应 `test/unit/CMakeLists.txt` 的 `add_executable` 列表。
- 单元测试**不得启动真实进程**,只测纯逻辑(解析/格式化/算法)。

### 4.2 集成测试模板

```cpp
// deeptrace/test/integration/target_util.h 提供:
//   launch_target() -> pid / 地址锚点 / 句柄
#include <gtest/gtest.h>
#include "deeptrace.h"
#include "target_util.h"

TEST(ProcessIntegrationTest, 场景描述) {
    Target t = launch_target();
    std::vector<deeptrace::ProcessInfo> list;
    ASSERT_EQ(deeptrace::enumerate_processes(list), deeptrace::Result::Ok);
    // 断言 list 包含 t.pid
}
```

- 集成测试启动真实 target,结束后必须清理(终止 target)。
- 断点/watch/注入类用例注意 `%TEMP%/deeptrace_<pid>/` 状态文件的清理。

### 4.3 e2e 测试

在 `cli/test/e2e/test_cli_e2e.py` 中按 `check(name, cond, detail)` 模式追加断言:

```python
code, out, _ = run_cli(["-p", str(pid), "mem", "read", g_int, "4", "hex"])
check("mem read exit 0", code == 0)
```

新增命令/参数必须补 e2e 断言(命令行行为是产品契约的一部分)。

## 5. 测试要求

- 新增功能必须有配套测试(单元 + 集成或 e2e),否则不予合入。
- 修改算法/引擎(如反汇编格式)必须同步更新单元测试断言。
- 测试不依赖网络、不依赖固定 PID、不依赖系统特定进程。

# 扩展开发指南(EXTENDING)

> 目标读者:贡献者。说明扩展点并给出完整示例(代码 + 预期输出)。
> 函数级 API 说明见 [API 文档](../../api/v1.3/README.md);架构约定见 [ARCHITECTURE.md](ARCHITECTURE.md);测试要求见 [TESTING.md](TESTING.md)。

## 1. 扩展点总览

| 扩展类型 | 位置 | 改动面 |
|---------|------|--------|
| 新增 CLI 命令 | `cli/src/command/commands.cpp` + 新增/修改 `cli/src/interface/cmd_*.cpp` | 命令表 + 执行函数(+ e2e 断言) |
| 新增公共 API | `deeptrace/include/deeptrace.h` + `deeptrace/src/service/` | 公共头 + service 实现(+ 集成测试) |
| 新增算法 | `deeptrace/src/algorithm/` | 纯计算模块 + 单元测试 |
| 替换引擎(汇编/反汇编) | `deeptrace/src/infrastructure/assembly|disassembly/` | 仅基础设施内部,上层零改动 |

所有扩展都必须配套测试(见 [TESTING.md](TESTING.md)),并同步更新 API 文档(`docs/api/v1.3/`)。

---

## 2. 新增 CLI 命令(完整示例)

目标:新增 `ps list2` 命令(与 `ps list` 等价,仅展示一个动作里多个分支的写法)。

### 2.1 命令表(cli/src/command/commands.cpp)

在 `command_table()` 的 `// ---- ps ----` 段追加:

```cpp
cmd("ps", "list2", "ps list2", "List all processes (example)", {}),
```

`cmd(group, action, usage, brief, params)` 构造一行命令规格;`req()`/`opt()` 声明必填/可选参数,类型枚举见 `commands.h` 的 `ParamSpec::type` 注释。

### 2.2 执行函数(cli/src/interface/cmd_process.cpp)

在 `cmd_ps` 中追加分支:

```cpp
if (req.action == "list2") {          // 与 list 相同的实现,展示多分支
    std::vector<deeptrace::ProcessInfo> procs;
    Result r = deeptrace::enumerate_processes(procs);
    if (r != Result::Ok) return internal::report_error(r, "");
    printer::print_processes(procs);
    return 0;
}
```

### 2.3 预期输出

```bat
cli\out\bin\Debug\deeptrace_cli.exe ps list2
:: PID        NAME
:: 1234       deeptrace_target.exe
:: ...        (与 ps list 一致)
```

### 2.4 约定与检查

- `cmd_ps` 是 `deeptrace_cli` 命名空间函数,声明在 `interface/cmd.h`(按 `req.action` 分派)。
- 参数从 `req.args[i]` 取,`internal::to_u32`/`to_u64` 等辅助在 `interface/executor.cpp` 的 `internal` 命名空间。
- 错误统一 `internal::report_error(r, arg)`,成功经 `printer::print_*` 输出;禁止直接 printf 业务结果。
- 退出码:0 成功 / 1 执行失败 / 2 用法错误。
- 必须补 e2e 断言(`cli/test/e2e/test_cli_e2e.py` 的 `check(...)`)。

---

## 3. 新增公共 API(分层改动清单)

目标:新增 `deeptrace::foo_bar()` 的完整改动面。

### 3.1 公共头(deeptrace/include/deeptrace.h)

```cpp
// ---- foo ------------------------------------------------------------------
// 说明:xxx。前置:已 attach。返回 Result 错误码见 API 文档。
Result foo_bar(uint32_t param, std::vector<uint32_t>& out);
```

### 3.2 service 实现(deeptrace/src/service/foo.cpp,新文件)

```cpp
#include "deeptrace.h"          // 或 service/foo.h 声明
#include "domain/types.h"

namespace deeptrace {

Result foo_bar(uint32_t param, std::vector<uint32_t>& out) {
    if (param == 0) return Result::InvalidArg;          // 参数校验
    // 组装:internal 算法 / 基础设施能力,禁止直接 WinAPI
    return Result::Ok;
}

}  // namespace deeptrace
```

service 公共函数放 `deeptrace` 命名空间;会话相关辅助(session()/state_dir())在 `deeptrace::internal`,经 `service/session.h` 引用。

### 3.3 注册构建(deeptrace/src/CMakeLists.txt)

在 `add_library(deeptrace STATIC ...)` 列表追加 `service/foo.h` + `service/foo.cpp`。

### 3.4 同步事项

- 若涉及新枚举/结构体:追加到 `src/domain/types.h` **并同步** `include/domain/types.h`(两个文件必须一致)。
- 集成测试:在 `deeptrace/test/integration/` 新增用例(真实 target)。
- API 文档:更新 `docs/api/v1.3/`(函数签名/参数/返回值/行为),并记录 CHANGELOG。
- CLI(可选):按第 2 节包装为命令。

---

## 4. 新增算法(纯计算 + 单元测试)

### 4.1 实现(deeptrace/src/algorithm/foo.h/.cpp)

```cpp
// foo.h
#pragma once
#include <cstdint>
#include <vector>
namespace deeptrace::internal {
// 纯计算:无 WinAPI、无 I/O。返回 false 表示输入非法。
bool foo_transform(const std::vector<uint8_t>& in, std::vector<uint8_t>& out);
}
```

### 4.2 注册(deeptrace/src/CMakeLists.txt)

在 `add_library` 列表追加 `algorithm/foo.h` + `algorithm/foo.cpp`。

### 4.3 单元测试(deeptrace/test/unit/foo_test.cpp)

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

加入 `deeptrace/test/unit/CMakeLists.txt` 的 `add_executable(deeptrace_unit_test ...)` 列表。算法层单测不启动进程,只测纯函数。

---

## 5. 替换引擎(汇编/反汇编)

先例:`design/v1.2/deeptrace/00_CHANGELOG.md`(自研解码器 → Capstone、自研编码器 → Keystone)。

原则:
- 引擎适配文件(`infrastructure/disassembly/disasm.{h,cpp}`、`infrastructure/assembly/asmenc.{h,cpp}`)只暴露纯函数接口;
- 替换实现时**保持接口不变**,service/公共 API/CLI 零改动;
- 引擎以源码形式放在 `deeptrace/third_party/`,CMake 裁剪后端;静态库不合并依赖,CLI 需显式链接;
- 输出格式变化时同步更新 `test/unit/disasm_test.cpp` / `asm_test.cpp` 断言与 API 文档示例;
- 踩坑:本环境统一用 `cs_disasm` 路径(不用 `cs_disasm_iter` + 栈结构体,见 DESIGN_DECISIONS ADR-05)。

---

## 6. 测试要求(扩展必读)

| 扩展 | 必须配套 |
|------|---------|
| 新命令 | parser 单测(如涉及参数)+ e2e 断言 |
| 新 API | 集成测试(真实 target) |
| 新算法 | 单元测试(纯函数边界/条件/分组) |
| 引擎替换 | 单元测试断言更新 + 全量回归(Debug/Release) |

全量回归命令见 [TESTING.md](TESTING.md) 第 2.4 节。

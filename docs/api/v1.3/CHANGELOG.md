# API 文档变更记录

## v1.3(2026-08-09)

首版 API 参考文档,对应代码 tag `v1.3`(项目更名 DeepTrace 后首次发布)。

**新增**
- 完整 API 清单:**55 个公共函数**、**3 个枚举**(`Result`/`ValueType`/`BreakpointType`)、**11 个结构体**,零遗漏。
- 文档结构:
  - `README.md` — API 概述、分组总览、调用前置条件(会话生命周期/权限/持久化/线程安全)
  - `GettingStarted.md` — 从零到一的完整示例(可直接编译运行)
  - `Modules/` — 10 个模块文档(进程与会话/内存/模块/线程/调试/反汇编/汇编/解析/监视/注入)
  - `Types/` — `RESULT.md`(14 个错误码触发条件)、`ENUMS.md`、`STRUCTS.md`
  - `Examples/` — 3 个完整示例 + 可编译源码 + `build_examples.bat`
- 示例验证:3 个示例基于 MSVC C++20 链接 `deeptrace.lib` 全部编译通过。

**约定**
- 每个函数文档包含:语法(与 `deeptrace.h` 完全一致)、参数表、返回值表、
  行为说明、前置/后置条件、示例、头文件、参见。
- 文档随代码版本归档于 `docs/api/v<版本>/`,后续版本在对应目录增量更新。

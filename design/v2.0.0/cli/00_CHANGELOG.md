# deeptrace_cli - 版本变更记录

## v2.0.0 脚本驱动的一次性调试会话(重大重构,相对 v1.4.1)

> 大版本重构:CLI 从「单命令无状态入口 + 调试会话状态依赖跨调用/持久化文件」调整为
> **「一次调试脚本 = 一次完整的调试会话,会话状态仅存在于本次调用(内存中)」**。
> 动态调试与动态/静态分析解耦:动态调试收敛为 `debug run <script>` 单一入口,
> 其余分析命令(内存/反汇编/监视/扫描等)保持单命令形态、无状态不变。
> 静态库(deeptrace)同步 2.0.0,新增 `debug_continue` 能力(见 deeptrace/00_CHANGELOG)。

### 1. 改动点清单

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | 新增 `debug run <script>`:一次调用 = 一次完整调试会话(attach → debug_attach → 逐条步骤 → 清理 → detach),会话状态仅在内存中 | 新增 | debug/run |
| 2 | 调试脚本 = JSON 步骤数组,步骤覆盖调试+内存+反汇编+监视 | 新增 | 脚本格式 |
| 3 | 脚本执行器:会话内逐条执行步骤,首次失败即停,统一清理本次会话设置(软/硬断点、守护页)后 detach | 新增 | interface |
| 4 | 脚本解析与校验(JSON 子集 + 步骤表:操作/必填字段/字段取值) | 新增 | interface/script |
| 5 | 静态库能力缺口:运行到断点(continue)→ 触发静态库流程新增 `debug_continue` | 新增(库能力) | deeptrace |
| 6 | 版本号统一 2.0.0(help/version/CMake/测试断言) | 修改 | 全局 |
| 7 | 既有全部命令保留,向后兼容;debug 组新增 run 动作 | 修改 | debug 组 |

### 2. 能力边界(声明支持 / 不支持)

- `debug run` 支持:JSON 步骤数组(操作、全部字段值为字符串);步骤操作:
  break/clear/hbreak/hclear/guard/unguard/pause/resume/step/next/registers/register/
  status/read/write/disasm/watch_list/watch_add/watch_remove/watch_refresh/watch_clear/
  continue。
- 不支持:JSON 注释/多行字符串/嵌套对象(仅字符串值);脚本内流程控制(条件/循环/
  变量);断点命中回调;会话跨调用持久化。以上不支持项返回明确 `Error:` 与退出码。
- 退出码:0 全部步骤成功;1 步骤运行时失败(会话已清理);2 脚本文件/格式/校验错误。
- 兼容性:既有全部命令行为不变;`debug run` 需要 `-p <pid>`。

### 3. 变更范围

| 位置 | 变更 |
|------|------|
| cli/src/command/commands.cpp | debug 组新增 run 命令表项;帮助文本版本 v2.0.0 |
| cli/src/interface/script.h/cpp | 新增:JSON 子集解析 + 步骤校验 |
| cli/src/interface/cmd_debug_run.cpp | 新增:会话执行器(逐条步骤、清理、continue 输出) |
| cli/src/interface/cmd_debug.cpp | run 动作分派 |
| cli/src/interface/cmd.h | cmd_debug_run 声明 |
| cli/src/printing/printer.cpp | version v2.0.0 |
| cli/CMakeLists.txt | 工程版本 2.0.0 |
| cli/src/CMakeLists.txt | core 库新增 script.cpp、cmd_debug_run.cpp |
| cli/test/unit/script_test.cpp | 新增:脚本解析/校验用例 |
| cli/test/unit/parser_test.cpp | debug run 解析用例 |
| cli/test/integration/cli_integration_test.cpp | debug run 真实会话用例 |
| cli/test/e2e/test_cli_e2e.py | e2e:脚本会话用例 + 版本号断言更新 |
| cli/test/e2e/cases.md | 用例清单更新 |

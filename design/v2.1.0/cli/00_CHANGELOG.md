# deeptrace_cli - 版本变更记录

## v2.1.0 debug 收敛为单一入口(相对 v2.0.0)

> 实测结论驱动(v2.0.0 遗留问题):debug 单命令(step/break/registers/attach 等)
> 在「一次调用 = 一个操作 + 自动 attach/detach」的无状态模式下语义错误:
>
> - `debug step` 两次执行 rip 完全相同 → **假单步**(无调试会话,未真正执行);
> - `debug break` 单独执行后目标内存残留 0xCC,无会话清理 → **污染目标进程**,
>   若打在 worker 循环体上,目标会因无人处理 INT3 而崩溃;
> - `debug registers`/`debug status` 读的是非暂停上下文/仅 OpenProcess 句柄 → **语义误导**;
> - `debug attach` 立即 attach 又 detach,中间无操作 → **无意义**。
>
> 结论:动态调试强依赖调试会话(有状态),无状态 CLI 只应暴露**一个入口
> `debug run <script>`**——一次调用 = 一次完整调试会话(attach → debug_attach →
> 步骤 → 清理 → detach),会话状态全程在内存中。其余调试操作全部收敛到脚本
> 步骤中执行,脚本步骤表**全量覆盖静态库全部调试能力**。

### 1. 改动点清单

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | debug 命令组只保留 `debug run` 一个入口;其余 15 个单命令(attach/detach/pause/resume/step/next/break/clear/hbreak/hclear/guard/unguard/status/registers/register)从命令表删除,调用报「unknown command」 | 删除 | command/commands.cpp |
| 2 | cmd_debug.cpp 删除全部单命令动作分派,仅保留 run 转发 | 删除 | interface/cmd_debug.cpp |
| 3 | 脚本步骤表全量覆盖调试能力:break/clear/hbreak/hclear/guard/unguard/pause/resume/step/next/registers/register/status/continue + read/write/disasm/watch_* | 修改(补齐校验) | interface/script.cpp |
| 4 | 会话执行器语义不变(一次调用 = 一次会话,清理保证) | 不变 | interface/cmd_debug_run.cpp |
| 5 | 版本号 2.1.0(help/version/CMake/测试断言) | 修改 | 全局 |
| 6 | 测试:e2e/集成移除 debug 单命令用例,保留并回归 debug run 脚本会话用例 | 修改 | test/ |

### 2. 能力边界(声明支持 / 不支持)

- **调试入口**:仅 `deeptrace_cli -p <pid> debug run <script.json>`。
- **脚本步骤支持**(全量覆盖静态库调试 API):
  - 断点:`break`/`clear`(软件断点)、`hbreak`/`hclear`(硬件断点)、`guard`/`unguard`(页守护);
  - 执行控制:`pause`/`resume`/`step`/`next`/`continue`(运行到断点/退出/超时);
  - 状态查询:`status`/`registers`/`register`;
  - 配合调试的读改写:`read`/`write`/`disasm`/`watch_*`(内存/反汇编/监视)。
- **不支持**:debug 单命令交互(step/break/registers/attach 等命令行直接调用——
  调用即报 `unknown command`,引导使用 `debug run`);会话跨调用持久化。
- 退出码:0 全部步骤成功;1 步骤运行时失败(会话已清理);2 脚本文件/格式/校验错误。
- 兼容性:既有非 debug 命令全部不变;`debug run` 需要 `-p <pid>`。

### 3. 变更范围

| 位置 | 变更 |
|------|------|
| design/v2.1.0/cli/ | 受影响文档复制并标注改动(07/10/12/13/14/15/16/18) |
| cli/src/command/commands.cpp | debug 组仅保留 run 命令表项;帮助文本版本 v2.1.0 |
| cli/src/interface/cmd_debug.cpp | 删除单命令分派,仅转发 run |
| cli/src/interface/script.cpp | 步骤表全量覆盖调试能力(校验表确认) |
| cli/src/printing/printer.cpp | version v2.1.0 |
| cli/CMakeLists.txt | 工程版本 2.1.0 |
| cli/test/unit/parser_test.cpp | 移除 debug 单命令解析用例,保留 DebugRunScriptPath |
| cli/test/unit/script_test.cpp | 保持(步骤表即调试能力覆盖) |
| cli/test/integration/cli_integration_test.cpp | 移除单命令用例,保留/回归 DebugRun* |
| cli/test/e2e/test_cli_e2e.py + cases.md | 移除单命令用例,保留 debug run 用例 |

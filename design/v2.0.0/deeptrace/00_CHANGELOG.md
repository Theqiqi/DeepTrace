# deeptrace - 版本变更记录

## v2.0.0 新增 debug_continue(运行到断点)(相对 v1.3.0)

> 由 CLI v2.0.0 流程通过「静态库流程交接记录」触发(CLI 需求:脚本驱动的一次性
> 调试会话需要"运行到断点"能力)。静态库完成契约回交 CLI 后由 CLI 接口调用层消费。

### 1. 改动点清单

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | 新增公共 API `Result debug_continue(uint32_t timeout_ms, ContinueInfo& out)` | 新增 | service/debug |
| 2 | 新增公共数据结构 `ContinueInfo{hit, exited, exit_code, exception_code, address, rip, tid}` | 新增 | domain/types |
| 3 | 基础设施新增 `DebugContinue`(等待调试事件:软件断点命中/其他异常/进程退出/超时) | 新增 | infrastructure/debug |
| 4 | 版本号 1.3.0 → 2.0.0 | 修改 | 全局 |

### 2. 能力边界(声明支持 / 不支持)

- 支持:调试模式下运行目标直至——①命中已设置软件断点(自动恢复原字节、
  单步执行断点指令、重新武装 INT3,报告断点地址与执行后 RIP,目标保持暂停);
  ②其他异常(硬件断点/守护页等,报告异常码与地址,不消费,目标保持暂停);
  ③目标进程退出(报告退出码);④超时(报告 timeout)。超时与命中均为 Ok。
- 不支持:非调试模式下调用(返回 NotAttached);断点命中的条件过滤;
  单步类异常的自动消费(仅软件断点被消费)。
- 异常处理:失败路径返回 AccessDenied/Error/Timeout(WaitForDebugEvent 失败)。

### 3. 变更范围

| 位置 | 变更 |
|------|------|
| DeepTrace/include/deeptrace.h | debug_continue 声明 |
| DeepTrace/include/domain/types.h + src/domain/types.h | ContinueInfo(同步) |
| DeepTrace/src/infrastructure/debug/debug.h/cpp | DebugContinue |
| DeepTrace/src/service/debug.h/cpp | debug_continue |
| DeepTrace/CMakeLists.txt | 工程版本 2.0.0 |
| DeepTrace/test/integration/process_integration_test.cpp | continue 命中/超时用例 |
| docs/api/v2.0.0/ | DEBUG.md 更新、STRUCTS.md 增加 ContinueInfo、CHANGELOG |

既有公共 API 全部保留,向后兼容。

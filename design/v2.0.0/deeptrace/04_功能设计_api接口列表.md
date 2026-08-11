# deeptrace - 功能设计 / API 接口列表(修改模式)

> 引用:v1.2.0/deeptrace/07_功能设计_api接口列表.md 既有 API 表。本版本新增:

## 1. 新增公共 API

```cpp
struct ContinueInfo {
    bool hit = false;             // 停在异常上(断点/守护页/其他)
    bool exited = false;          // 目标进程退出
    uint32_t exit_code = 0;       // 退出码(exited 时有效)
    uint32_t exception = 0;       // 异常码(hit 时有效)
    uintptr_t address = 0;        // 异常地址(hit 时有效)
    uintptr_t rip = 0;            // 命中线程 RIP(hit 时有效;软件断点=执行后 RIP)
    uint32_t tid = 0;             // 命中线程 ID(hit 时有效)
};

Result debug_continue(uint32_t timeout_ms, ContinueInfo& out);
```

## 2. 基础设施原语(内部)

```cpp
// 等待下一个调试事件;返回的异常事件不继续(目标冻结其上)。
Result DebugWaitEvent(uint32_t pid, uint32_t timeout_ms, ContinueInfo& out);
// 消费挂起的软件断点:恢复原字节 → TF 单步执行断点指令 → 重新武装 INT3。
Result DebugConsumeBreakpoint(uint32_t pid, uint32_t tid, uintptr_t addr,
                              uint8_t orig, uintptr_t* out_rip);
```

## 3. 行为说明

- debug_continue 要求 debug_mode;timeout_ms>0。
- 软件断点命中:Service 层依据持久化断点记录判定并调用 DebugConsumeBreakpoint。
- 其他异常:直接报告,不消费(目标保持冻结于异常)。
- 进程退出:报告退出码(EXIT_PROCESS_DEBUG_EVENT)。
- 超时:WaitForDebugEvent 超时 → 报告 timeout(Ok)。

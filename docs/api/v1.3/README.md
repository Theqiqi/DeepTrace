# deeptrace 静态库 API 参考(v1.3)

> 本文档是 **deeptrace 静态库**(进程内存操作/调试工具库)面向调用者的 API 参考。
> 调用者包括:CLI(deeptrace_cli)、其他开发者、AI 工具与集成方。
> 对应代码版本:**v1.3**(git tag),公共头文件:`deeptrace/include/deeptrace.h`。

## 1. 概述

deeptrace 是一个 Windows x64 平台上的进程操作静态库,提供进程枚举/附加、远程内存读写、
模块查询、线程操作、调试(断点/单步/寄存器)、反汇编、汇编、特征码扫描、监视(Watch)、
DLL/Shellcode 注入等能力。全部能力通过**单一公共头文件**暴露,仅使用标准 C++ 类型
(不暴露 `windows.h` 类型),便于集成与跨语言封装。

- 语言标准:C++20
- 平台:Windows x64(依赖 WinAPI,不支持 Linux/macOS)
- 头文件:`#include "deeptrace.h"`(位于 `deeptrace/include/`)
- 链接:`deeptrace/out/lib/<Debug|Release>/deeptrace.lib`(Debug=/MDd、Release=/MT 静态运行时)
- 依赖:汇编/反汇编能力依赖 Keystone、Capstone,静态库**不合并依赖**,消费方需额外
  链接 `keystone.lib` 与 `capstone.lib`(链接命令见 [GettingStarted](GettingStarted.md));
  调试/注入等能力依赖系统调试 API(需管理员权限)

## 2. API 分组总览

共 **55 个公共函数**、**3 个枚举**、**11 个结构体**。

| 模块 | 文档 | 函数 | 数量 |
|------|------|------|------|
| 进程与会话 | [Modules/PROCESS.md](Modules/PROCESS.md) | `result_message`、`enumerate_processes`、`attach`、`detach`、`process_info`、`suspend_process`、`resume_process`、`terminate_process`、`session_pid` | 9 |
| 内存 | [Modules/MEMORY.md](Modules/MEMORY.md) | `memory_read`、`memory_write`、`memory_dump`、`memory_regions`、`memory_readval` | 5 |
| 模块 | [Modules/MODULE.md](Modules/MODULE.md) | `module_list`、`module_find`、`module_base`、`module_exports`、`module_dump` | 5 |
| 线程 | [Modules/THREAD.md](Modules/THREAD.md) | `thread_list`、`thread_suspend`、`thread_resume`、`thread_terminate` | 4 |
| 调试 | [Modules/DEBUG.md](Modules/DEBUG.md) | `debug_attach`、`debug_detach`、`debug_pause`、`debug_resume`、`debug_step`、`debug_step_over`、`breakpoint_set`、`breakpoint_clear`、`hw_breakpoint_set`、`hw_breakpoint_clear`、`guard_set`、`guard_clear`、`debug_status`、`registers_get`、`register_get` | 15 |
| 反汇编 | [Modules/DISASM.md](Modules/DISASM.md) | `disasm_at`、`disasm_range` | 2 |
| 汇编 | [Modules/ASM.md](Modules/ASM.md) | `asm_assemble` | 1 |
| 解析 | [Modules/RESOLVE.md](Modules/RESOLVE.md) | `resolve_base`、`pattern_scan` | 2 |
| 监视 | [Modules/WATCH.md](Modules/WATCH.md) | `watch_list`、`watch_add`、`watch_remove`、`watch_refresh`、`watch_clear` | 5 |
| 注入 | [Modules/INJECT.md](Modules/INJECT.md) | `dll_inject`、`dll_eject`、`dll_list`、`dll_status`、`shellcode_inject`、`shellcode_inject_at`、`shellcode_status` | 7 |

数据类型文档:

| 类型 | 文档 |
|------|------|
| `Result`(14 个错误码) | [Types/RESULT.md](Types/RESULT.md) |
| `ValueType`、`BreakpointType` | [Types/ENUMS.md](Types/ENUMS.md) |
| `ProcessInfo`、`MemoryRegion`、`ModuleInfo`、`ExportInfo`、`ThreadInfo`、`RegisterInfo`、`BreakpointInfo`、`WatchEntry`、`Instruction`、`DebugStatus`、`InjectInfo` | [Types/STRUCTS.md](Types/STRUCTS.md) |

## 3. 调用前置条件(依赖分析)

### 3.1 会话生命周期(核心前置)

库维护**单一全局会话**(进程内只能有一个附加目标):

```
enumerate_processes / process_info(按 pid 查询)   ← 无需会话
        │
        ▼
 attach(pid)     ──►  会话操作(memory/module/thread/disasm/…)
        │
        ▼
 debug_attach()  ──►  调试操作(断点/单步/寄存器/guard)
        │
        ▼
 debug_detach()  /  detach()   ──►  关闭会话
```

- 所有需要目标进程的 API 均要求先 `attach(pid)`,否则返回 `Result::NotAttached`。
- 例外(按 pid 操作、无需会话):`enumerate_processes`、`process_info`、`suspend_process`、
  `resume_process`、`terminate_process`、`asm_assemble`、`result_message`。
- 调试会话嵌套在进程会话之上:`debug_attach()` 前必须先 `attach()`。
- `debug_step` / `debug_step_over` 允许在没有调试会话时**一次性**附加→单步→分离(CLI 非交互模式用法)。

### 3.2 权限要求

| 操作 | 权限 |
|------|------|
| 附加其他进程(`attach`) | 需要 `PROCESS_ALL_ACCESS` 或基础查询/读写权限;同权限级别进程可附加,低权限进程可能 `AccessDenied` |
| 调试(`debug_attach` 及断点/单步/寄存器) | 需要管理员权限(SeDebugPrivilege),否则 `AccessDenied` |
| 注入(`dll_inject` / `shellcode_inject`) | 目标进程需允许创建远程线程与写内存;受限目标返回 `AccessDenied` |

### 3.3 状态持久化

断点、注入记录、监视项会持久化到目标 PID 对应的状态目录,进程重启后仍可恢复:

```
%TEMP%/deeptrace_<pid>/
├── breaks.dat    # 软件/硬件断点记录
├── injects.dat   # DLL/Shellcode 注入记录
└── watch.dat     # Watch 监视项
```

### 3.4 线程安全

- 库使用**全局单会话**,设计为单线程使用(CLI 为非交互单线程模型)。
- 多线程并发调用会改变全局会话状态,属于未定义行为;如确需并发,调用方需自行加锁。

### 3.5 通用约定

- 地址类型均为 64 位 `uintptr_t`;`out_*` 输出参数可传 `nullptr` 忽略(有默认值者除外)。
- 所有函数返回 `Result`,成功为 `Result::Ok`;错误码含义见 [Types/RESULT.md](Types/RESULT.md)。
- 字符串类型:进程/模块名等宽字符用 `std::wstring`;解析/描述等 ASCII 内容用 `std::string`。
- 反汇编/汇编为 x64 指令集。

## 4. 快速上手

见 [GettingStarted.md](GettingStarted.md) —— 一个从零开始的完整示例(可直接编译运行)。

## 5. 完整示例

- [Examples/session_lifecycle.md](Examples/session_lifecycle.md) — 会话生命周期(枚举→附加→查询→分离)
- [Examples/read_write_memory.md](Examples/read_write_memory.md) — 远程内存读写与特征码扫描
- [Examples/debug_breakpoints.md](Examples/debug_breakpoints.md) — 调试会话与软断点

## 6. 变更历史

见 [CHANGELOG.md](CHANGELOG.md)。

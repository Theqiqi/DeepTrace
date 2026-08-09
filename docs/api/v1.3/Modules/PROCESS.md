# 模块:进程与会话

会话管理是全部目标进程操作的入口。`attach` 建立会话,`detach` 关闭会话;
按 pid 操作的查询/控制函数(`process_info`、`suspend_process`、`resume_process`、
`terminate_process`)不要求会话。`result_message` 为通用错误码描述工具。

## deeptrace::result_message

### 语法

```cpp
const char* result_message(Result r);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `r` | `Result` | 任意错误码枚举值 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `const char*` | 错误码的可读英文描述字符串;未知值返回 `"Unknown"` |

### 说明

将 `Result` 枚举值转换为人类可读的静态字符串,用于日志与错误提示。该函数永不失败、
不分配内存、不要求会话。CLI 的错误输出即基于此函数。典型用途:捕获任意 API 返回的
非 `Ok` 值并打印原因,配合 `Result` 判定流程分支。

前置条件:无。后置条件:无。

### 示例

```cpp
if (deeptrace::attach(pid) != deeptrace::Result::Ok) {
    // 由上层自行决定打印什么;也可以直接打印描述文本
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [Types/RESULT.md](../Types/RESULT.md)

---

## deeptrace::enumerate_processes

### 语法

```cpp
Result enumerate_processes(std::vector<ProcessInfo>& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `out` | `std::vector<ProcessInfo>&` | 输出参数,填充系统当前全部进程列表 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 枚举成功,`out` 已填充 |
| `Result::Error` | 系统快照创建失败 |

### 说明

枚举系统中所有进程并写入 `out`,每条含 pid、映像名、父 pid、线程数。调用前无需
`attach`,也无需管理员权限。典型场景:列出进程供用户选择目标,随后对其 `attach`。
该调用只做一次系统快照,快照后新起的进程不会出现在列表中。

前置条件:无。后置条件:无。

### 示例

```cpp
std::vector<deeptrace::ProcessInfo> procs;
if (deeptrace::enumerate_processes(procs) == deeptrace::Result::Ok) {
    for (const auto& p : procs) {
        std::wcout << p.pid << L"  " << p.name << L"\n";
    }
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::attach](#deeptraceattach)

---

## deeptrace::attach

### 语法

```cpp
Result attach(uint32_t pid);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `pid` | `uint32_t` | 目标进程 ID,不允许为 0 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 会话建立成功,后续目标进程操作可用 |
| `Result::InvalidArg` | `pid == 0` |
| `Result::NoSuchProcess` | 目标进程不存在 |
| `Result::AccessDenied` | 权限不足,无法打开目标进程句柄 |

### 说明

打开目标进程句柄并建立全局会话(同时记录 pid 与句柄)。库先尝试以
`PROCESS_ALL_ACCESS` 打开,失败时降级为查询/读写/创建线程/挂起恢复的组合权限,
以支持只读场景。会话建立后,内存、模块、线程、调试、解析、监视、注入等 API 方可调用;
否则这些 API 返回 `NotAttached`。同一进程内同时只存在一个会话,重复 `attach` 会
替换旧会话。进程 0 是系统空闲进程,永远无法附加,因此被显式拒绝。

前置条件:无。后置条件:会话激活;应通过 `detach` 关闭。

### 示例

```cpp
deeptrace::Result r = deeptrace::attach(1234);
if (r == deeptrace::Result::Ok) {
    // ... 执行内存/调试操作 ...
    deeptrace::detach();
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::detach](#deeptracedetach)
- [deeptrace::session_pid](#deeptracesession_pid)
- [deeptrace::debug_attach](DEBUG.md#deeptracedebug_attach)

---

## deeptrace::detach

### 语法

```cpp
Result detach();
```

### 参数

无。

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 会话已关闭(无论之前是否处于调试模式) |

### 说明

关闭当前会话:若处于调试模式,先调用 `DebugActiveProcessStop` 结束调试会话
(Windows 规定:调试器未调用该函数直接退出会连带终止被调试进程,故必须先分离),
再关闭进程句柄并清空 pid。该函数总是成功。CLI 每次命令结束后自动调用,保证目标
进程不被意外终止。

前置条件:无(即使未附加也安全)。后置条件:会话关闭;所有依赖会话的 API 返回
`NotAttached`。

### 示例

```cpp
deeptrace::attach(pid);
// ... 操作 ...
deeptrace::detach();
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::attach](#deeptraceattach)
- [deeptrace::debug_detach](DEBUG.md#deeptracedebug_detach)

---

## deeptrace::process_info

### 语法

```cpp
Result process_info(uint32_t pid, ProcessInfo& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `pid` | `uint32_t` | 目标进程 ID(可为 0,返回系统进程) |
| `out` | `ProcessInfo&` | 输出参数,进程信息 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 查询成功 |
| `Result::NoSuchProcess` | 进程不存在 |
| `Result::AccessDenied` | 无查询权限 |

### 说明

按 pid 查询单个进程的映像名、父进程、线程数,写入 `out`。**不要求会话**,适合在
`attach` 之前确认目标信息。与 `enumerate_processes` 不同,该函数实时打开目标进程
查询,结果不受快照时机影响。

前置条件:无。后置条件:无。

### 示例

```cpp
deeptrace::ProcessInfo info;
if (deeptrace::process_info(pid, info) == deeptrace::Result::Ok) {
    std::wcout << L"parent=" << info.parent_pid << L" threads=" << info.thread_count << L"\n";
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::enumerate_processes](#deeptraceenumerate_processes)

---

## deeptrace::suspend_process

### 语法

```cpp
Result suspend_process(uint32_t pid);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `pid` | `uint32_t` | 目标进程 ID |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 全部线程已挂起 |
| `Result::InvalidArg` | `pid == 0` |
| `Result::NoSuchProcess` | 进程不存在 |
| `Result::AccessDenied` | 无 `PROCESS_SUSPEND_RESUME` 权限 |

### 说明

挂起目标进程的所有线程,使目标暂停执行。**不要求会话**。调试器场景下常用于配合
内存修改或断点设置。恢复请调用 `resume_process`。注意:挂起后目标进程停止响应,
若调用方自身是目标进程会死锁。

前置条件:无。后置条件:目标进程暂停;需 `resume_process` 恢复。

### 示例

```cpp
deeptrace::suspend_process(pid);
// ... 修改内存 ...
deeptrace::resume_process(pid);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::resume_process](#deeptraceresume_process)

---

## deeptrace::resume_process

### 语法

```cpp
Result resume_process(uint32_t pid);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `pid` | `uint32_t` | 目标进程 ID |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 全部线程已恢复 |
| `Result::InvalidArg` | `pid == 0` |
| `Result::NoSuchProcess` | 进程不存在 |
| `Result::AccessDenied` | 无挂起/恢复权限 |

### 说明

恢复 `suspend_process` 挂起的进程线程。**不要求会话**。每个线程的挂起计数被减一,
只有挂起计数归零后线程才真正恢复执行,因此必须与挂起调用一一配对。

前置条件:目标进程曾被挂起。后置条件:目标进程恢复执行。

### 示例

```cpp
deeptrace::resume_process(pid);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::suspend_process](#deeptracesuspend_process)

---

## deeptrace::terminate_process

### 语法

```cpp
Result terminate_process(uint32_t pid, uint32_t exit_code);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `pid` | `uint32_t` | 目标进程 ID |
| `exit_code` | `uint32_t` | 进程退出码(如 0 表示正常) |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 已发出终止请求 |
| `Result::InvalidArg` | `pid == 0` |
| `Result::NoSuchProcess` | 进程不存在 |
| `Result::AccessDenied` | 无 `PROCESS_TERMINATE` 权限 |

### 说明

强制终止目标进程,退出码为 `exit_code`。**不要求会话**。该操作**不可逆**且不进行
优雅清理(不会触发 DLL 卸载等),调用前务必确认目标。系统关键进程可能返回
`AccessDenied`。

前置条件:无。后置条件:目标进程终止。

### 示例

```cpp
deeptrace::terminate_process(pid, 0);
```

### 头文件

```cpp
#include "deeptrace.h"
```

---

## deeptrace::session_pid

### 语法

```cpp
Result session_pid(uint32_t* out_pid);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `out_pid` | `uint32_t*` | 输出参数,当前会话目标 pid(未附加时为 0) |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 查询成功 |
| `Result::InvalidArg` | `out_pid == nullptr` |

### 说明

返回当前全局会话的目标 pid,未附加时写入 0。用于在复杂流程中确认当前操作目标,
或判断会话是否存在。

前置条件:无。后置条件:无。

### 示例

```cpp
uint32_t cur = 0;
deeptrace::session_pid(&cur);
if (cur != 0) { /* 已有会话 */ }
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::attach](#deeptraceattach)

# 模块:线程

线程枚举与线程级控制。`thread_list` 要求已 `attach` 目标进程;
`thread_suspend`/`thread_resume`/`thread_terminate` 按 tid 直接操作,不要求会话。

## deeptrace::thread_list

### 语法

```cpp
Result thread_list(std::vector<ThreadInfo>& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `out` | `std::vector<ThreadInfo>&` | 输出参数,会话目标进程的线程列表 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 枚举成功 |
| `Result::NotAttached` | 未附加会话 |

### 说明

枚举会话目标进程的全部线程(tid、优先级、入口地址)。调试场景下常用于选择要单步/读取
寄存器的线程(tid 0 表示首线程的约定见 `debug_step`、`registers_get`)。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
std::vector<deeptrace::ThreadInfo> threads;
deeptrace::thread_list(threads);
for (const auto& t : threads) {
    std::cout << "tid=" << t.tid << " prio=" << t.priority << "\n";
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::registers_get](DEBUG.md#deeptraceregisters_get)

---

## deeptrace::thread_suspend

### 语法

```cpp
Result thread_suspend(uint32_t tid);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `tid` | `uint32_t` | 目标线程 ID,不允许为 0 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 挂起成功 |
| `Result::InvalidArg` | `tid == 0` |
| `Result::AccessDenied` | 无 `THREAD_SUSPEND_RESUME` 权限 |
| `Result::NoSuchProcess` | 线程不存在 |

### 说明

挂起单个线程(挂起计数 +1)。**不要求会话**。挂起某线程可暂停其特定执行流,常用于
多线程目标中冻结某个 worker 或游戏逻辑线程。挂起计数需由 `thread_resume` 配对递减。

前置条件:无。后置条件:该线程暂停执行。

### 示例

```cpp
deeptrace::thread_suspend(tid);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::thread_resume](#deeptracethread_resume)

---

## deeptrace::thread_resume

### 语法

```cpp
Result thread_resume(uint32_t tid);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `tid` | `uint32_t` | 目标线程 ID,不允许为 0 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 恢复成功 |
| `Result::InvalidArg` | `tid == 0` |
| `Result::AccessDenied` | 无挂起/恢复权限 |
| `Result::NoSuchProcess` | 线程不存在 |

### 说明

恢复 `thread_suspend` 挂起的线程。**不要求会话**。挂起计数归零前线程不会恢复,
必须与挂起调用一一配对。

前置条件:线程曾被挂起。后置条件:该线程恢复执行。

### 示例

```cpp
deeptrace::thread_resume(tid);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::thread_suspend](#deeptracethread_suspend)

---

## deeptrace::thread_terminate

### 语法

```cpp
Result thread_terminate(uint32_t tid, uint32_t exit_code);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `tid` | `uint32_t` | 目标线程 ID,不允许为 0 |
| `exit_code` | `uint32_t` | 线程退出码 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 终止成功 |
| `Result::InvalidArg` | `tid == 0` |
| `Result::Error` | `TerminateThread` 系统调用失败 |
| `Result::AccessDenied` | 无 `THREAD_TERMINATE` 权限 |
| `Result::NoSuchProcess` | 线程不存在 |

### 说明

强制终止目标线程(基于 `TerminateThread`)。**不要求会话**。该操作**不可逆**,不执行
线程清理(不释放栈/DLL 锁),可能造成目标进程不稳定;终止主线程通常导致进程退出。
调用前务必确认。

前置条件:无。后置条件:线程终止。

### 示例

```cpp
deeptrace::thread_terminate(tid, 0);
```

### 头文件

```cpp
#include "deeptrace.h"
```

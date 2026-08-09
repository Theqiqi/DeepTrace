# 模块:调试

调试会话(附加/分离)、暂停/恢复、单步、断点(软件/硬件/页守卫)、寄存器读写。
调试会话建立在进程会话之上:**先 `attach(pid)` 再 `debug_attach()`**。

断点记录持久化到 `%TEMP%/deeptrace_<pid>/breaks.dat`,同一目标进程重开会话后
仍可通过 `debug_status` 看到并清除。

## deeptrace::debug_attach

### 语法

```cpp
Result debug_attach();
```

### 参数

无。

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 进入调试模式 |
| `Result::NotAttached` | 尚未 `attach(pid)` |
| `Result::AlreadyExists` | 已处于调试模式 |
| `Result::AccessDenied` | 无调试权限(需要管理员/SeDebugPrivilege) |

### 说明

对会话目标进程执行 `DebugActiveProcess`,进入调试模式。调试模式下可使用单步、
硬件断点等依赖调试器的能力。Windows 要求调试器在退出前调用 `DebugActiveProcessStop`
(库在 `detach`/`debug_detach` 中处理),否则被调试进程会被系统连带终止。
调试模式是进程会话的增强状态,`debug_detach` 只退出调试模式、保留进程会话。

前置条件:已 `attach(pid)`。后置条件:进入调试模式;结束前调用 `debug_detach` 或 `detach`。

### 示例

```cpp
deeptrace::attach(pid);
if (deeptrace::debug_attach() == deeptrace::Result::Ok) {
    // ... 调试操作 ...
    deeptrace::debug_detach();
}
deeptrace::detach();
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::debug_detach](#deeptracedebug_detach)

---

## deeptrace::debug_detach

### 语法

```cpp
Result debug_detach();
```

### 参数

无。

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 退出调试模式 |
| `Result::NotAttached` | 未处于调试模式 |
| `Result::Error` | `DebugActiveProcessStop` 失败 |

### 说明

结束调试模式(不关闭进程会话)。若失败返回 `Error` 但库仍会清除调试状态标记,
避免后续重复尝试。退出调试模式后被调试进程继续独立运行,断点(INT3)若未清除
会导致目标异常——结束前应清理断点。

前置条件:已 `debug_attach()`。后置条件:退出调试模式;进程会话保留。

### 示例

```cpp
deeptrace::debug_detach();
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::debug_attach](#deeptracedebug_attach)
- [deeptrace::breakpoint_clear](#deeptracebreakpoint_clear)

---

## deeptrace::debug_pause

### 语法

```cpp
Result debug_pause();
```

### 参数

无。

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 已挂起目标全部线程 |
| `Result::NotAttached` | 未附加会话 |

### 说明

挂起会话目标进程的全部线程(等价于 `suspend_process(session.pid)`,但作用于会话目标)。
用于冻结目标进行内存修改或断点调试。恢复调用 `debug_resume`。

前置条件:已 `attach(pid)`。后置条件:目标暂停;需 `debug_resume` 恢复。

### 示例

```cpp
deeptrace::debug_pause();
// ... 修改目标 ...
deeptrace::debug_resume();
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::debug_resume](#deeptracedebug_resume)

---

## deeptrace::debug_resume

### 语法

```cpp
Result debug_resume();
```

### 参数

无。

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 已恢复目标全部线程 |
| `Result::NotAttached` | 未附加会话 |

### 说明

恢复 `debug_pause` 挂起的会话目标进程线程。需与挂起调用配对。

前置条件:目标被 `debug_pause` 挂起。后置条件:目标恢复执行。

### 示例

```cpp
deeptrace::debug_resume();
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::debug_pause](#deeptracedebug_pause)

---

## deeptrace::debug_step

### 语法

```cpp
Result debug_step(uint32_t tid, uintptr_t* out_rip);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `tid` | `uint32_t` | 目标线程 ID;0 = 会话首线程 |
| `out_rip` | `uintptr_t*` | 可选,单步后的 RIP 值;传 `nullptr` 忽略 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 单步完成,`*out_rip` 为下一条指令地址 |
| `Result::NotAttached` | 尚未 `attach(pid)` |
| `Result::AccessDenied` | 调试附加失败(权限不足) |

### 说明

单步执行目标线程的一条指令(设置陷阱标志并等待 `EXCEPTION_SINGLE_STEP`)。
若尚未进入调试模式,则自动执行「附加→单步→分离」的一次性流程(CLI 非交互模式
即为此设计);已处于调试模式则直接单步。`tid=0` 表示目标进程首线程。单步返回后
可通过 `*out_rip` 得知当前执行位置,配合 `disasm_at` 可跟踪代码流。

前置条件:已 `attach(pid)`。后置条件:目标执行了一条指令。

### 示例

```cpp
uintptr_t rip = 0;
if (deeptrace::debug_step(0, &rip) == deeptrace::Result::Ok) {
    // rip 为下一条指令地址
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::debug_step_over](#deeptracedebug_step_over)

---

## deeptrace::debug_step_over

### 语法

```cpp
Result debug_step_over(uint32_t tid, uintptr_t* out_rip);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `tid` | `uint32_t` | 目标线程 ID;0 = 会话首线程 |
| `out_rip` | `uintptr_t*` | 可选,执行后的 RIP 值 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 步过完成 |
| `Result::NotAttached` | 尚未 `attach(pid)` |
| `Result::AccessDenied` | 调试附加失败 |

### 说明

「步过」:若当前指令是近调用(call),在返回地址设置临时软件断点、运行至命中后恢复,
整体表现为不进入函数体;否则行为等同 `debug_step`。同样支持无调试会话时的一次性
附加流程。适合逐行调试时跳过函数调用。

前置条件:已 `attach(pid)`。后置条件:目标执行至当前函数的下一行。

### 示例

```cpp
uintptr_t rip = 0;
deeptrace::debug_step_over(0, &rip);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::debug_step](#deeptracedebug_step)

---

## deeptrace::breakpoint_set

### 语法

```cpp
Result breakpoint_set(uintptr_t addr, BreakpointInfo& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `addr` | `uintptr_t` | 断点地址,不允许为 0 |
| `out` | `BreakpointInfo&` | 输出参数,断点信息(原字节等) |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 断点已设置并持久化 |
| `Result::InvalidArg` | `addr == 0` |
| `Result::NotAttached` | 未附加会话 |
| `Result::AlreadyExists` | 该地址已有软件断点 |
| `Result::ReadFault` | 目标地址不可读(无法保存原字节) |
| `Result::WriteFault` | 目标地址不可写(无法写入 0xCC) |

### 说明

在目标进程 `addr` 处设置软件断点:读取并保存原字节,改写为 `0xCC`(INT3),记录写入
`%TEMP%/deeptrace_<pid>/breaks.dat`。目标执行到该地址会触发单步异常。断点持久化
意味着重开会话后仍可恢复/清除。清除用 `breakpoint_clear`。注意:设置断点后目标
代码已被修改,不清除断点就分离调试可能导致目标异常。

前置条件:已 `attach(pid)`;`addr` 处代码可读可写。后置条件:目标代码被改写并持久化记录。

### 示例

```cpp
deeptrace::BreakpointInfo bp;
if (deeptrace::breakpoint_set(0x140001000, bp) == deeptrace::Result::Ok) {
    // bp.original_byte 为原字节
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::breakpoint_clear](#deeptracebreakpoint_clear)
- [deeptrace::hw_breakpoint_set](#deeptracehw_breakpoint_set)

---

## deeptrace::breakpoint_clear

### 语法

```cpp
Result breakpoint_clear(uintptr_t addr);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `addr` | `uintptr_t` | 断点地址 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 断点已移除 |
| `Result::NotAttached` | 未附加会话 |
| `Result::NotFound` | 该地址没有软件断点记录 |
| `Result::WriteFault` | 恢复原字节失败 |

### 说明

清除 `addr` 处的软件断点:将持久化记录中的原字节写回目标内存并删除记录。若断点
不在记录中返回 `NotFound`。恢复失败返回 `WriteFault`(此时记录仍保留,可重试)。

前置条件:已 `attach(pid)`。后置条件:目标代码恢复原样。

### 示例

```cpp
deeptrace::breakpoint_clear(0x140001000);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::breakpoint_set](#deeptracebreakpoint_set)

---

## deeptrace::hw_breakpoint_set

### 语法

```cpp
Result hw_breakpoint_set(uintptr_t addr, uint32_t type, uint32_t length);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `addr` | `uintptr_t` | 断点地址,不允许为 0 |
| `type` | `uint32_t` | 触发类型:0=执行,1=写入,2=读/写 |
| `length` | `uint32_t` | 监视长度,仅允许 1/2/4/8 字节 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 硬件断点已设置到全部线程的 DR 寄存器 |
| `Result::InvalidArg` | `addr == 0`、`type > 2` 或 `length ∉ {1,2,4,8}` |
| `Result::NotAttached` | 未附加会话 |
| `Result::AlreadyExists` | 该地址已有硬件断点 |
| `Result::Error` | 无空闲 DR 槽位(最多 4 个) |

### 说明

使用 x64 调试寄存器 DR0-DR3 设置硬件断点,**不修改目标代码**(对只读/受保护代码页
也有效,这是相对软件断点的核心优势)。type/长度按 Intel DR7 语义编码,设置应用于
目标全部线程。最多 4 个硬件断点,无空闲槽位返回 `Error`。记录持久化到
`breaks.dat`。清除用 `hw_breakpoint_clear`。设置不要求先 `debug_attach`(CLI 的
`debug hbreak` 即可用),但需要管理员权限访问目标线程上下文;DR 断点命中产生的
异常需调试器处理,无调试会话时目标可能直接崩溃,建议配合 `debug_attach` 使用。

前置条件:已 `attach(pid)` 且已 `debug_attach()`。后置条件:目标线程 DR 寄存器被修改。

### 示例

```cpp
deeptrace::hw_breakpoint_set(0x140001000, 0 /*execute*/, 1);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::hw_breakpoint_clear](#deeptracehw_breakpoint_clear)

---

## deeptrace::hw_breakpoint_clear

### 语法

```cpp
Result hw_breakpoint_clear(uintptr_t addr);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `addr` | `uintptr_t` | 硬件断点地址 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 硬件断点已清除 |
| `Result::NotAttached` | 未附加会话 |
| `Result::NotFound` | 该地址没有硬件断点记录 |

### 说明

清除 `addr` 处的硬件断点:复位全部线程对应 DR 槽位并删除持久化记录。

前置条件:已 `attach(pid)`。后置条件:DR 寄存器复位。

### 示例

```cpp
deeptrace::hw_breakpoint_clear(0x140001000);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::hw_breakpoint_set](#deeptracehw_breakpoint_set)

---

## deeptrace::guard_set

### 语法

```cpp
Result guard_set(uintptr_t addr, size_t size);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `addr` | `uintptr_t` | 目标区域起始地址 |
| `size` | `size_t` | 区域大小(页对齐取整) |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 页守卫已设置 |
| `Result::NotAttached` | 未附加会话 |
| `Result::Error` | `VirtualProtectEx` 失败 |

### 说明

对目标进程 `addr` 起的区域设置 `PAGE_GUARD` 属性(连同 `PAGE_EXECUTE_READWRITE`),
使目标对该区域的访问触发一次守卫异常。适合监测/拦截对特定数据的读写。守卫是一次性
的:触发后属性被系统清除,需重新设置。清除用 `guard_clear`。

前置条件:已 `attach(pid)`。后置条件:目标区域带 PAGE_GUARD 属性。

### 示例

```cpp
deeptrace::guard_set(0x140001000, 0x1000);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::guard_clear](#deeptraceguard_clear)

---

## deeptrace::guard_clear

### 语法

```cpp
Result guard_clear(uintptr_t addr, size_t size);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `addr` | `uintptr_t` | 目标区域起始地址 |
| `size` | `size_t` | 区域大小 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 守卫已移除 |
| `Result::NotAttached` | 未附加会话 |
| `Result::Error` | `VirtualProtectEx` 失败 |

### 说明

将 `guard_set` 设置的区域恢复为 `PAGE_EXECUTE_READWRITE`(清除 PAGE_GUARD 位)。
多次设置守卫后应调用本函数恢复,避免目标后续访问被拦截。

前置条件:已 `attach(pid)`。后置条件:目标区域恢复正常属性。

### 示例

```cpp
deeptrace::guard_clear(0x140001000, 0x1000);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::guard_set](#deeptraceguard_set)

---

## deeptrace::debug_status

### 语法

```cpp
Result debug_status(DebugStatus& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `out` | `DebugStatus&` | 输出参数,当前调试/会话状态 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 查询成功 |

### 说明

返回当前会话与调试状态:是否附加(调试模式或进程会话)、会话 pid、软件/硬件断点
数量(基于持久化记录统计)。**不要求会话**。适合在流程入口展示状态或做一致性检查。

前置条件:无。后置条件:无。

### 示例

```cpp
deeptrace::DebugStatus st;
deeptrace::debug_status(st);
std::cout << "attached=" << st.attached << " bp=" << st.breakpoint_count << "\n";
```

### 头文件

```cpp
#include "deeptrace.h"
```

---

## deeptrace::registers_get

### 语法

```cpp
Result registers_get(std::vector<RegisterInfo>& out, uint32_t tid);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `out` | `std::vector<RegisterInfo>&` | 输出参数,寄存器名/值列表 |
| `tid` | `uint32_t` | 目标线程 ID;0 = 会话首线程 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 读取成功 |
| `Result::NotAttached` | 未附加会话 |

### 说明

读取目标线程的完整寄存器集合(通用寄存器、RIP、RSP、EFLAGS 及调试寄存器等)写入
`out`。寄存器读取不需要调试模式(基于 `GetThreadContext`)。`tid=0` 表示首线程。
典型用途:单步后观察寄存器状态、分析调用约定参数传递。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
std::vector<deeptrace::RegisterInfo> regs;
deeptrace::registers_get(regs, 0);
for (const auto& r : regs) {
    std::cout << r.name << " = " << r.value << "\n";
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::register_get](#deeptraceregister_get)

---

## deeptrace::register_get

### 语法

```cpp
Result register_get(const std::string& name, uint64_t* out_value, uint32_t tid);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `name` | `const std::string&` | 寄存器名(如 `"rax"`、`"rip"`、`"eflags"`) |
| `out_value` | `uint64_t*` | 输出参数,寄存器值 |
| `tid` | `uint32_t` | 目标线程 ID;0 = 会话首线程 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 读取成功 |
| `Result::InvalidArg` | `out_value == nullptr` |
| `Result::NotAttached` | 未附加会话 |

### 说明

`registers_get` 的单寄存器版本,按名称返回一个寄存器的值。寄存器名大小写不敏感
(如 `"RAX"` 等同 `"rax"`)。未知寄存器名返回 `Result::Error`(由底层解析失败引起)。
适合快速取样单个关键寄存器。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
uint64_t rax = 0;
deeptrace::register_get("rax", &rax, 0);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::registers_get](#deeptraceregisters_get)

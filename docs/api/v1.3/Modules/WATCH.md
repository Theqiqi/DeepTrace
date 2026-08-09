# 模块:监视(Watch)

对目标地址的「监视项」管理:记录地址/类型/描述,可随时刷新读取当前值。
监视项持久化到 `%TEMP%/deeptrace_<pid>/watch.dat`,目标进程重开后仍可列出。

`watch_list` 与 `watch_remove`、`watch_clear` 只需会话 pid(未附加句柄也能操作,
此时值显示为 `??`);`watch_add` 与 `watch_refresh` 需要已附加的进程句柄。

## deeptrace::watch_list

### 语法

```cpp
Result watch_list(std::vector<WatchEntry>& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `out` | `std::vector<WatchEntry>&` | 输出参数,监视项列表 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 列表已生成(可能为空) |
| `Result::NotAttached` | 无会话(未设置 pid) |

### 说明

列出会话目标进程的全部监视项,并按地址实时读取当前值:已附加时读取真实内存值,
未附加时值标记为 `"??"` 且 `valid=false`(不会静默给出过期数据)。`index` 字段为
持久化顺序索引,供 `watch_remove` 使用。

前置条件:已 `attach(pid)`(无句柄也可,值无效)。后置条件:无。

### 示例

```cpp
std::vector<deeptrace::WatchEntry> ws;
deeptrace::watch_list(ws);
for (const auto& w : ws) {
    std::cout << w.index << ": " << w.value << "\n";
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::watch_add](#deeptracewatch_add)
- [deeptrace::watch_refresh](#deeptracewatch_refresh)

---

## deeptrace::watch_add

### 语法

```cpp
Result watch_add(const std::string& desc, uintptr_t addr, ValueType type);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `desc` | `const std::string&` | 描述文本(写入文件时 `|` 会被替换为空格) |
| `addr` | `uintptr_t` | 监视地址 |
| `type` | `ValueType` | 值类型 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 监视项已添加并持久化 |
| `Result::NotAttached` | 未附加会话 |
| `Result::ReadFault` | 目标地址当前不可读(拒绝添加) |

### 说明

新增一个监视项并写入 `watch.dat`。添加前会读 1 字节验证地址可读,不可读返回
`ReadFault` 且不添加,避免记录永久无效的地址。重复地址允许(每条独立)。刷新值用
`watch_refresh`,删除用 `watch_remove`,清空用 `watch_clear`。

前置条件:已 `attach(pid)`。后置条件:监视项持久化。

### 示例

```cpp
deeptrace::watch_add("hp", 0x140001000, deeptrace::ValueType::Dword);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::watch_remove](#deeptracewatch_remove)

---

## deeptrace::watch_remove

### 语法

```cpp
Result watch_remove(uint32_t index);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `index` | `uint32_t` | 监视项索引(来自 `watch_list` 的 `index` 字段) |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 监视项已删除 |
| `Result::NotAttached` | 无会话 |
| `Result::NotFound` | 索引越界 |

### 说明

按索引删除监视项并写回 `watch.dat`。删除后其余项的索引会前移,需以最新
`watch_list` 结果为准。

前置条件:已 `attach(pid)`。后置条件:监视项持久化更新。

### 示例

```cpp
deeptrace::watch_remove(0);  // 删除第一个监视项
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::watch_list](#deeptracewatch_list)

---

## deeptrace::watch_refresh

### 语法

```cpp
Result watch_refresh(std::vector<WatchEntry>& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `out` | `std::vector<WatchEntry>&` | 输出参数,含最新值的监视项列表 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 刷新完成 |
| `Result::NotAttached` | 未附加会话 |

### 说明

强制重新读取全部监视项的当前值并返回列表(行为同 `watch_list`,但要求已附加句柄)。
用于定时轮询目标变量变化(如游戏血量、金币)。单个项读取失败时该项 `valid=false`、
值为 `"??"`,不影响其余项。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
std::vector<deeptrace::WatchEntry> ws;
deeptrace::watch_refresh(ws);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::watch_list](#deeptracewatch_list)

---

## deeptrace::watch_clear

### 语法

```cpp
Result watch_clear();
```

### 参数

无。

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 已清空全部监视项 |
| `Result::NotAttached` | 无会话 |

### 说明

删除会话目标进程的全部监视项(清空 `watch.dat`)。

前置条件:已 `attach(pid)`。后置条件:监视项清空。

### 示例

```cpp
deeptrace::watch_clear();
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::watch_remove](#deeptracewatch_remove)

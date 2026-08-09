# 模块:内存

远程内存读写能力。全部函数要求已 `attach` 目标进程,否则返回 `NotAttached`。

## deeptrace::memory_read

### 语法

```cpp
Result memory_read(uintptr_t addr, void* buf, size_t size, size_t* out_read);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `addr` | `uintptr_t` | 目标进程内起始地址 |
| `buf` | `void*` | 输出缓冲区(调用方分配,至少 `size` 字节) |
| `size` | `size_t` | 期望读取字节数,必须非 0 |
| `out_read` | `size_t*` | 可选,实际读取字节数;传 `nullptr` 忽略 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 读取调用成功(可能为部分读取,以 `*out_read` 为准) |
| `Result::InvalidArg` | `buf == nullptr` 或 `size == 0` |
| `Result::NotAttached` | 未附加会话 |
| `Result::ReadFault` | 目标内存不可读 |

### 说明

从目标进程 `addr` 处读取最多 `size` 字节到本地 `buf`。与 `memory_dump` 不同,该函数
允许部分读取:跨越无效页时只返回可读部分,实际长度写入 `out_read`。适合读取已知大小
的结构体、变量或不确定可读性的区域。地址越界或页不可读时返回 `ReadFault` 而非崩溃。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
uint8_t buf[16];
size_t got = 0;
if (deeptrace::memory_read(0x140001000, buf, sizeof buf, &got) == deeptrace::Result::Ok) {
    // buf[0..got) 为读取到的数据
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::memory_write](#deeptracememory_write)
- [deeptrace::memory_dump](#deeptracememory_dump)

---

## deeptrace::memory_write

### 语法

```cpp
Result memory_write(uintptr_t addr, const void* buf, size_t size, size_t* out_written);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `addr` | `uintptr_t` | 目标进程内起始地址 |
| `buf` | `const void*` | 待写入的本地数据 |
| `size` | `size_t` | 写入字节数,必须非 0 |
| `out_written` | `size_t*` | 可选,实际写入字节数 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 写入调用成功(可能部分写入) |
| `Result::InvalidArg` | `buf == nullptr` 或 `size == 0` |
| `Result::NotAttached` | 未附加会话 |
| `Result::WriteFault` | 目标内存不可写(只读页/越界) |

### 说明

向目标进程 `addr` 处写入 `size` 字节本地数据。目标内存必须可写(如 `PAGE_READWRITE`),
写只读代码段或代码页会返回 `WriteFault`。典型用途:修改全局变量、游戏数值、绕过检查等。
注意写目标代码段通常需要先调整页保护(见 `guard_clear`/`ProtectRegion` 相关能力)。

前置条件:已 `attach(pid)`;目标地址可写。后置条件:目标内存被修改。

### 示例

```cpp
uint32_t v = 0xCAFEBABE;
size_t written = 0;
deeptrace::memory_write(0x140001000, &v, sizeof v, &written);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::memory_read](#deeptracememory_read)

---

## deeptrace::memory_dump

### 语法

```cpp
Result memory_dump(uintptr_t addr, size_t size, std::vector<uint8_t>& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `addr` | `uintptr_t` | 目标进程内起始地址 |
| `size` | `size_t` | 读取字节数,范围 1 ~ 64 MiB |
| `out` | `std::vector<uint8_t>&` | 输出参数,完整字节序列 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 完整读取 `size` 字节 |
| `Result::InvalidArg` | `size == 0` 或 `size > 64 MiB` |
| `Result::NotAttached` | 未附加会话 |
| `Result::ReadFault` | 读取不完整(区域含不可读页) |

### 说明

一次性读取目标进程 `addr` 起 `size` 字节到 `out`。与 `memory_read` 不同,它要求
**完整读取**:任何不完整都返回 `ReadFault` 且不输出部分数据,适合转储连续内存区域、
模块数据或用于十六进制查看。上限 64 MiB 防止单次分配过大;更大的区域应分块调用
或使用 `memory_regions` 规划。

前置条件:已 `attach(pid)`;目标区域连续可读。后置条件:无。

### 示例

```cpp
std::vector<uint8_t> bytes;
if (deeptrace::memory_dump(0x140001000, 0x100, bytes) == deeptrace::Result::Ok) {
    // bytes.size() == 0x100
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::memory_read](#deeptracememory_read)
- [deeptrace::memory_regions](#deeptracememory_regions)

---

## deeptrace::memory_regions

### 语法

```cpp
Result memory_regions(std::vector<MemoryRegion>& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `out` | `std::vector<MemoryRegion>&` | 输出参数,目标进程内存区域列表 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 枚举成功 |
| `Result::NotAttached` | 未附加会话 |

### 说明

枚举目标进程全部已提交/保留的内存区域(基址、大小、保护、状态、类型),供内存布局
分析、特征码扫描规划(`pattern_scan` 内部即使用该接口)或防护策略设计使用。
`MemoryRegion.protection` 为 `PAGE_*` 位组合,`state` 区分 `MEM_COMMIT`/`MEM_RESERVE`。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
std::vector<deeptrace::MemoryRegion> regions;
deeptrace::memory_regions(regions);
for (const auto& rg : regions) {
    if (rg.state == MEM_COMMIT) { /* 处理已提交区域 */ }
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::pattern_scan](RESOLVE.md#deeptracepattern_scan)

---

## deeptrace::memory_readval

### 语法

```cpp
Result memory_readval(uintptr_t addr, ValueType type, std::string& out_text);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `addr` | `uintptr_t` | 目标进程内起始地址 |
| `type` | `ValueType` | 值类型(决定读取长度与格式化方式) |
| `out_text` | `std::string&` | 输出参数,格式化后的值文本 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 读取并格式化成功 |
| `Result::NotAttached` | 未附加会话 |
| `Result::ReadFault` | 无法完整读取该类型所需字节数 |
| `Result::Error` | 值格式化失败(理论不可达) |

### 说明

按 `ValueType` 读取目标地址处的类型化值并格式化为文本:整数类型输出 `0x` 前缀十六进制
(如 `0x11223344`),浮点类型输出十进制。读取长度为该类型的固定大小(见
[Types/ENUMS.md](../Types/ENUMS.md)),不足则 `ReadFault`。该函数是「查看目标某个
变量的当前值」的快捷方式,CLI 的 `mem readval` 命令即基于此。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
std::string text;
if (deeptrace::memory_readval(addr, deeptrace::ValueType::Dword, text) == deeptrace::Result::Ok) {
    // text == "0x11223344"
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::memory_read](#deeptracememory_read)
- [ValueType](../Types/ENUMS.md#valuetype--值类型)

# 模块:解析

地址解析与特征码扫描。`pattern_scan` 要求已 `attach` 目标进程。

## deeptrace::resolve_base

### 语法

```cpp
Result resolve_base(const std::string& name, uintptr_t* out_base);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `name` | `const std::string&` | 模块名或完整路径(ASCII) |
| `out_base` | `uintptr_t*` | 输出参数,模块基址 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 成功 |
| `Result::InvalidArg` | `out_base == nullptr` |
| `Result::NotAttached` | 未附加会话 |
| `Result::NotFound` | 模块未加载 |

### 说明

`module_base` 的别名,语义与返回值完全一致(见
[deeptrace::module_base](MODULE.md#deeptracemodule_base))。保留此接口用于语义区分
「解析模块基址」与「模块管理」。基于基址+偏移或配合 `pattern_scan` 可精确定位目标
符号地址。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
uintptr_t base = 0;
deeptrace::resolve_base("game.exe", &base);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::module_base](MODULE.md#deeptracemodule_base)
- [deeptrace::pattern_scan](#deeptracepattern_scan)

---

## deeptrace::pattern_scan

### 语法

```cpp
Result pattern_scan(const std::string& pattern, std::vector<uintptr_t>& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `pattern` | `const std::string&` | 特征码,空格分隔的十六进制字节,`??` 表示任意字节 |
| `out` | `std::vector<uintptr_t>&` | 输出参数,全部命中地址(可为空) |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 扫描完成(命中列表写入 `out`,可能为空) |
| `Result::InvalidArg` | `pattern` 为空 |
| `Result::BadFormat` | 特征码含非法字符(非十六进制、通配符不完整等) |
| `Result::NotAttached` | 未附加会话 |

### 说明

在目标进程全部已提交、可读且无 `PAGE_GUARD` 的内存区域中扫描特征码,返回所有命中
地址。特征码格式示例:`"48 8B ?? ?? 00"`(`??` 匹配任意单字节)。扫描按 1 MiB 分块
进行并处理跨块命中,大进程可能耗时数秒至数十秒。典型用途:定位函数地址(跨版本
更新不变的特征)、找全局数据。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
std::vector<uintptr_t> hits;
deeptrace::pattern_scan("48 8B 05 ?? ?? ?? ??", hits);
for (auto h : hits) { /* h 为命中地址 */ }
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::memory_regions](MEMORY.md#deeptracememory_regions)

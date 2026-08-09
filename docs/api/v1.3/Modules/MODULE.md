# 模块:模块查询

查询目标进程加载的模块(EXE/DLL)信息。全部函数要求已 `attach` 目标进程。
模块名匹配规则:对模块名、完整路径(均忽略大小写)精确匹配,也允许省略扩展名
(如 `kernel32` 可匹配 `kernel32.dll`)。

## deeptrace::module_list

### 语法

```cpp
Result module_list(std::vector<ModuleInfo>& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `out` | `std::vector<ModuleInfo>&` | 输出参数,模块列表(基址/大小/名称/路径) |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 枚举成功 |
| `Result::NotAttached` | 未附加会话 |

### 说明

枚举目标进程全部已加载模块。模块信息常用于定位游戏/应用基址、校验注入是否成功
(`dll_list` 也基于模块枚举判断 DLL 是否仍加载)、解析依赖。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
std::vector<deeptrace::ModuleInfo> mods;
deeptrace::module_list(mods);
for (const auto& m : mods) {
    std::wcout << m.name << L" @ " << m.base << L"\n";
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::module_base](#deeptracemodule_base)
- [deeptrace::module_exports](#deeptracemodule_exports)

---

## deeptrace::module_find

### 语法

```cpp
Result module_find(const std::string& name, ModuleInfo& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `name` | `const std::string&` | 模块名或完整路径(ASCII,忽略大小写,可省略扩展名) |
| `out` | `ModuleInfo&` | 输出参数,匹配的模块信息 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 找到模块 |
| `Result::InvalidArg` | `name` 为空 |
| `Result::NotAttached` | 未附加会话 |
| `Result::NotFound` | 目标进程未加载该模块 |

### 说明

按名称查找模块并返回完整信息(基址/大小/名称/路径)。匹配规则见模块页首说明。典型用途:
获取游戏主模块信息、确认某 DLL 是否被加载(注意与注入记录无关,只查真实加载状态)。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
deeptrace::ModuleInfo m;
if (deeptrace::module_find("kernel32.dll", m) == deeptrace::Result::Ok) {
    // m.base 为 kernel32 基址
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::module_list](#deeptracemodule_list)

---

## deeptrace::module_base

### 语法

```cpp
Result module_base(const std::string& name, uintptr_t* out_base);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `name` | `const std::string&` | 模块名或完整路径(ASCII) |
| `out_base` | `uintptr_t*` | 输出参数,模块基址 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 成功,`*out_base` 为基址 |
| `Result::InvalidArg` | `out_base == nullptr` |
| `Result::NotAttached` | 未附加会话 |
| `Result::NotFound` | 模块未加载 |

### 说明

`module_find` 的便捷封装,只返回基址。基址 + 偏移是计算目标地址(全局变量、函数地址)的
常用手段;结合 `disasm_at`、`pattern_scan` 可完成特征定位。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
uintptr_t base = 0;
deeptrace::module_base("game.exe", &base);
uintptr_t g_health = base + 0x123456;
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::resolve_base](RESOLVE.md#deeptraceresolve_base)

---

## deeptrace::module_exports

### 语法

```cpp
Result module_exports(const std::string& name, std::vector<ExportInfo>& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `name` | `const std::string&` | 模块名或完整路径(ASCII) |
| `out` | `std::vector<ExportInfo>&` | 输出参数,导出符号列表(名称/地址) |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 解析成功 |
| `Result::NotAttached` | 未附加会话 |
| `Result::NotFound` | 模块未加载或非 PE 模块 |

### 说明

解析目标进程中模块的 PE 导出表,返回导出函数名与绝对地址。用于定位 API 真实地址
(绕过 IAT 或直接在目标内调用)、HOOK 目标计算等。仅对导出型模块(DLL、部分 EXE)有效,
无导出表的模块返回空列表但结果为 `Ok`。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
std::vector<deeptrace::ExportInfo> exps;
if (deeptrace::module_exports("ntdll.dll", exps) == deeptrace::Result::Ok) {
    for (const auto& e : exps) {
        if (e.name == "NtQueryInformationProcess") { /* e.address */ }
    }
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

---

## deeptrace::module_dump

### 语法

```cpp
Result module_dump(const std::string& name, const std::string& output_file,
                   std::string* out_hex);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `name` | `const std::string&` | 模块名或完整路径(ASCII) |
| `output_file` | `const std::string&` | 输出文件路径(写二进制);为空字符串则输出 hex 文本 |
| `out_hex` | `std::string*` | 可选,模块内容的十六进制文本;传 `nullptr` 忽略 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 转储成功(写入文件或 `*out_hex`) |
| `Result::NotAttached` | 未附加会话 |
| `Result::NotFound` | 模块未加载 |
| `Result::Error` | 输出文件无法打开 |

### 说明

将模块映像内容读出:指定 `output_file` 时按二进制写入磁盘(可保存被修改的模块做离线
分析);否则将内容以十六进制文本写入 `out_hex`。读取按 1 MiB 分块进行,遇到不可读页
提前停止(此时仍返回 `Ok`,以实际读到内容为准)。模块体积可达数十 MiB,转储大模块
时注意内存占用。

前置条件:已 `attach(pid)`。后置条件:`output_file` 被创建或覆盖。

### 示例

```cpp
// 保存到文件
deeptrace::module_dump("game.exe", "C:\\temp\\game.bin", nullptr);
// 或取 hex 文本
std::string hex;
deeptrace::module_dump("game.exe", "", &hex);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::memory_dump](MEMORY.md#deeptracememory_dump)

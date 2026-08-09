# 模块:注入

DLL 注入与 Shellcode 注入。注入通过远程线程(`CreateRemoteThreadEx`)执行,要求
目标进程允许创建远程线程与写内存(`attach` 的降级权限已包含这些能力)。
注入记录持久化到 `%TEMP%/deeptrace_<pid>/injects.dat`。

## deeptrace::dll_inject

### 语法

```cpp
Result dll_inject(const std::string& path, InjectInfo& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `path` | `std::string&` | DLL 完整路径(ASCII;建议用绝对路径) |
| `out` | `InjectInfo&` | 输出参数,注入结果(基址/线程/状态) |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 注入成功,DLL 已在目标加载 |
| `Result::InvalidArg` | `path` 为空 |
| `Result::NotAttached` | 未附加会话 |
| `Result::WriteFault` | 路径写入目标失败 |
| `Result::Timeout` | 等待 DLL 加载超过 15 秒 |
| `Result::Error` | 无法解析 `LoadLibraryA` 或目标返回加载失败(路径不存在、位宽不符等) |
| `Result::AccessDenied` | 无创建远程线程/写内存权限 |

### 说明

在目标进程中远程分配内存写入 DLL 路径,创建线程调用 `LoadLibraryA`,等待最多 15 秒
返回模块基址。成功返回时 DLL 的 `DllMain` 已执行,`out.remote_base` 为模块基址、
`out.thread_id` 为执行线程。注入 64 位目标必须使用 64 位 DLL。记录写入
`injects.dat`,可用 `dll_list` 查询、`dll_eject` 卸载。目标为受保护进程时返回
`AccessDenied`。

前置条件:已 `attach(pid)`;目标非保护进程。后置条件:DLL 已加载;记录持久化。

### 示例

```cpp
deeptrace::InjectInfo info;
deeptrace::dll_inject("C:\\tools\\myhack.dll", info);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::dll_eject](#deeptracedll_eject)
- [deeptrace::dll_list](#deeptracedll_list)

---

## deeptrace::dll_eject

### 语法

```cpp
Result dll_eject(const std::string& path_or_addr);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `path_or_addr` | `const std::string&` | 注入记录中的 DLL 路径,或以 `0x` 开头的模块基址 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 已请求卸载并删除记录 |
| `Result::NotAttached` | 未附加会话 |
| `Result::NotFound` | 注入记录中不存在该 DLL/基址 |
| `Result::Error` | 无法解析 `FreeLibrary` 地址 |

### 说明

卸载先前注入的 DLL:创建远程线程调用 `FreeLibrary` 并删除 `injects.dat` 记录。
匹配方式:参数以 `0x`/`0X` 开头时按模块基址匹配,否则按路径精确匹配。注意
`FreeLibrary` 调用是异步发出,返回 `Ok` 不代表卸载线程已执行完毕。

前置条件:已 `attach(pid)`。后置条件:DLL 卸载请求已发出,记录已删除。

### 示例

```cpp
deeptrace::dll_eject("C:\\tools\\myhack.dll");
// 或 deeptrace::dll_eject("0x7ff600001000");
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::dll_inject](#deeptracedll_inject)

---

## deeptrace::dll_list

### 语法

```cpp
Result dll_list(std::vector<InjectInfo>& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `out` | `std::vector<InjectInfo>&` | 输出参数,本库注入的 DLL 记录 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 查询成功 |
| `Result::NotAttached` | 无会话 |

### 说明

列出 `injects.dat` 中本库注入的全部 DLL 记录。`running` 表示该 DLL 当前是否仍真实
加载于目标(实时比对模块列表);目标会话未附加句柄时 `running` 为 false。注意该列表
仅包含本库注入的 DLL,不含目标自行加载的模块(模块查询用 `module_list`)。

前置条件:已 `attach(pid)`(无句柄也可,状态字段不刷新)。后置条件:无。

### 示例

```cpp
std::vector<deeptrace::InjectInfo> list;
deeptrace::dll_list(list);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::dll_inject](#deeptracedll_inject)
- [deeptrace::module_list](MODULE.md#deeptracemodule_list)

---

## deeptrace::dll_status

### 语法

```cpp
Result dll_status(std::vector<InjectInfo>& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `out` | `std::vector<InjectInfo>&` | 输出参数,注入 DLL 状态列表 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 查询成功 |
| `Result::NotAttached` | 无会话 |

### 说明

`dll_list` 的别名,行为完全一致。保留用于语义区分「查询记录」与「查询状态」。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
std::vector<deeptrace::InjectInfo> list;
deeptrace::dll_status(list);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::dll_list](#deeptracedll_list)

---

## deeptrace::shellcode_inject

### 语法

```cpp
Result shellcode_inject(const std::vector<uint8_t>& bytes, InjectInfo& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `bytes` | `const std::vector<uint8_t>&` | Shellcode 机器码字节,非空 |
| `out` | `InjectInfo&` | 输出参数,注入结果(分配地址/线程) |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 已分配可执行内存并启动远程线程 |
| `Result::InvalidArg` | `bytes` 为空 |
| `Result::NotAttached` | 未附加会话 |
| `Result::WriteFault` | 载荷写入失败 |
| `Result::AccessDenied` | 无分配可执行内存/创建线程权限 |

### 说明

在目标进程中分配 `PAGE_EXECUTE_READWRITE` 内存,写入 Shellcode 字节并立即创建远程
线程执行。`out.remote_base` 为分配地址,`out.thread_id` 为执行线程。**不等待执行
结果**(Shellcode 通常无返回约定)。记录写入 `injects.dat`,可用 `shellcode_status`
查询线程是否仍在运行。可用 `asm_assemble` 生成载荷。

前置条件:已 `attach(pid)`。后置条件:Shellcode 已在目标中执行;记录持久化。

### 示例

```cpp
std::vector<uint8_t> code = {0x48, 0x31, 0xC0, 0xC3};  // xor rax,rax; ret
deeptrace::InjectInfo info;
deeptrace::shellcode_inject(code, info);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::shellcode_inject_at](#deeptraceshellcode_inject_at)
- [deeptrace::asm_assemble](ASM.md#deeptraceasm_assemble)

---

## deeptrace::shellcode_inject_at

### 语法

```cpp
Result shellcode_inject_at(uintptr_t addr, const std::vector<uint8_t>& bytes,
                           InjectInfo& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `addr` | `uintptr_t` | 目标进程内写入并执行地址,不允许为 0 |
| `bytes` | `const std::vector<uint8_t>&` | Shellcode 机器码字节 |
| `out` | `InjectInfo&` | 输出参数,注入结果 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 已写入并启动远程线程 |
| `Result::InvalidArg` | `bytes` 为空或 `addr == 0` |
| `Result::NotAttached` | 未附加会话 |
| `Result::WriteFault` | 写入失败(地址不可写/只读页) |
| `Result::AccessDenied` | 无创建远程线程权限 |

### 说明

将 Shellcode 写入目标进程**指定地址**(不自行分配)并从该地址启动远程线程执行。
写入前请确保目标地址可写且是合法的可执行内存(如已用 `VirtualAllocEx` 预留并调整为
可执行属性,或模块内空穴)。适合复用固定缓冲区、避免改变目标内存布局的场景。

前置条件:已 `attach(pid)`;目标地址可写。后置条件:Shellcode 已在目标中执行;记录持久化。

### 示例

```cpp
deeptrace::shellcode_inject_at(0x140001000, code, info);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::shellcode_inject](#deeptraceshellcode_inject)

---

## deeptrace::shellcode_status

### 语法

```cpp
Result shellcode_status(std::vector<InjectInfo>& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `out` | `std::vector<InjectInfo>&` | 输出参数,Shellcode 注入记录及运行状态 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 查询成功 |
| `Result::NotAttached` | 无会话 |

### 说明

列出 `injects.dat` 中的 Shellcode 注入记录,`running` 表示对应远程线程是否仍在运行
(通过 `GetExitCodeThread` 判断,`STILL_ACTIVE` 即运行中)。已结束的线程 `running`
为 false,但记录仍保留。

前置条件:已 `attach(pid)`(无句柄也可,状态字段不刷新)。后置条件:无。

### 示例

```cpp
std::vector<deeptrace::InjectInfo> list;
deeptrace::shellcode_status(list);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::shellcode_inject](#deeptraceshellcode_inject)

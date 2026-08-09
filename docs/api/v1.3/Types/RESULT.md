# Result 枚举

所有 deeptrace API 的返回值类型。定义于 `deeptrace.h`(经 `domain/types.h`)。

## 语法

```cpp
enum class Result {
    Ok = 0, Error, InvalidArg, NotAttached, NoSuchProcess, AccessDenied,
    ReadFault, WriteFault, NotFound, Timeout, NotSupported, AlreadyExists,
    NotExecutable, BadFormat
};
```

用 `deeptrace::result_message(r)` 获取可读描述字符串。

## 枚举值说明

| 枚举值 | 触发条件 |
|--------|----------|
| `Ok` | 操作成功。 |
| `Error` | 一般性失败:系统调用返回失败(`TerminateThread` 失败、`FreeLibrary` 前置地址缺失、`LoadLibraryA` 地址缺失、注入后返回基址为 0)、输出文件无法打开、值格式化失败等。 |
| `InvalidArg` | 参数非法:`pid==0`、`buf==nullptr`、`size==0`、`name` 为空、`out`/`out_pid`/`out_base` 等指针为 `nullptr`、`count==0` 或 `>10000`、`addr==0`、硬件断点 `type>2` 或 `length∉{1,2,4,8}`、`end<start`、范围/大小超过上限、特征码为空。 |
| `NotAttached` | 调用了需要会话(目标进程)的 API,但尚未 `attach()`。 |
| `NoSuchProcess` | 指定 pid 的进程不存在(`OpenProcess` 返回 NULL)。 |
| `AccessDenied` | 权限不足:附加目标进程、挂起/恢复/终止进程或线程、调试附加、注入时被系统拒绝。 |
| `ReadFault` | 远程读取失败或读取字节数不完整(`memory_dump`/`memory_readval` 要求一次读满,`disasm` 首块读取失败)。 |
| `WriteFault` | 远程写入失败或写入字节数不完整(注入路径/载荷写入、`memory_write` 底层失败)。 |
| `NotFound` | 找不到目标:模块名未匹配、断点/硬件断点不存在、监视索引越界、注入记录未找到、`module_dump` 目标模块不存在。 |
| `Timeout` | 等待超时:`dll_inject` 等待目标线程加载 DLL 超过 15 秒。 |
| `NotSupported` | 预留,当前实现不使用。 |
| `AlreadyExists` | 重复操作:已处于调试会话时再次 `debug_attach`、对同一地址重复设置断点/硬件断点。 |
| `NotExecutable` | 预留,当前实现不使用。 |
| `BadFormat` | 输入格式错误:`asm_assemble` 指令无法汇编、`pattern_scan` 特征码格式非法(非十六进制/非法通配符)。 |

## 使用示例

```cpp
#include "deeptrace.h"
#include <iostream>

int main() {
    uint32_t pid = 1234;
    deeptrace::Result r = deeptrace::attach(pid);
    if (r != deeptrace::Result::Ok) {
        std::cout << "attach failed: " << deeptrace::result_message(r) << "\n";
        return 1;
    }
    deeptrace::detach();
    return 0;
}
```

## 参见

- [deeptrace::result_message](../Modules/PROCESS.md#deeptraceresult_message)
- [GettingStarted](../GettingStarted.md)

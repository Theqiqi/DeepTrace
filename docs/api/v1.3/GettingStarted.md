# 快速入门

本页用一个完整示例带你走通 deeptrace 的核心流程:枚举进程 → 附加目标 → 读取内存 →
分离。示例可直接复制编译运行。

## 1. 工程准备

- 环境:Windows x64、MSVC(Visual Studio 2022)、C++20
- 头文件:`deeptrace/include/deeptrace.h`
- 库文件:`deeptrace/out/lib/<Debug|Release>/deeptrace.lib`
- 链接依赖:deeptrace 是静态库,汇编/反汇编依赖 Keystone/Capstone,**不合并依赖**,
  消费方需同时显式链接:
  - `deeptrace/out/build/<配置小写>/third_party/keystone/lib/keystone.lib`
  - `deeptrace/out/lib/<Debug|Release>/capstone.lib`

### 编译命令(MSVC)

```bat
cl /nologo /std:c++20 /EHsc /MDd /I deeptrace\include getting_started.cpp ^
   deeptrace\out\lib\Debug\deeptrace.lib ^
   deeptrace\out\build\debug\third_party\keystone\lib\keystone.lib ^
   deeptrace\out\lib\Debug\capstone.lib /link /out:getting_started.exe
```

> 建议先用项目自带脚本构建库:`deeptrace\script\build_debug.bat`
> (产物生成于 `deeptrace/out/lib/Debug/deeptrace.lib`)。

## 2. 使用流程

```cpp
#include "deeptrace.h"
#include <iostream>

int main() {
    // 1. 枚举进程,让用户选择目标
    std::vector<deeptrace::ProcessInfo> procs;
    if (deeptrace::enumerate_processes(procs) != deeptrace::Result::Ok) {
        std::cerr << "枚举进程失败\n";
        return 1;
    }
    uint32_t pid = 0;
    for (const auto& p : procs) {
        std::wcout << p.pid << L"  " << p.name << L"\n";
        if (pid == 0) pid = p.pid;  // 演示:取第一个进程
    }
    if (pid == 0) return 1;

    // 2. 附加目标进程(建立会话)
    if (deeptrace::attach(pid) != deeptrace::Result::Ok) {
        std::cerr << "附加失败(可能需要管理员权限)\n";
        return 1;
    }

    // 3. 读取目标内存
    uint32_t val = 0;
    size_t got = 0;
    if (deeptrace::memory_read(0x140000000, &val, sizeof val, &got) ==
        deeptrace::Result::Ok && got == sizeof val) {
        std::cout << "读到的值: 0x" << std::hex << val << "\n";
    }

    // 4. 分离会话(必须:调试模式下防止目标被连带终止)
    deeptrace::detach();
    return 0;
}
```

## 3. 流程要点

| 阶段 | API | 说明 |
|------|-----|------|
| 枚举 | `enumerate_processes` | 无需会话,列出目标候选 |
| 附加 | `attach(pid)` | 建立全局会话,后续操作的前提 |
| 读取 | `memory_read` / `memory_readval` / `memory_dump` | 需要已附加 |
| 分离 | `detach()` | 关闭会话;处于调试模式时自动安全分离 |

## 4. 下一步

- 读写与扫描:[Examples/read_write_memory.md](Examples/read_write_memory.md)
- 调试断点:[Examples/debug_breakpoints.md](Examples/debug_breakpoints.md)
- 完整 API 清单:[README.md](README.md)

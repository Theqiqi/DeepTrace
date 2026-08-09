# 数据结构(结构体)

定义于 `deeptrace.h`(经 `domain/types.h`)。所有结构体仅使用标准 C++ 类型,
不暴露 `windows.h` 类型;字段均有默认初始化值。

## ProcessInfo —— 进程信息

```cpp
struct ProcessInfo {
    uint32_t pid = 0;          // 进程 ID
    std::wstring name;         // 进程映像名(如 L"notepad.exe")
    uint32_t parent_pid = 0;   // 父进程 ID
    uint32_t thread_count = 0; // 线程数
};
```

## MemoryRegion —— 内存区域

```cpp
struct MemoryRegion {
    uintptr_t base = 0;      // 区域起始地址
    size_t size = 0;         // 区域大小(字节)
    uint32_t protection = 0; // 保护属性(PAGE_READONLY/PAGE_EXECUTE_READ 等,含 PAGE_GUARD 位)
    uint32_t state = 0;      // 状态(MEM_COMMIT/MEM_RESERVE/MEM_FREE)
    uint32_t type = 0;       // 类型(MEM_PRIVATE/MEM_MAPPED/MEM_IMAGE)
};
```

## ModuleInfo —— 模块信息

```cpp
struct ModuleInfo {
    uintptr_t base = 0;      // 模块基址
    size_t size = 0;         // 模块映像大小(字节)
    std::wstring name;       // 模块名(如 L"kernel32.dll")
    std::wstring path;       // 模块完整路径
};
```

## ExportInfo —— 导出符号

```cpp
struct ExportInfo {
    std::string name;    // 导出函数名(ASCII)
    uintptr_t address = 0; // 导出函数地址(模块基址 + RVA)
};
```

## ThreadInfo —— 线程信息

```cpp
struct ThreadInfo {
    uint32_t tid = 0;            // 线程 ID
    int32_t priority = 0;        // 线程优先级
    uintptr_t start_address = 0; // 线程入口地址
};
```

## RegisterInfo —— 寄存器值

```cpp
struct RegisterInfo {
    std::string name;  // 寄存器名("rax"、"rip"、"eflags" 等)
    uint64_t value = 0; // 寄存器值
};
```

## BreakpointInfo —— 断点信息

```cpp
struct BreakpointInfo {
    uintptr_t address = 0;                    // 断点地址
    BreakpointType type = BreakpointType::Software; // 断点类型
    uint8_t original_byte = 0;                // 软件断点:被覆盖的原字节(0xCC 之外的原始值)
    int32_t hw_index = -1;                    // 硬件断点:DR0-DR3 槽位(-1 表示非硬件断点)
};
```

## WatchEntry —— 监视项

```cpp
struct WatchEntry {
    uint32_t index = 0;             // 监视项索引(从 0 开始)
    std::string description;        // 描述文本
    uintptr_t address = 0;          // 监视地址
    ValueType type = ValueType::Dword; // 值类型
    std::string value;              // 当前值文本(如 "0x11223344");读取失败为 "??"
    bool valid = false;             // 是否成功读取到目标内存值
};
```

## Instruction —— 反汇编指令

```cpp
struct Instruction {
    uintptr_t address = 0;    // 指令地址
    std::vector<uint8_t> bytes; // 指令机器码
    std::string text;          // 反汇编文本(纯 ASCII,如 "mov rax, rbx")
};
```

## DebugStatus —— 调试状态

```cpp
struct DebugStatus {
    bool attached = false;      // 是否已附加(调试会话或进程会话)
    uint32_t pid = 0;           // 当前会话目标进程 ID
    uint32_t breakpoint_count = 0;     // 软件断点数量(已持久化记录)
    uint32_t hw_breakpoint_count = 0;  // 硬件断点数量(已持久化记录)
};
```

## InjectInfo —— 注入信息

```cpp
struct InjectInfo {
    std::wstring path;          // DLL 路径(dll 注入);shellcode 注入时为空
    uintptr_t remote_base = 0;  // 目标进程内的模块基址 / shellcode 分配地址
    uint32_t thread_id = 0;     // 执行注入的远程线程 ID
    bool running = false;       // DLL:是否仍加载;shellcode:远程线程是否仍在运行
    std::string kind;           // "dll" 或 "shellcode"
    size_t size = 0;            // 注入数据大小(dll:路径长度;shellcode:字节数)
};
```

## 参见

- [Result](RESULT.md)
- [ValueType / BreakpointType](ENUMS.md)
- [GettingStarted](../GettingStarted.md)

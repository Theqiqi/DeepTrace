# pmem 静态库 - 功能设计(API 接口列表)

> 本列表即 pmem 静态库公共 API 完整清单。所有类型使用标准 C++ 类型,
> 不暴露 windows.h 类型,保证 CLI 无需包含平台头文件。

## 1. 公共头文件

```cpp
#include "pmem.h"        // 唯一公共头文件,位于 pmem/include/
namespace pmem { ... }
```

## 2. 数据结构列表

| 结构体 | 字段 | 说明 |
|--------|------|------|
| `Result` (enum) | Ok/Error/InvalidArg/NotAttached/NoSuchProcess/AccessDenied/ReadFault/WriteFault/NotFound/Timeout/NotSupported/AlreadyExists/NotExecutable | 全部 API 返回 |
| `ProcessInfo` | pid, name(wstring), thread_count, parent_pid | 进程信息 |
| `MemoryRegion` | base, size, protection, state, type | 内存区域 |
| `ModuleInfo` | base, size, name(wstring), path(wstring) | 模块信息 |
| `ExportInfo` | name(string), address | 导出符号 |
| `ThreadInfo` | tid, priority, start_address | 线程信息 |
| `RegisterInfo` | name, value | 寄存器 |
| `BreakpointInfo` | address, type, original_byte, hw_index | 断点信息 |
| `WatchEntry` | index, description, address, type, value | 监视条目 |
| `InjectInfo` | path, remote_base, thread_id | DLL 注入信息 |
| `ShellcodeInfo` | address, thread_id, size, running | 壳码信息 |
| `Instruction` | address, bytes, text | 反汇编指令 |
| `ValueType` (enum) | Byte/Word/Dword/Qword/Float/Double | 类型化读取/监视类型 |
| `BreakpointType` (enum) | Software/Hardware/PageGuard | 断点类型 |

## 3. API 列表(按模块)

### 进程
```cpp
Result enumerate_processes(std::vector<ProcessInfo>& out);
Result attach(uint32_t pid);                      // 建立会话
Result detach();
Result process_info(uint32_t pid, ProcessInfo& out);
Result suspend_process(uint32_t pid);
Result resume_process(uint32_t pid);
Result terminate_process(uint32_t pid, uint32_t exit_code);
```

### 内存(会话已附加)
```cpp
Result memory_read(uintptr_t addr, void* buf, size_t size, size_t* out_read);
Result memory_write(uintptr_t addr, const void* buf, size_t size, size_t* out_written);
Result memory_dump(uintptr_t addr, size_t size, std::vector<uint8_t>& out);
Result memory_regions(std::vector<MemoryRegion>& out);
Result memory_readval(uintptr_t addr, ValueType type, std::string& out_text); // 格式化为文本
```

### 模块
```cpp
Result module_list(std::vector<ModuleInfo>& out);
Result module_find(const std::string& name, ModuleInfo& out);
Result module_base(const std::string& name, uintptr_t* out_base);
Result module_exports(const std::string& name, std::vector<ExportInfo>& out);
Result module_dump(const std::string& name, const std::string& output_file, std::string* out_hex);
```

### 线程
```cpp
Result thread_list(std::vector<ThreadInfo>& out);
Result thread_suspend(uint32_t tid);
Result thread_resume(uint32_t tid);
Result thread_terminate(uint32_t tid, uint32_t exit_code);
```

### 调试
```cpp
Result debug_attach();
Result debug_detach();
Result debug_pause();                             // 挂起全部线程
Result debug_resume();                            // 恢复全部线程
Result debug_step(uint32_t tid, uintptr_t* out_rip);
Result debug_step_over(uint32_t tid, uintptr_t* out_rip);
Result breakpoint_set(uintptr_t addr, BreakpointInfo& out);
Result breakpoint_clear(uintptr_t addr);
Result hw_breakpoint_set(uintptr_t addr, uint32_t type, uint32_t length);
Result hw_breakpoint_clear(uintptr_t addr);
Result guard_set(uintptr_t addr, size_t size);
Result guard_clear(uintptr_t addr, size_t size);
Result debug_status(DebugStatus& out);
Result registers_get(std::vector<RegisterInfo>& out, uint32_t tid);
Result register_get(const std::string& name, uint64_t* out_value, uint32_t tid);
```

### 反汇编
```cpp
Result disasm_at(uintptr_t addr, uint32_t count, std::vector<Instruction>& out);
Result disasm_range(uintptr_t start, uintptr_t end, std::vector<Instruction>& out);
```

### 解析
```cpp
Result resolve_base(const std::string& name, uintptr_t* out_base);   // = module_base
Result pattern_scan(const std::string& pattern, std::vector<uintptr_t>& out);
```

### 监视
```cpp
Result watch_list(std::vector<WatchEntry>& out);
Result watch_add(const std::string& desc, uintptr_t addr, ValueType type);
Result watch_remove(uint32_t index);
Result watch_refresh(std::vector<WatchEntry>& out);
Result watch_clear();
```

### DLL
```cpp
Result dll_inject(const std::string& path, InjectInfo& out);
Result dll_eject(const std::string& path_or_addr);
Result dll_list(std::vector<InjectInfo>& out);
Result dll_status(std::vector<InjectInfo>& out);
```

### 汇编
```cpp
Result asm_assemble(const std::string& code, std::vector<uint8_t>& out, std::string* out_text);
```

### 壳码
```cpp
Result shellcode_inject(const std::vector<uint8_t>& bytes, ShellcodeInfo& out);
Result shellcode_inject_at(uintptr_t addr, const std::vector<uint8_t>& bytes, ShellcodeInfo& out);
Result shellcode_status(std::vector<ShellcodeInfo>& out);
```

### 工具
```cpp
const char* result_message(Result r);             // ASCII 错误文本
Result session_pid(uint32_t* out_pid);            // 当前会话 PID
```

## 4. 实现分层

| 层 | 目录 | 职责 |
|----|------|------|
| 数据层 | src/data/ | 类型定义(types.h) |
| 算法层 | src/algorithm/ | 纯计算:hex、AOB 匹配、x64 解码/编码、格式化(无 WinAPI) |
| 原子化层 | src/atomic/ | WinAPI 最小封装:进程/内存/模块/线程/调试/注入句柄操作 |
| 接口层 | src/api/ | 公共 API:组装 算法+数据结构+原子化,错误码与状态持久化 |

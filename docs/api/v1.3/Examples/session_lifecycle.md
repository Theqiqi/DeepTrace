# 示例:会话生命周期

演示 deeptrace 最基础的流程:**枚举进程 → 附加 → 查询 → 分离**。
源码:[src/session_lifecycle.cpp](src/session_lifecycle.cpp)(可直接编译运行)。

## API 调用顺序

| 步骤 | API | 说明 |
|------|-----|------|
| 1 | `enumerate_processes` | 无需会话,获取进程候选列表 |
| 2 | `process_info` | 无需会话,按 pid 查询目标信息 |
| 3 | `attach(pid)` | 建立会话(后续操作的前提) |
| 4 | `session_pid` | 确认当前会话目标 |
| 5 | `detach()` | 关闭会话 |

## 代码

```cpp
// 完整代码见 src/session_lifecycle.cpp,关键片段:
std::vector<deeptrace::ProcessInfo> procs;
deeptrace::enumerate_processes(procs);              // 1. 枚举

if (deeptrace::process_info(pid, info) == deeptrace::Result::Ok) {
    std::printf("pid=%u threads=%u\n", info.pid, info.thread_count);
}                                                   // 2. 查询

if (deeptrace::attach(pid) != deeptrace::Result::Ok) return 1;  // 3. 附加

uint32_t cur = 0;
deeptrace::session_pid(&cur);                       // 4. 会话确认

deeptrace::detach();                                // 5. 分离
```

## 构建与运行

```bat
build_examples.bat                      rem 或直接:
cl /nologo /std:c++20 /EHsc /MDd /I deeptrace\include src\session_lifecycle.cpp ^
   deeptrace\out\lib\Debug\deeptrace.lib ^
   deeptrace\out\build\debug\third_party\keystone\lib\keystone.lib ^
   deeptrace\out\lib\Debug\capstone.lib /link /out:session_lifecycle.exe

session_lifecycle.exe 1234              rem 附加 pid 1234
```

> 注意:附加其他进程通常需要管理员权限;库的静态链接依赖见
> [GettingStarted](../GettingStarted.md)。

## 相关 API

- [attach](../Modules/PROCESS.md#deeptraceattach)
- [detach](../Modules/PROCESS.md#deeptracedetach)
- [enumerate_processes](../Modules/PROCESS.md#deeptraceenumerate_processes)

# Example: Session Lifecycle

Demonstrates the most basic deeptrace flow: **enumerate → attach → query → detach**.
Source: [src/session_lifecycle.cpp](src/session_lifecycle.cpp) (compilable and runnable directly).

## API Call Order

| Step | API | Description |
|------|-----|-------------|
| 1 | `enumerate_processes` | no session needed; get the candidate process list |
| 2 | `process_info` | no session needed; query target info by pid |
| 3 | `attach(pid)` | establish a session (prerequisite for later operations) |
| 4 | `session_pid` | confirm the current session target |
| 5 | `detach()` | close the session |

## Code

```cpp
// Full code in src/session_lifecycle.cpp; key excerpts:
std::vector<deeptrace::ProcessInfo> procs;
deeptrace::enumerate_processes(procs);              // 1. enumerate

if (deeptrace::process_info(pid, info) == deeptrace::Result::Ok) {
    std::printf("pid=%u threads=%u\n", info.pid, info.thread_count);
}                                                   // 2. query

if (deeptrace::attach(pid) != deeptrace::Result::Ok) return 1;  // 3. attach

uint32_t cur = 0;
deeptrace::session_pid(&cur);                       // 4. confirm session

deeptrace::detach();                                // 5. detach
```

## Build & Run

```bat
build_examples.bat                      rem or directly:
cl /nologo /std:c++20 /EHsc /MDd /I deeptrace\include src\session_lifecycle.cpp ^
   deeptrace\out\lib\Debug\deeptrace.lib ^
   deeptrace\out\build\debug\third_party\keystone\lib\keystone.lib ^
   deeptrace\out\lib\Debug\capstone.lib /link /out:session_lifecycle.exe

session_lifecycle.exe 1234              rem attach pid 1234
```

> Note: attaching to other processes usually requires administrator privileges; the library's static link dependencies are described in [GettingStarted](../GettingStarted.md).

## Related APIs

- [attach](../Modules/PROCESS.md#deeptraceattach)
- [detach](../Modules/PROCESS.md#deeptracedetach)
- [enumerate_processes](../Modules/PROCESS.md#deeptraceenumerate_processes)

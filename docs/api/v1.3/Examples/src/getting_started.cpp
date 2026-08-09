// deeptrace API example: GettingStarted quick start.
// enumerate -> attach -> memory_read -> detach
//
// Build (Windows x64, MSVC, C++20):
//   cl /nologo /std:c++20 /EHsc /MDd /I deeptrace\include getting_started.cpp ^
//      deeptrace\out\lib\Debug\deeptrace.lib ^
//      deeptrace\out\build\debug\third_party\keystone\lib\keystone.lib ^
//      deeptrace\out\lib\Debug\capstone.lib /link /out:getting_started.exe
// Run:
//   getting_started.exe [pid]

#include "deeptrace.h"

#include <iostream>
#include <vector>

int main() {
    // 1. enumerate processes, let the user choose a target
    std::vector<deeptrace::ProcessInfo> procs;
    if (deeptrace::enumerate_processes(procs) != deeptrace::Result::Ok) {
        std::cerr << "枚举进程失败\n";
        return 1;
    }
    uint32_t pid = 0;
    for (const auto& p : procs) {
        std::wcout << p.pid << L"  " << p.name << L"\n";
        if (pid == 0 && p.pid != 0) pid = p.pid;  // demo: pick first non-zero pid
    }
    if (pid == 0) return 1;

    // 2. attach to the target (establish session)
    if (deeptrace::attach(pid) != deeptrace::Result::Ok) {
        std::cerr << "附加失败(可能需要管理员权限)\n";
        return 1;
    }

    // 3. read target memory
    uint32_t val = 0;
    size_t got = 0;
    if (deeptrace::memory_read(0x140000000, &val, sizeof val, &got) ==
            deeptrace::Result::Ok &&
        got == sizeof val) {
        std::cout << "读到的值: 0x" << std::hex << val << "\n";
    }

    // 4. detach session (required: prevents debuggee termination in debug mode)
    deeptrace::detach();
    return 0;
}

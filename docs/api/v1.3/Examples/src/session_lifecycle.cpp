// deeptrace API example: session lifecycle.
// enumerate -> process_info -> attach -> session_pid -> detach
//
// Build (Windows x64, MSVC, C++20):
//   cl /nologo /std:c++20 /EHsc /MDd /I deeptrace\include session_lifecycle.cpp ^
//      deeptrace\out\lib\Debug\deeptrace.lib ^
//      deeptrace\out\build\debug\third_party\keystone\lib\keystone.lib ^
//      deeptrace\out\lib\Debug\capstone.lib /link /out:session_lifecycle.exe
// Run:
//   session_lifecycle.exe [pid]      (default: first enumerated process)

#include "deeptrace.h"

#include <cstdio>
#include <cstdlib>

int main(int argc, char** argv) {
    // 1. enumerate processes (no session required)
    std::vector<deeptrace::ProcessInfo> procs;
    if (deeptrace::enumerate_processes(procs) != deeptrace::Result::Ok) {
        std::fprintf(stderr, "enumerate_processes failed\n");
        return 1;
    }
    std::printf("processes: %zu\n", procs.size());

    // 2. choose target pid
    uint32_t pid = 0;
    if (argc > 1) pid = static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 0));
    if (pid == 0 && !procs.empty()) pid = procs[0].pid;
    if (pid == 0) {
        std::fprintf(stderr, "no target pid\n");
        return 1;
    }

    // 3. query process info without attaching
    deeptrace::ProcessInfo info;
    if (deeptrace::process_info(pid, info) == deeptrace::Result::Ok) {
        std::printf("pid=%u threads=%u\n", info.pid, info.thread_count);
    }

    // 4. attach (admin rights usually required)
    if (deeptrace::attach(pid) != deeptrace::Result::Ok) {
        std::fprintf(stderr, "attach failed (admin rights required?)\n");
        return 1;
    }
    uint32_t cur = 0;
    deeptrace::session_pid(&cur);
    std::printf("session pid=%u\n", cur);

    // 5. detach (safe even when in debug mode)
    deeptrace::detach();
    std::printf("done\n");
    return 0;
}

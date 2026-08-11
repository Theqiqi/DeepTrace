// deeptrace API example: debug session with software breakpoint + step.
//
// Build (Windows x64, MSVC, C++20):
//   cl /nologo /std:c++20 /EHsc /MDd /I deeptrace\include debug_breakpoints.cpp ^
//      deeptrace\out\lib\Debug\deeptrace.lib ^
//      deeptrace\out\build\debug\third_party\keystone\lib\keystone.lib ^
//      deeptrace\out\lib\Debug\capstone.lib /link /out:debug_breakpoints.exe
// Run (requires admin rights):
//   debug_breakpoints.exe <pid> [addr]

#include "deeptrace.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: debug_breakpoints <pid> [addr]\n");
        return 1;
    }
    uint32_t pid = static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 0));
    uintptr_t addr = argc > 2 ? std::strtoull(argv[2], nullptr, 0) : 0x140001000;

    if (deeptrace::attach(pid) != deeptrace::Result::Ok) {
        std::fprintf(stderr, "attach failed\n");
        return 1;
    }

    // registers do not require debug mode
    std::vector<deeptrace::RegisterInfo> regs;
    if (deeptrace::registers_get(regs, 0) == deeptrace::Result::Ok) {
        std::printf("registers: %zu\n", regs.size());
    }

    // enter debug mode (admin rights required)
    if (deeptrace::debug_attach() != deeptrace::Result::Ok) {
        std::fprintf(stderr, "debug_attach failed (admin rights required?)\n");
        deeptrace::detach();
        return 1;
    }

    // software breakpoint
    deeptrace::BreakpointInfo bp;
    if (deeptrace::breakpoint_set(addr, bp) == deeptrace::Result::Ok) {
        std::printf("breakpoint set at %#llx (original=%#x)\n",
                    (unsigned long long)addr, (unsigned)bp.original_byte);
    }

    // single step (one-shot attach/step/detach if not in debug mode)
    uintptr_t rip = 0;
    if (deeptrace::debug_step(0, &rip) == deeptrace::Result::Ok) {
        std::printf("stepped, rip=%#llx\n", (unsigned long long)rip);
    }

    // restore the target: clear breakpoint, leave debug mode, close session
    deeptrace::breakpoint_clear(addr);
    deeptrace::debug_detach();
    deeptrace::detach();
    return 0;
}

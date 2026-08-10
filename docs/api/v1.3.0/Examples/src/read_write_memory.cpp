// deeptrace API example: remote memory read/write + pattern scan.
//
// Build (Windows x64, MSVC, C++20):
//   cl /nologo /std:c++20 /EHsc /MDd /I deeptrace\include read_write_memory.cpp ^
//      deeptrace\out\lib\Debug\deeptrace.lib ^
//      deeptrace\out\build\debug\third_party\keystone\lib\keystone.lib ^
//      deeptrace\out\lib\Debug\capstone.lib /link /out:read_write_memory.exe
// Run:
//   read_write_memory.exe <pid>

#include "deeptrace.h"

#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: read_write_memory <pid>\n");
        return 1;
    }
    uint32_t pid = static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 0));

    if (deeptrace::attach(pid) != deeptrace::Result::Ok) {
        std::fprintf(stderr, "attach failed\n");
        return 1;
    }

    // typed read of a dword
    std::string text;
    if (deeptrace::memory_readval(0x140000000, deeptrace::ValueType::Dword, text) ==
        deeptrace::Result::Ok) {
        std::printf("readval: %s\n", text.c_str());
    }

    // write back
    uint32_t v = 0xCAFEBABE;
    size_t written = 0;
    if (deeptrace::memory_write(0x140000000, &v, sizeof v, &written) ==
        deeptrace::Result::Ok) {
        std::printf("wrote %zu bytes\n", written);
    }

    // pattern scan (?? matches any byte)
    std::vector<uintptr_t> hits;
    if (deeptrace::pattern_scan("DE AD BE EF", hits) == deeptrace::Result::Ok) {
        std::printf("hits: %zu\n", hits.size());
    }

    deeptrace::detach();
    return 0;
}

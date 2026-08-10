#include "service/resolve.h"
#include "service/module.h"
#include "service/session.h"
#include "algorithm/scan.h"
#include "infrastructure/memory/memory.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>
#include <vector>

namespace deeptrace {

Result resolve_base(const std::string& name, uintptr_t* out_base) {
    return module_base(name, out_base);
}

Result pattern_scan(const std::string& pattern, std::vector<uintptr_t>& out) {
    out.clear();
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;

    std::vector<internal::PatternByte> pat;
    if (!internal::parse_pattern(pattern, pat)) return Result::BadFormat;
    if (pat.empty()) return Result::InvalidArg;

    std::vector<MemoryRegion> regions;
    Result r = internal::EnumMemoryRegions(s.handle, regions);
    if (r != Result::Ok) return r;

    const size_t CHUNK = 1u << 20;
    const size_t plen = pat.size();
    std::vector<uint8_t> buf;

    for (const auto& region : regions) {
        if (region.state != MEM_COMMIT) continue;
        uint32_t prot = region.protection & 0xFF;
        if (prot == PAGE_NOACCESS) continue;
        if ((prot & PAGE_GUARD) != 0) continue;
        bool readable = (prot & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                                 PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                                 PAGE_EXECUTE_WRITECOPY)) != 0;
        if (!readable) continue;

        size_t region_size = region.size;
        uintptr_t base = region.base;
        size_t offset = 0;
        // overlap handling across chunk boundaries
        size_t overlap = plen > 1 ? plen - 1 : 0;
        std::vector<uint8_t> carry;  // tail of previous chunk
        while (offset < region_size) {
            size_t want = region_size - offset;
            if (want > CHUNK) want = CHUNK;
            buf.assign(want + overlap, 0);
            Result err;
            size_t got = internal::ReadRemoteMemory(s.handle, base + offset, buf.data(),
                                                    want, &err);
            if (err != Result::Ok || got == 0) break;
            // copy carry from previous read to handle cross-chunk matches
            if (!carry.empty() && carry.size() <= want) {
                std::memcpy(buf.data(), carry.data(), carry.size());
            }
            std::vector<size_t> hits =
                internal::scan_bytes(buf.data(), got, pat);
            for (size_t h : hits) {
                if (h + plen <= got) {
                    out.push_back(base + offset + h);
                }
            }
            offset += got;
            // save tail for next chunk
            carry.assign(buf.begin() + (got > overlap ? got - overlap : 0),
                         buf.begin() + got);
            if (got < want) break;
        }
    }
    return Result::Ok;
}

}  // namespace deeptrace

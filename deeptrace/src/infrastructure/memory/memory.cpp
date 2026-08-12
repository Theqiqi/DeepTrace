#include "infrastructure/memory/memory.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace deeptrace::internal {

size_t ReadRemoteMemory(void* hprocess, uintptr_t addr, void* buf, size_t size, Result* err) {
    SIZE_T done = 0;
    BOOL ok = ::ReadProcessMemory(static_cast<HANDLE>(hprocess),
                                  reinterpret_cast<LPCVOID>(addr), buf, size, &done);
    *err = ok ? Result::Ok : Result::ReadFault;
    return done;
}

size_t WriteRemoteMemory(void* hprocess, uintptr_t addr, const void* buf, size_t size, Result* err) {
    SIZE_T done = 0;
    BOOL ok = ::WriteProcessMemory(static_cast<HANDLE>(hprocess),
                                   reinterpret_cast<LPVOID>(addr), buf, size, &done);
    *err = ok ? Result::Ok : Result::WriteFault;
    return done;
}

Result EnumMemoryRegions(void* hprocess, std::vector<MemoryRegion>& out) {
    out.clear();
    uintptr_t addr = 0;
    while (addr < 0x7FFFFFFFFFFFULL) {
        MEMORY_BASIC_INFORMATION mbi;
        SIZE_T r = ::VirtualQueryEx(static_cast<HANDLE>(hprocess),
                                    reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi));
        if (r == 0) break;
        MemoryRegion region;
        region.base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        region.size = mbi.RegionSize;
        region.protection = mbi.Protect;
        region.state = mbi.State;
        region.type = mbi.Type;
        out.push_back(region);
        if (mbi.RegionSize == 0) break;
        addr = region.base + region.size;
    }
    return Result::Ok;
}

Result QueryRegion(void* hprocess, uintptr_t addr, MemoryRegion& out) {
    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T r = ::VirtualQueryEx(static_cast<HANDLE>(hprocess),
                                reinterpret_cast<LPCVOID>(addr), &mbi, sizeof(mbi));
    if (r == 0) return Result::ReadFault;
    out.base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    out.size = mbi.RegionSize;
    out.protection = mbi.Protect;
    out.state = mbi.State;
    out.type = mbi.Type;
    return Result::Ok;
}

Result RemoteAlloc(void* hprocess, size_t size, uint32_t protection, uintptr_t* out_addr) {
    void* p = ::VirtualAllocEx(static_cast<HANDLE>(hprocess), nullptr, size,
                               MEM_COMMIT | MEM_RESERVE, protection);
    if (!p) return Result::Error;
    *out_addr = reinterpret_cast<uintptr_t>(p);
    return Result::Ok;
}

Result RemoteFree(void* hprocess, uintptr_t addr) {
    BOOL ok = ::VirtualFreeEx(static_cast<HANDLE>(hprocess),
                              reinterpret_cast<LPVOID>(addr), 0, MEM_RELEASE);
    return ok ? Result::Ok : Result::Error;
}

namespace {

constexpr uintptr_t kNearDisp = 0x7FFFFFFF;   // RIP-relative rel32 limit (+-2GB)
constexpr uintptr_t kGranularity = 0x10000;   // allocation granularity (64KB)

inline uintptr_t align_up(uintptr_t v) { return (v + kGranularity - 1) & ~(kGranularity - 1); }
inline uintptr_t align_down(uintptr_t v) { return v & ~(kGranularity - 1); }

}  // namespace

Result RemoteAllocNear(void* hprocess, size_t size, uint32_t protection,
                       uintptr_t anchor, uintptr_t* out_addr) {
    const uintptr_t high =
        (anchor > UINTPTR_MAX - kNearDisp) ? UINTPTR_MAX : anchor + kNearDisp;
    const uintptr_t low = (anchor < kNearDisp) ? 0 : anchor - kNearDisp;

    auto try_alloc = [&](uintptr_t cand) -> uintptr_t {
        if (cand < low || cand > high || cand == 0) return 0;
        if (cand + size < cand || cand + size - 1 > high) return 0;
        void* p = ::VirtualAllocEx(static_cast<HANDLE>(hprocess),
                                   reinterpret_cast<LPVOID>(cand), size,
                                   MEM_COMMIT | MEM_RESERVE, protection);
        return reinterpret_cast<uintptr_t>(p);
    };

    // Upward pass: nearest free region at/above the anchor (a free region
    // containing the anchor yields the closest possible placement).
    uintptr_t pos = anchor;
    while (pos <= high) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!::VirtualQueryEx(static_cast<HANDLE>(hprocess),
                              reinterpret_cast<LPCVOID>(pos), &mbi, sizeof(mbi)))
            break;
        uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        uintptr_t end = base + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            uintptr_t cand = align_up(base > anchor ? base : anchor);
            if (cand >= base && cand + size <= end) {
                uintptr_t p = try_alloc(cand);
                if (p) {
                    *out_addr = p;
                    return Result::Ok;
                }
            }
        }
        if (end <= pos) break;  // no-progress guard
        if (end > high) break;  // window ceiling reached
        pos = end;
    }

    // Downward pass: nearest free region below the anchor (or the portion of
    // the anchor's own region below it, if the upward alignment did not fit).
    pos = anchor;
    while (pos >= low) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!::VirtualQueryEx(static_cast<HANDLE>(hprocess),
                              reinterpret_cast<LPCVOID>(pos), &mbi, sizeof(mbi)))
            break;
        uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        uintptr_t end = base + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            uintptr_t top = (end < anchor) ? end : anchor;
            uintptr_t cand = align_down(top - size);
            uintptr_t floor = base > low ? base : low;
            if (cand >= floor && cand + size <= top && cand + size - 1 >= low) {
                uintptr_t p = try_alloc(cand);
                if (p) {
                    *out_addr = p;
                    return Result::Ok;
                }
            }
        }
        if (base == 0) break;
        pos = base - 1;
    }
    return Result::Error;
}

Result ProtectRegion(void* hprocess, uintptr_t addr, size_t size, uint32_t new_prot,
                     uint32_t* old_prot) {
    DWORD old = 0;
    BOOL ok = ::VirtualProtectEx(static_cast<HANDLE>(hprocess),
                                 reinterpret_cast<LPVOID>(addr), size, new_prot, &old);
    if (!ok) return Result::Error;
    if (old_prot) *old_prot = old;
    return Result::Ok;
}

}  // namespace deeptrace::internal

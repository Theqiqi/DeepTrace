#include "infrastructure/memory/memory.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace pmem::internal {

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

Result ProtectRegion(void* hprocess, uintptr_t addr, size_t size, uint32_t new_prot,
                     uint32_t* old_prot) {
    DWORD old = 0;
    BOOL ok = ::VirtualProtectEx(static_cast<HANDLE>(hprocess),
                                 reinterpret_cast<LPVOID>(addr), size, new_prot, &old);
    if (!ok) return Result::Error;
    if (old_prot) *old_prot = old;
    return Result::Ok;
}

}  // namespace pmem::internal

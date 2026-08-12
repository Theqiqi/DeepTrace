#pragma once
#include "domain/types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace deeptrace::internal {

// Read/write remote memory. Returns bytes transferred; sets err.
size_t ReadRemoteMemory(void* hprocess, uintptr_t addr, void* buf, size_t size, Result* err);
size_t WriteRemoteMemory(void* hprocess, uintptr_t addr, const void* buf, size_t size, Result* err);

// Enumerate committed/reserved regions of the process.
Result EnumMemoryRegions(void* hprocess, std::vector<MemoryRegion>& out);

// Read a region's protection and state.
Result QueryRegion(void* hprocess, uintptr_t addr, MemoryRegion& out);

// VirtualAllocEx / VirtualFreeEx wrappers.
Result RemoteAlloc(void* hprocess, size_t size, uint32_t protection, uintptr_t* out_addr);
Result RemoteFree(void* hprocess, uintptr_t addr);

// Allocate within +/-2GB of an anchor address (RIP-relative rel32 range),
// preferring the free region closest to the anchor. Scans MEM_FREE regions
// upward then downward via VirtualQueryEx. Returns Error when no free region
// in the window fits `size` (never falls back to arbitrary placement).
Result RemoteAllocNear(void* hprocess, size_t size, uint32_t protection,
                       uintptr_t anchor, uintptr_t* out_addr);

// VirtualProtectEx wrapper.
Result ProtectRegion(void* hprocess, uintptr_t addr, size_t size, uint32_t new_prot, uint32_t* old_prot);

}  // namespace deeptrace::internal

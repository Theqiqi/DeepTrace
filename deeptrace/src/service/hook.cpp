#include "service/hook.h"
#include "service/session.h"
#include "service/store.h"
#include "infrastructure/memory/memory.h"

#include <vector>

namespace deeptrace {

namespace {

// Encode `jmp rel32` (E9 <rel32>), where rel = newmem - (addr + 5).
std::vector<uint8_t> encode_jmp_rel32(uintptr_t addr, uintptr_t newmem) {
    int64_t rel = static_cast<int64_t>(newmem) - (static_cast<int64_t>(addr) + 5);
    std::vector<uint8_t> bytes;
    bytes.push_back(0xE9);
    for (int i = 0; i < 4; ++i) {
        bytes.push_back(static_cast<uint8_t>((static_cast<uint64_t>(rel) >> (8 * i)) & 0xFF));
    }
    return bytes;
}

}  // namespace

Result hook_set(uintptr_t addr, uintptr_t newmem, HookInfo& out) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    if (addr == 0 || newmem == 0) return Result::InvalidArg;

    // Read the original 5 bytes at the target before patching.
    std::vector<uint8_t> orig(5, 0);
    Result err;
    size_t got = internal::ReadRemoteMemory(s.handle, addr, orig.data(), 5, &err);
    if (err != Result::Ok) return err;
    if (got != 5) return Result::ReadFault;

    std::vector<uint8_t> patch = encode_jmp_rel32(addr, newmem);
    size_t written = 0;
    err = Result::Ok;
    written = internal::WriteRemoteMemory(s.handle, addr, patch.data(), patch.size(), &err);
    if (err != Result::Ok || written != patch.size()) {
        return err != Result::Ok ? err : Result::WriteFault;
    }

    // Record the hook (target -> original bytes) per PID.
    auto recs = internal::load_hooks(s.pid);
    bool replaced = false;
    for (auto& h : recs) {
        if (h.target == addr) {
            h.newmem = newmem;
            h.orig_bytes = orig;
            h.size = 5;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        internal::HookRecord rec;
        rec.target = addr;
        rec.newmem = newmem;
        rec.orig_bytes = orig;
        rec.size = 5;
        recs.push_back(std::move(rec));
    }
    if (!internal::save_hooks(s.pid, recs)) return Result::Error;

    out.target = addr;
    out.newmem = newmem;
    out.orig_bytes = orig;
    out.size = 5;
    return Result::Ok;
}

Result hook_clear(uintptr_t addr) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    if (addr == 0) return Result::InvalidArg;

    auto recs = internal::load_hooks(s.pid);
    const internal::HookRecord* rec = nullptr;
    for (const auto& h : recs) {
        if (h.target == addr) {
            rec = &h;
            break;
        }
    }
    if (!rec) return Result::NotFound;

    // Restore the original bytes first; on write failure keep the record so
    // the target is never left half-restored with no way to retry.
    size_t written = 0;
    Result err = Result::Ok;
    written = internal::WriteRemoteMemory(s.handle, addr, rec->orig_bytes.data(),
                                          rec->orig_bytes.size(), &err);
    if (err != Result::Ok || written != rec->orig_bytes.size()) {
        return err != Result::Ok ? err : Result::WriteFault;
    }

    std::vector<internal::HookRecord> rest;
    for (const auto& h : recs) {
        if (h.target == addr) continue;
        rest.push_back(h);
    }
    return internal::save_hooks(s.pid, rest) ? Result::Ok : Result::Error;
}

}  // namespace deeptrace

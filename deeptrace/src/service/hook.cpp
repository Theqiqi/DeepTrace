#include "service/hook.h"
#include "service/session.h"
#include "service/store.h"
#include "infrastructure/assembly/asmenc.h"
#include "infrastructure/memory/memory.h"

#include <vector>

namespace deeptrace {

Result hook_set(uintptr_t addr, uintptr_t newmem, const std::string& owner,
                HookInfo& out) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    if (addr == 0 || newmem == 0) return Result::InvalidArg;

    // Load the hook record first. If this target is already hooked, the saved
    // original bytes must be preserved (re-reading them now would capture the
    // patch itself, breaking hook_clear on repeated set).
    auto recs = internal::load_hooks(s.pid);
    std::vector<uint8_t> orig(5, 0);
    internal::HookRecord* existing = nullptr;
    for (auto& h : recs) {
        if (h.target == addr) {
            existing = &h;
            break;
        }
    }
    if (existing) {
        orig = existing->orig_bytes;
    } else {
        // Read the original 5 bytes at the target before patching.
        Result err;
        size_t got = internal::ReadRemoteMemory(s.handle, addr, orig.data(), 5, &err);
        if (err != Result::Ok) return err;
        if (got != 5) return Result::ReadFault;
    }

    std::vector<uint8_t> patch = internal::encode_jmp_rel32(addr, newmem);
    size_t written = 0;
    Result err = Result::Ok;
    written = internal::WriteRemoteMemory(s.handle, addr, patch.data(), patch.size(), &err);
    if (err != Result::Ok || written != patch.size()) {
        return err != Result::Ok ? err : Result::WriteFault;
    }

    // Persist the hook record. On save failure, roll the patch back so the
    // target is never left patched without a recoverable record.
    if (existing) {
        existing->newmem = newmem;
        existing->size = 5;
        if (existing->owner.empty()) existing->owner = owner;
    } else {
        internal::HookRecord rec;
        rec.target = addr;
        rec.newmem = newmem;
        rec.orig_bytes = orig;
        rec.size = 5;
        rec.owner = owner;
        recs.push_back(std::move(rec));
    }
    if (!internal::save_hooks(s.pid, recs)) {
        internal::WriteRemoteMemory(s.handle, addr, orig.data(), orig.size(), &err);
        return Result::Error;
    }

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

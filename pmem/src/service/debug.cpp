#include "service/debug.h"
#include "service/session.h"
#include "service/store.h"
#include "infrastructure/disassembly/disasm.h"
#include "infrastructure/debug/debug.h"
#include "infrastructure/memory/memory.h"
#include "infrastructure/process/process.h"
#include "infrastructure/thread/thread.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>

namespace pmem {

namespace {

bool session_attached() {
    auto& s = internal::session();
    return s.handle != nullptr && s.pid != 0;
}

}  // namespace

Result debug_attach() {
    if (!session_attached()) return Result::NotAttached;
    auto& s = internal::session();
    if (s.debug_mode) return Result::AlreadyExists;
    if (internal::DebugAttachProcess(s.pid) != Result::Ok) return Result::AccessDenied;
    s.debug_mode = true;
    return Result::Ok;
}

Result debug_detach() {
    auto& s = internal::session();
    if (!s.debug_mode) return Result::NotAttached;
    if (internal::DebugDetachProcess(s.pid) != Result::Ok) return Result::Error;
    s.debug_mode = false;
    return Result::Ok;
}

Result debug_pause() {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    return internal::SuspendProcessThreads(s.pid);
}

Result debug_resume() {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    return internal::ResumeProcessThreads(s.pid);
}

namespace {

// One-shot single step: attach, step, detach. The debug session is per-CLI
// invocation (non-interactive), so each step re-attaches as needed.
Result step_once(uint32_t pid, uint32_t tid, uintptr_t* out_rip) {
    Result r = internal::DebugAttachProcess(pid);
    if (r != Result::Ok) return Result::AccessDenied;
    Result step = internal::DebugSingleStep(pid, tid, out_rip);
    internal::DebugDetachProcess(pid);
    return step;
}

}  // namespace

Result debug_step(uint32_t tid, uintptr_t* out_rip) {
    if (!session_attached()) return Result::NotAttached;
    auto& s = internal::session();
    if (!s.debug_mode) return step_once(s.pid, tid, out_rip);
    return internal::DebugSingleStep(s.pid, tid, out_rip);
}

Result debug_step_over(uint32_t tid, uintptr_t* out_rip) {
    if (!session_attached()) return Result::NotAttached;
    auto& s = internal::session();
    if (!s.debug_mode) {
        Result r = internal::DebugAttachProcess(s.pid);
        if (r != Result::Ok) return Result::AccessDenied;
        Result step = internal::DebugStepOver(s.pid, tid, out_rip);
        internal::DebugDetachProcess(s.pid);
        return step;
    }
    return internal::DebugStepOver(s.pid, tid, out_rip);
}

Result breakpoint_set(uintptr_t addr, BreakpointInfo& out) {
    if (!session_attached()) return Result::NotAttached;
    auto& s = internal::session();
    if (addr == 0) return Result::InvalidArg;

    auto sw = internal::load_sw_breaks(s.pid);
    for (const auto& b : sw) {
        if (b.address == addr) return Result::AlreadyExists;
    }

    uint8_t orig = 0;
    Result r = internal::ReadByte(s.handle, addr, &orig);
    if (r != Result::Ok) return r;

    r = internal::WriteByte(s.handle, addr, 0xCC);
    if (r != Result::Ok) return r;

    sw.push_back(internal::SwBreakRecord{addr, orig});
    internal::save_sw_breaks(s.pid, sw);

    out.address = addr;
    out.type = BreakpointType::Software;
    out.original_byte = orig;
    return Result::Ok;
}

Result breakpoint_clear(uintptr_t addr) {
    if (!session_attached()) return Result::NotAttached;
    auto& s = internal::session();

    auto sw = internal::load_sw_breaks(s.pid);
    bool found = false;
    uint8_t orig = 0;
    std::vector<internal::SwBreakRecord> rest;
    for (const auto& b : sw) {
        if (b.address == addr) {
            found = true;
            orig = b.original;
        } else {
            rest.push_back(b);
        }
    }
    if (!found) return Result::NotFound;

    Result r = internal::WriteByte(s.handle, addr, orig);
    if (r != Result::Ok) return r;
    internal::save_sw_breaks(s.pid, rest);
    return Result::Ok;
}

Result hw_breakpoint_set(uintptr_t addr, uint32_t type, uint32_t length) {
    if (!session_attached()) return Result::NotAttached;
    auto& s = internal::session();
    if (addr == 0) return Result::InvalidArg;
    if (type > 2 || (length != 1 && length != 2 && length != 4 && length != 8))
        return Result::InvalidArg;

    auto hw = internal::load_hw_breaks(s.pid);
    for (const auto& b : hw) {
        if (b.address == addr) return Result::AlreadyExists;
    }

    // find free DR slot
    int slot = -1;
    for (int i = 0; i < 4; ++i) {
        bool used = false;
        for (const auto& b : hw)
            if (b.index == i) used = true;
        if (!used) { slot = i; break; }
    }
    if (slot < 0) return Result::Error;

    // set DR slot on all threads
    std::vector<ThreadInfo> threads;
    Result r = internal::EnumThreads(s.pid, threads);
    if (r != Result::Ok) return r;
    for (const auto& t : threads) {
        HANDLE ht = ::OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, t.tid);
        if (!ht) continue;
        CONTEXT ctx;
        std::memset(&ctx, 0, sizeof(ctx));
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (::GetThreadContext(ht, &ctx)) {
            *(&ctx.Dr0 + slot) = addr;
            // DR7: enable local breakpoint, type in bits (16+4*slot), len in bits (18+4*slot)
            uint64_t type_bits = static_cast<uint64_t>(type) << (16 + 4 * slot);
            uint64_t len_bits = 0;
            if (length == 2) len_bits = 1;
            else if (length == 8) len_bits = 2;
            else if (length == 4) len_bits = 3;
            len_bits = len_bits << (18 + 4 * slot);
            ctx.Dr7 |= (1ULL << (2 * slot)) | type_bits | len_bits;
            ::SetThreadContext(ht, &ctx);
        }
        ::CloseHandle(ht);
    }

    hw.push_back(internal::HwBreakRecord{addr, slot});
    internal::save_hw_breaks(s.pid, hw);
    return Result::Ok;
}

Result hw_breakpoint_clear(uintptr_t addr) {
    if (!session_attached()) return Result::NotAttached;
    auto& s = internal::session();

    auto hw = internal::load_hw_breaks(s.pid);
    int slot = -1;
    std::vector<internal::HwBreakRecord> rest;
    for (const auto& b : hw) {
        if (b.address == addr) slot = b.index;
        else rest.push_back(b);
    }
    if (slot < 0) return Result::NotFound;

    std::vector<ThreadInfo> threads;
    Result r = internal::EnumThreads(s.pid, threads);
    if (r != Result::Ok) return r;
    for (const auto& t : threads) {
        HANDLE ht = ::OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, t.tid);
        if (!ht) continue;
        CONTEXT ctx;
        std::memset(&ctx, 0, sizeof(ctx));
        ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (::GetThreadContext(ht, &ctx)) {
            *(&ctx.Dr0 + slot) = 0;
            ctx.Dr7 &= ~(1ULL << (2 * slot));
            ctx.Dr7 &= ~(0xFULL << (16 + 4 * slot));
            ::SetThreadContext(ht, &ctx);
        }
        ::CloseHandle(ht);
    }

    internal::save_hw_breaks(s.pid, rest);
    return Result::Ok;
}

Result guard_set(uintptr_t addr, size_t size) {
    if (!session_attached()) return Result::NotAttached;
    auto& s = internal::session();
    uint32_t old = 0;
    Result r = internal::ProtectRegion(s.handle, addr, size,
                                       PAGE_EXECUTE_READWRITE | PAGE_GUARD, &old);
    return r;
}

Result guard_clear(uintptr_t addr, size_t size) {
    if (!session_attached()) return Result::NotAttached;
    auto& s = internal::session();
    uint32_t old = 0;
    Result r = internal::ProtectRegion(s.handle, addr, size, PAGE_EXECUTE_READWRITE, &old);
    return r;
}

Result debug_status(DebugStatus& out) {
    auto& s = internal::session();
    out.attached = s.debug_mode || (s.handle != nullptr);
    out.pid = s.pid;
    out.breakpoint_count = static_cast<uint32_t>(internal::load_sw_breaks(s.pid).size());
    out.hw_breakpoint_count = static_cast<uint32_t>(internal::load_hw_breaks(s.pid).size());
    return Result::Ok;
}

Result registers_get(std::vector<RegisterInfo>& out, uint32_t tid) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    return internal::GetThreadRegisters(s.pid, tid, out);
}

Result register_get(const std::string& name, uint64_t* out_value, uint32_t tid) {
    if (!out_value) return Result::InvalidArg;
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    return internal::GetRegisterValue(s.pid, tid, name, out_value);
}

}  // namespace pmem

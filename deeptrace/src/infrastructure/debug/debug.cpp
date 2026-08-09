#include "infrastructure/debug/debug.h"
#include "infrastructure/memory/memory.h"
#include "infrastructure/process/process.h"
#include "infrastructure/thread/thread.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstring>
#include <string>

namespace deeptrace::internal {

namespace {

struct RegSlot {
    const char* name;
    size_t offset;
    bool is_eflags;
};

// Offsets are into a CONTEXT structure.
#define OFF(field) offsetof(CONTEXT, field)

const RegSlot kRegs[] = {
    {"rax", OFF(Rax), false},   {"rbx", OFF(Rbx), false},
    {"rcx", OFF(Rcx), false},   {"rdx", OFF(Rdx), false},
    {"rsi", OFF(Rsi), false},   {"rdi", OFF(Rdi), false},
    {"rbp", OFF(Rbp), false},   {"rsp", OFF(Rsp), false},
    {"r8", OFF(R8), false},     {"r9", OFF(R9), false},
    {"r10", OFF(R10), false},   {"r11", OFF(R11), false},
    {"r12", OFF(R12), false},   {"r13", OFF(R13), false},
    {"r14", OFF(R14), false},   {"r15", OFF(R15), false},
    {"rip", OFF(Rip), false},   {"eflags", OFF(EFlags), true},
};

#undef OFF

}  // namespace

Result GetThreadRegisters(uint32_t pid, uint32_t tid, std::vector<RegisterInfo>& out) {
    out.clear();
    Result err = Result::Ok;
    HANDLE h = static_cast<HANDLE>(OpenThreadById(pid, tid, THREAD_GET_CONTEXT, &err));
    if (!h) return err;
    CONTEXT ctx;
    std::memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
    if (!::GetThreadContext(h, &ctx)) {
        ::CloseHandle(h);
        return Result::Error;
    }
    for (const auto& r : kRegs) {
        uint64_t v;
        std::memcpy(&v, reinterpret_cast<const uint8_t*>(&ctx) + r.offset, 8);
        RegisterInfo ri;
        ri.name = r.name;
        ri.value = v;
        out.push_back(ri);
    }
    ::CloseHandle(h);
    return Result::Ok;
}

Result GetRegisterValue(uint32_t pid, uint32_t tid, const std::string& name,
                        uint64_t* out) {
    for (const auto& r : kRegs) {
        if (name == r.name) {
            std::vector<RegisterInfo> regs;
            Result res = GetThreadRegisters(pid, tid, regs);
            if (res != Result::Ok) return res;
            for (const auto& ri : regs) {
                if (ri.name == name) {
                    *out = ri.value;
                    return Result::Ok;
                }
            }
            return Result::NotFound;
        }
    }
    return Result::NotFound;
}

Result ReadByte(void* hprocess, uintptr_t addr, uint8_t* out) {
    Result err;
    size_t n = ReadRemoteMemory(hprocess, addr, out, 1, &err);
    if (err != Result::Ok || n != 1) return err != Result::Ok ? err : Result::ReadFault;
    return Result::Ok;
}

Result WriteByte(void* hprocess, uintptr_t addr, uint8_t value) {
    Result err;
    size_t n = WriteRemoteMemory(hprocess, addr, &value, 1, &err);
    if (err != Result::Ok || n != 1) return err != Result::Ok ? err : Result::WriteFault;
    return Result::Ok;
}

Result DebugAttachProcess(uint32_t pid) {
    return ::DebugActiveProcess(pid) ? Result::Ok : Result::Error;
}

Result DebugDetachProcess(uint32_t pid) {
    return ::DebugActiveProcessStop(pid) ? Result::Ok : Result::Error;
}

Result DebugSingleStep(uint32_t pid, uint32_t tid, uintptr_t* out_rip) {
    uint32_t resolved = tid;
    Result r = ResolveTid(pid, tid, &resolved);
    if (r != Result::Ok) return r;

    // Wait for the first debug event (CREATE_PROCESS_DEBUG_EVENT). While this
    // event is pending, all threads of the debuggee are frozen.
    DEBUG_EVENT de;
    for (;;) {
        if (!::WaitForDebugEvent(&de, 2000)) return Result::Timeout;
        if (de.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT &&
            de.dwProcessId == pid)
            break;
        ::ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);
    }

    // Set the trap flag on the target thread before the process runs.
    HANDLE hThread = ::OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE,
                                  resolved);
    if (!hThread) return Result::Error;
    CONTEXT ctx;
    std::memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_CONTROL;
    if (!::GetThreadContext(hThread, &ctx)) {
        ::CloseHandle(hThread);
        return Result::Error;
    }
    ctx.EFlags |= 0x100;  // TF
    if (!::SetThreadContext(hThread, &ctx)) {
        ::CloseHandle(hThread);
        return Result::Error;
    }
    ::CloseHandle(hThread);

    // Let the process run; the target thread stops after one instruction.
    ::ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);

    for (;;) {
        if (!::WaitForDebugEvent(&de, 2000)) return Result::Timeout;
        if (de.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            de.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP &&
            de.dwThreadId == resolved) {
            if (out_rip) {
                HANDLE ht = ::OpenThread(THREAD_GET_CONTEXT, FALSE, resolved);
                if (ht) {
                    CONTEXT c2;
                    std::memset(&c2, 0, sizeof(c2));
                    c2.ContextFlags = CONTEXT_CONTROL;
                    if (::GetThreadContext(ht, &c2)) *out_rip = c2.Rip;
                    ::CloseHandle(ht);
                }
            }
            ::ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);
            return Result::Ok;
        }
        ::ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);
    }
}

namespace {

// Detect a near call at the current RIP: 0xE8 (call rel32) or 0xFF /2
// (call r/m64). Returns true and sets the instruction length / return addr.
bool IsNearCall(const uint8_t* code, size_t len, uintptr_t rip,
                size_t* out_len, uintptr_t* out_ret) {
    size_t i = 0;
    // skip legacy prefixes
    while (i < len) {
        uint8_t b = code[i];
        if (b == 0x66 || b == 0x67 || b == 0xF2 || b == 0xF3) { ++i; continue; }
        if (b >= 0x40 && b <= 0x4F) { ++i; continue; }  // REX
        break;
    }
    if (i + 1 > len) return false;
    uint8_t op = code[i];
    if (op == 0xE8) {
        if (i + 5 > len) return false;
        *out_len = i + 5;
        *out_ret = rip + *out_len;
        return true;
    }
    if (op == 0xFF) {
        if (i + 1 > len) return false;
        uint8_t modrm = code[i + 1];
        int mod = (modrm >> 6) & 3;
        int reg = (modrm >> 3) & 7;
        if (reg != 2) return false;  // /2 = call
        int rm = modrm & 7;
        size_t extra = 2;
        if (mod != 3 && rm == 4) extra += 1;             // SIB
        if (mod == 1) extra += 1;
        else if (mod == 2) extra += 4;
        else if (mod == 0 && rm == 5) extra += 4;        // disp32
        if (i + extra > len) return false;
        *out_len = i + extra;
        *out_ret = rip + *out_len;
        return true;
    }
    return false;
}

}  // namespace

Result DebugStepOver(uint32_t pid, uint32_t tid, uintptr_t* out_rip) {
    uint32_t resolved = tid;
    Result r = ResolveTid(pid, tid, &resolved);
    if (r != Result::Ok) return r;

    // Wait for the CREATE_PROCESS_DEBUG_EVENT (threads frozen while pending).
    DEBUG_EVENT de;
    for (;;) {
        if (!::WaitForDebugEvent(&de, 2000)) return Result::Timeout;
        if (de.dwDebugEventCode == CREATE_PROCESS_DEBUG_EVENT &&
            de.dwProcessId == pid)
            break;
        ::ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);
    }

    // Current RIP of the target thread.
    HANDLE hThread = ::OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE,
                                  resolved);
    if (!hThread) return Result::Error;
    CONTEXT ctx;
    std::memset(&ctx, 0, sizeof(ctx));
    ctx.ContextFlags = CONTEXT_CONTROL;
    if (!::GetThreadContext(hThread, &ctx)) {
        ::CloseHandle(hThread);
        return Result::Error;
    }
    uintptr_t rip = ctx.Rip;

    // Decode the current instruction from the remote process.
    Result err = Result::Ok;
    void* hp = OpenProcessById(pid, PROCESS_VM_READ | PROCESS_VM_WRITE |
                                     PROCESS_VM_OPERATION,
                               &err);
    if (!hp) {
        ::CloseHandle(hThread);
        return err;
    }
    uint8_t code[16];
    size_t got = ReadRemoteMemory(hp, rip, code, sizeof(code), &err);
    size_t insn_len = 0;
    uintptr_t ret_addr = 0;
    bool is_call = err == Result::Ok && got >= 1 &&
                   IsNearCall(code, got, rip, &insn_len, &ret_addr);
    ::CloseHandle(static_cast<HANDLE>(hp));

    if (!is_call) {
        // Plain single step.
        ctx.EFlags |= 0x100;  // TF
        if (!::SetThreadContext(hThread, &ctx)) {
            ::CloseHandle(hThread);
            return Result::Error;
        }
        ::CloseHandle(hThread);
        ::ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);

        for (;;) {
            if (!::WaitForDebugEvent(&de, 2000)) return Result::Timeout;
            if (de.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
                de.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP &&
                de.dwThreadId == resolved) {
                if (out_rip) {
                    HANDLE ht = ::OpenThread(THREAD_GET_CONTEXT, FALSE, resolved);
                    if (ht) {
                        CONTEXT c2;
                        std::memset(&c2, 0, sizeof(c2));
                        c2.ContextFlags = CONTEXT_CONTROL;
                        if (::GetThreadContext(ht, &c2)) *out_rip = c2.Rip;
                        ::CloseHandle(ht);
                    }
                }
                ::ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);
                return Result::Ok;
            }
            ::ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);
        }
    }

    // Call: breakpoint at the return address, run, restore.
    void* hp2 = OpenProcessById(pid, PROCESS_VM_READ | PROCESS_VM_WRITE |
                                      PROCESS_VM_OPERATION,
                                &err);
    if (!hp2) {
        ::CloseHandle(hThread);
        return err;
    }
    uint8_t orig = 0;
    if (ReadByte(hp2, ret_addr, &orig) != Result::Ok ||
        WriteByte(hp2, ret_addr, 0xCC) != Result::Ok) {
        ::CloseHandle(hp2);
        ::CloseHandle(hThread);
        return Result::Error;
    }
    ::CloseHandle(hThread);
    ::ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);

    for (;;) {
        if (!::WaitForDebugEvent(&de, 2000)) {
            // Restore the temporary breakpoint so the target is left clean.
            WriteByte(hp2, ret_addr, orig);
            ::CloseHandle(hp2);
            return Result::Timeout;
        }
        if (de.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
            de.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT &&
            de.dwThreadId == resolved &&
            de.u.Exception.ExceptionRecord.ExceptionAddress ==
                reinterpret_cast<void*>(ret_addr)) {
            // Restore the original byte and complete the instruction with TF.
            WriteByte(hp2, ret_addr, orig);
            ::CloseHandle(hp2);

            HANDLE ht = ::OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE,
                                     resolved);
            if (!ht) return Result::Error;
            CONTEXT c2;
            std::memset(&c2, 0, sizeof(c2));
            c2.ContextFlags = CONTEXT_CONTROL;
            if (::GetThreadContext(ht, &c2)) {
                c2.EFlags |= 0x100;
                ::SetThreadContext(ht, &c2);
            }
            ::CloseHandle(ht);
            ::ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);

            // Wait for the single step that re-executes the restored byte.
            for (;;) {
                if (!::WaitForDebugEvent(&de, 2000)) return Result::Timeout;
                if (de.dwDebugEventCode == EXCEPTION_DEBUG_EVENT &&
                    de.u.Exception.ExceptionRecord.ExceptionCode ==
                        EXCEPTION_SINGLE_STEP &&
                    de.dwThreadId == resolved) {
                    if (out_rip) {
                        HANDLE ht2 =
                            ::OpenThread(THREAD_GET_CONTEXT, FALSE, resolved);
                        if (ht2) {
                            CONTEXT c3;
                            std::memset(&c3, 0, sizeof(c3));
                            c3.ContextFlags = CONTEXT_CONTROL;
                            if (::GetThreadContext(ht2, &c3)) *out_rip = c3.Rip;
                            ::CloseHandle(ht2);
                        }
                    }
                    ::ContinueDebugEvent(de.dwProcessId, de.dwThreadId,
                                         DBG_CONTINUE);
                    return Result::Ok;
                }
                ::ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);
            }
        }
        ::ContinueDebugEvent(de.dwProcessId, de.dwThreadId, DBG_CONTINUE);
    }
}

}  // namespace deeptrace::internal

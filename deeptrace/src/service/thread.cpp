#include "service/thread.h"
#include "service/session.h"
#include "infrastructure/inject/inject.h"
#include "infrastructure/thread/thread.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace deeptrace {

Result thread_list(std::vector<ThreadInfo>& out) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    return internal::EnumThreads(s.pid, out);
}

Result thread_suspend(uint32_t tid) {
    if (tid == 0) return Result::InvalidArg;
    Result err = Result::Ok;
    void* h = internal::OpenThreadById(0, tid, THREAD_SUSPEND_RESUME, &err);
    if (!h) return err;
    ::SuspendThread(static_cast<HANDLE>(h));
    ::CloseHandle(static_cast<HANDLE>(h));
    return Result::Ok;
}

Result thread_resume(uint32_t tid) {
    if (tid == 0) return Result::InvalidArg;
    Result err = Result::Ok;
    void* h = internal::OpenThreadById(0, tid, THREAD_SUSPEND_RESUME, &err);
    if (!h) return err;
    ::ResumeThread(static_cast<HANDLE>(h));
    ::CloseHandle(static_cast<HANDLE>(h));
    return Result::Ok;
}

Result thread_terminate(uint32_t tid, uint32_t exit_code) {
    if (tid == 0) return Result::InvalidArg;
    Result err = Result::Ok;
    void* h = internal::OpenThreadById(0, tid, THREAD_TERMINATE, &err);
    if (!h) return err;
    BOOL ok = ::TerminateThread(static_cast<HANDLE>(h), exit_code);
    ::CloseHandle(static_cast<HANDLE>(h));
    return ok ? Result::Ok : Result::Error;
}

Result thread_create_at(uintptr_t addr, uint32_t* out_tid) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    if (addr == 0) return Result::InvalidArg;
    if (!out_tid) return Result::InvalidArg;
    return internal::CreateRemoteThreadEx(s.handle, addr, 0, out_tid);
}

}  // namespace deeptrace

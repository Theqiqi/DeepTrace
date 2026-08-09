#include "infrastructure/inject/inject.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace pmem::internal {

Result CreateRemoteThreadEx(void* hprocess, uintptr_t entry, uintptr_t arg,
                            uint32_t* out_tid) {
    HANDLE h = ::CreateRemoteThread(static_cast<HANDLE>(hprocess), nullptr, 0,
                                    reinterpret_cast<LPTHREAD_START_ROUTINE>(entry),
                                    reinterpret_cast<LPVOID>(arg), 0, nullptr);
    if (!h) return Result::Error;
    DWORD tid = ::GetThreadId(h);
    if (out_tid) *out_tid = tid;
    ::CloseHandle(h);
    return Result::Ok;
}

Result WaitRemoteThread(void* hprocess, uint32_t tid, uint32_t timeout_ms,
                        uint32_t* out_exit_code) {
    HANDLE h = ::OpenThread(THREAD_QUERY_INFORMATION | SYNCHRONIZE, FALSE, tid);
    if (!h) return Result::Error;
    DWORD r = ::WaitForSingleObject(h, timeout_ms);
    if (r == WAIT_TIMEOUT) {
        ::CloseHandle(h);
        return Result::Timeout;
    }
    DWORD code = 0;
    ::GetExitCodeThread(h, &code);
    if (out_exit_code) *out_exit_code = code;
    ::CloseHandle(h);
    return Result::Ok;
}

Result IsRemoteThreadRunning(void* hprocess, uint32_t tid, bool* out_running,
                             uint32_t* out_exit_code) {
    HANDLE h = ::OpenThread(THREAD_QUERY_INFORMATION, FALSE, tid);
    if (!h) {
        // Thread handle vanished -> not running (terminated).
        *out_running = false;
        return Result::Ok;
    }
    DWORD code = 0;
    ::GetExitCodeThread(h, &code);
    ::CloseHandle(h);
    *out_running = (code == STILL_ACTIVE);
    if (out_exit_code) *out_exit_code = code;
    return Result::Ok;
}

}  // namespace pmem::internal

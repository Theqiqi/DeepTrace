#include "service/process.h"
#include "service/session.h"
#include "infrastructure/debug/debug.h"
#include "infrastructure/process/process.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace deeptrace {

Result enumerate_processes(std::vector<ProcessInfo>& out) {
    return internal::EnumProcesses(out);
}

Result attach(uint32_t pid) {
    if (pid == 0) return Result::InvalidArg;
    Result err = Result::Ok;
    void* h = internal::OpenProcessById(pid, PROCESS_ALL_ACCESS, &err);
    if (!h) {
        // Fall back to limited access for query-only operations.
        h = internal::OpenProcessById(
            pid, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_VM_WRITE |
                 PROCESS_VM_OPERATION | PROCESS_CREATE_THREAD | PROCESS_SUSPEND_RESUME,
            &err);
        if (!h) return err;
    }
    auto& s = internal::session();
    s.pid = pid;
    s.handle = h;
    return Result::Ok;
}

Result detach() {
    auto& s = internal::session();
    // A debugger that exits without DebugActiveProcessStop terminates the
    // debuggee on Windows. The non-interactive CLI auto-detaches after every
    // command, so pair the debug session before closing the process handle
    // (otherwise the target crashes right after `debug attach`).
    if (s.debug_mode && s.pid != 0) {
        // Failure here is deliberately not propagated: the caller can do
        // nothing about it, but we still clear debug_mode so a second detach
        // does not retry with an already-released debug session.
        internal::DebugDetachProcess(s.pid);
        s.debug_mode = false;
    }
    if (s.handle) {
        ::CloseHandle(static_cast<HANDLE>(s.handle));
        s.handle = nullptr;
    }
    s.pid = 0;
    return Result::Ok;
}

Result process_info(uint32_t pid, ProcessInfo& out) {
    return internal::QueryProcessInfo(pid, out);
}

Result suspend_process(uint32_t pid) {
    if (pid == 0) return Result::InvalidArg;
    return internal::SuspendProcessThreads(pid);
}

Result resume_process(uint32_t pid) {
    if (pid == 0) return Result::InvalidArg;
    return internal::ResumeProcessThreads(pid);
}

Result terminate_process(uint32_t pid, uint32_t exit_code) {
    if (pid == 0) return Result::InvalidArg;
    return internal::TerminateProcessById(pid, exit_code);
}

Result session_pid(uint32_t* out_pid) {
    if (!out_pid) return Result::InvalidArg;
    *out_pid = internal::session().pid;
    return Result::Ok;
}

}  // namespace deeptrace

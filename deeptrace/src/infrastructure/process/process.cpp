#include "infrastructure/process/process.h"
#include "infrastructure/thread/thread.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

#include <string>

namespace deeptrace::internal {

void* OpenProcessById(uint32_t pid, uint32_t access, Result* err) {
    HANDLE h = ::OpenProcess(access, FALSE, pid);
    if (!h) {
        DWORD e = ::GetLastError();
        if (e == ERROR_INVALID_PARAMETER) *err = Result::NoSuchProcess;
        else if (e == ERROR_ACCESS_DENIED) *err = Result::AccessDenied;
        else *err = Result::Error;
        return nullptr;
    }
    *err = Result::Ok;
    return h;
}

Result EnumProcesses(std::vector<ProcessInfo>& out) {
    out.clear();
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return Result::Error;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (::Process32FirstW(snap, &pe)) {
        do {
            ProcessInfo info;
            info.pid = pe.th32ProcessID;
            info.parent_pid = pe.th32ParentProcessID;
            info.thread_count = pe.cntThreads;
            info.name = pe.szExeFile;
            out.push_back(info);
        } while (::Process32NextW(snap, &pe));
    }
    ::CloseHandle(snap);
    return Result::Ok;
}

Result QueryProcessInfo(uint32_t pid, ProcessInfo& out) {
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return Result::Error;
    bool found = false;
    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    if (::Process32FirstW(snap, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                out.pid = pid;
                out.parent_pid = pe.th32ParentProcessID;
                out.thread_count = pe.cntThreads;
                out.name = pe.szExeFile;
                found = true;
                break;
            }
        } while (::Process32NextW(snap, &pe));
    }
    ::CloseHandle(snap);
    return found ? Result::Ok : Result::NoSuchProcess;
}

Result SuspendProcessThreads(uint32_t pid) {
    return ForEachThread(pid, [](const ThreadInfo& t) {
        HANDLE h = ::OpenThread(THREAD_SUSPEND_RESUME, FALSE, t.tid);
        if (h) {
            ::SuspendThread(h);
            ::CloseHandle(h);
        }
    });
}

Result ResumeProcessThreads(uint32_t pid) {
    return ForEachThread(pid, [](const ThreadInfo& t) {
        HANDLE h = ::OpenThread(THREAD_SUSPEND_RESUME, FALSE, t.tid);
        if (h) {
            ::ResumeThread(h);
            ::CloseHandle(h);
        }
    });
}

Result TerminateProcessById(uint32_t pid, uint32_t exit_code) {
    Result err = Result::Ok;
    void* h = OpenProcessById(pid, PROCESS_TERMINATE, &err);
    if (!h) return err;
    BOOL ok = ::TerminateProcess(static_cast<HANDLE>(h), exit_code);
    ::CloseHandle(static_cast<HANDLE>(h));
    return ok ? Result::Ok : Result::Error;
}

}  // namespace deeptrace::internal

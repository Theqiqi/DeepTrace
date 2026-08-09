#include "infrastructure/thread/thread.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

namespace pmem::internal {

Result EnumThreads(uint32_t pid, std::vector<ThreadInfo>& out) {
    out.clear();
    HANDLE snap = ::CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return Result::Error;
    THREADENTRY32 te;
    te.dwSize = sizeof(te);
    if (::Thread32First(snap, &te)) {
        do {
            if (te.th32OwnerProcessID == pid) {
                ThreadInfo info;
                info.tid = te.th32ThreadID;
                info.priority = te.tpBasePri;
                info.start_address = 0;
                out.push_back(info);
            }
        } while (::Thread32Next(snap, &te));
    }
    ::CloseHandle(snap);
    return Result::Ok;
}

Result ForEachThread(uint32_t pid, const std::function<void(const ThreadInfo&)>& cb) {
    std::vector<ThreadInfo> threads;
    Result r = EnumThreads(pid, threads);
    if (r != Result::Ok) return r;
    for (const auto& t : threads) cb(t);
    return Result::Ok;
}

void* OpenThreadById(uint32_t pid, uint32_t tid, uint32_t access, Result* err) {
    uint32_t resolved = tid;
    if (tid == 0) {
        Result r = ResolveTid(pid, 0, &resolved);
        if (r != Result::Ok) {
            *err = r;
            return nullptr;
        }
    }
    HANDLE h = ::OpenThread(access, FALSE, resolved);
    if (!h) {
        *err = Result::Error;
        return nullptr;
    }
    *err = Result::Ok;
    return h;
}

Result ResolveTid(uint32_t pid, uint32_t tid, uint32_t* out_tid) {
    if (tid != 0) {
        *out_tid = tid;
        return Result::Ok;
    }
    std::vector<ThreadInfo> threads;
    Result r = EnumThreads(pid, threads);
    if (r != Result::Ok) return r;
    if (threads.empty()) return Result::NotFound;
    *out_tid = threads[0].tid;
    return Result::Ok;
}

}  // namespace pmem::internal

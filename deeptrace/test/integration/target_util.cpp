#include "target_util.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>

namespace testutil {

std::string target_exe_path() {
    char buf[MAX_PATH] = {0};
    ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string exe(buf);
    // test exe lives in the same directory as deeptrace_target.exe
    size_t slash = exe.find_last_of("/\\");
    if (slash != std::string::npos) exe = exe.substr(0, slash);
    return exe + "\\deeptrace_target.exe";
}

bool spawn_target(const std::string& exe_path, TargetInfo& out) {
    std::string cmdline = "\"" + exe_path + "\"";
    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!::CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    ::SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    PROCESS_INFORMATION pi = {0};

    char* cmd = _strdup(cmdline.c_str());
    BOOL ok = ::CreateProcessA(nullptr, cmd, nullptr, nullptr, TRUE, 0, nullptr,
                               nullptr, &si, &pi);
    free(cmd);
    ::CloseHandle(hWrite);
    if (!ok) {
        ::CloseHandle(hRead);
        return false;
    }
    ::CloseHandle(pi.hThread);

    // read banner
    char buf[8192];
    DWORD total = 0;
    DWORD readn = 0;
    // read until "WORKER_TID:" line or timeout
    std::string all;
    for (int i = 0; i < 100; ++i) {
        if (!::PeekNamedPipe(hRead, nullptr, 0, nullptr, &readn, nullptr)) break;
        if (readn > 0) {
            DWORD got = 0;
            if (::ReadFile(hRead, buf + total % 4096, 4096, &got, nullptr) && got > 0) {
                all.append(buf + total % 4096, got);
                total += got;
            }
        }
        if (all.find("WORKER_TID:") != std::string::npos) break;
        ::Sleep(50);
    }
    ::CloseHandle(hRead);
    ::CloseHandle(pi.hProcess);

    if (all.find("WORKER_TID:") == std::string::npos) return false;

    auto parse_addr = [](const std::string& line, const char* key) -> uintptr_t {
        // banner format: "<name> = <value> @0x<address>"
        // the address follows the '@' marker; fall back to the first 0x
        // (used by lines without a value column, e.g. worker_fn).
        size_t p = line.find(key);
        if (p == std::string::npos) return 0;
        size_t at = line.find("@0x", p);
        if (at == std::string::npos) {
            at = line.find("0x", p);
            if (at == std::string::npos) return 0;
            return strtoull(line.c_str() + at, nullptr, 16);
        }
        return strtoull(line.c_str() + at + 3, nullptr, 16);
    };

    out.pid = 0;
    size_t p = all.find("PID:");
    if (p != std::string::npos) {
        p += 4;
        while (p < all.size() && (all[p] == ' ' || all[p] == '\t' || all[p] == '\r' ||
                                  all[p] == '\n'))
            ++p;
        out.pid = static_cast<uint32_t>(strtoul(all.c_str() + p, nullptr, 10));
    }
    size_t wt = all.find("WORKER_TID:");
    if (wt != std::string::npos) {
        wt += 11;
        while (wt < all.size() && (all[wt] == ' ' || all[wt] == '\t' || all[wt] == '\r' ||
                                   all[wt] == '\n'))
            ++wt;
        out.worker_tid = static_cast<uint32_t>(strtoul(all.c_str() + wt, nullptr, 10));
    }
    out.g_int = parse_addr(all, "g_int ");
    out.g_int64 = parse_addr(all, "g_int64");
    out.g_float = parse_addr(all, "g_float");
    out.g_double = parse_addr(all, "g_double");
    out.g_bytes = parse_addr(all, "g_bytes");
    out.g_flag = parse_addr(all, "g_flag");
    out.g_counter = parse_addr(all, "g_counter");
    out.worker_fn = parse_addr(all, "worker_fn");
    return out.pid != 0;
}

std::string test_dll_path() {
    std::string dir = target_exe_path();
    size_t slash = dir.find_last_of("/\\");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    return dir + "\\testdll.dll";
}

bool file_exists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

void kill_target(uint32_t pid) {
    HANDLE h = ::OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (h) {
        ::TerminateProcess(h, 1);
        ::CloseHandle(h);
    }
}

}  // namespace testutil

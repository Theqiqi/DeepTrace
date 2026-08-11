#include "service/inject.h"
#include "service/session.h"
#include "service/store.h"
#include "algorithm/hex.h"
#include "infrastructure/inject/inject.h"
#include "infrastructure/memory/memory.h"
#include "infrastructure/module/module.h"
#include "infrastructure/process/process.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

namespace deeptrace {

namespace {

// Locate kernel32 LoadLibraryW / FreeLibrary addresses in the local process.
uintptr_t kernel32_proc(const char* name) {
    HMODULE k32 = ::GetModuleHandleA("kernel32.dll");
    if (!k32) return 0;
    FARPROC p = ::GetProcAddress(k32, name);
    return reinterpret_cast<uintptr_t>(p);
}

// Check whether a dll path is still loaded in the target (by filename).
bool dll_still_loaded(const std::string& path, bool* loaded) {
    auto& s = internal::session();
    if (!s.handle) return false;
    // basename
    size_t slash = path.find_last_of("/\\");
    std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
    std::vector<ModuleInfo> mods;
    if (internal::EnumModules(s.handle, mods) != Result::Ok) {
        *loaded = false;
        return true;
    }
    std::wstring wbase(base.begin(), base.end());
    for (const auto& m : mods) {
        std::wstring mn = m.name;
        if (mn.size() == wbase.size()) {
            bool same = true;
            for (size_t i = 0; i < mn.size(); ++i) {
                wchar_t a = mn[i], b = wbase[i];
                if (a >= L'A' && a <= L'Z') a = static_cast<wchar_t>(a - L'A' + L'a');
                if (b >= L'A' && b <= L'Z') b = static_cast<wchar_t>(b - L'A' + L'a');
                if (a != b) { same = false; break; }
            }
            if (same) { *loaded = true; return true; }
        }
    }
    *loaded = false;
    return true;
}

std::string wtoa_ascii(const std::wstring& w) {
    std::string out;
    for (wchar_t c : w) {
        if (c >= 0x20 && c < 0x7F) out.push_back(static_cast<char>(c));
        else out.push_back('?');
    }
    return out;
}

}  // namespace

Result dll_inject(const std::string& path, InjectInfo& out) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    if (path.empty()) return Result::InvalidArg;

    // 1. write the path (as ANSI) into the target
    size_t len = path.size() + 1;
    uintptr_t remote = 0;
    Result r = internal::RemoteAlloc(s.handle, len, PAGE_READWRITE, &remote);
    if (r != Result::Ok) return r;

    Result err;
    size_t n = internal::WriteRemoteMemory(s.handle, remote, path.c_str(), len, &err);
    if (err != Result::Ok || n != len) {
        internal::RemoteFree(s.handle, remote);
        return err != Result::Ok ? err : Result::WriteFault;
    }

    uintptr_t load_lib = kernel32_proc("LoadLibraryA");
    if (!load_lib) {
        internal::RemoteFree(s.handle, remote);
        return Result::Error;
    }

    uint32_t tid = 0;
    r = internal::CreateRemoteThreadEx(s.handle, load_lib, remote, &tid);
    if (r != Result::Ok) {
        internal::RemoteFree(s.handle, remote);
        return r;
    }

    uint32_t code = 0;
    r = internal::WaitRemoteThread(s.handle, tid, 15000, &code);
    if (r == Result::Timeout) {
        internal::RemoteFree(s.handle, remote);
        return Result::Timeout;
    }
    if (r != Result::Ok) {
        internal::RemoteFree(s.handle, remote);
        return r;
    }
    // LoadLibraryA returns the module base (or NULL=0)
    uintptr_t base = code;
    internal::RemoteFree(s.handle, remote);

    if (base == 0) return Result::Error;

    out.kind = "dll";
    out.path = std::wstring(path.begin(), path.end());
    out.remote_base = base;
    out.thread_id = tid;
    out.running = true;
    out.size = len;

    auto recs = internal::load_injects(s.pid);
    recs.push_back(internal::InjectRecord{"dll", path, base, tid});
    internal::save_injects(s.pid, recs);
    return Result::Ok;
}

Result dll_eject(const std::string& path_or_addr) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;

    uintptr_t target_addr = 0;
    bool have_addr = false;
    if (path_or_addr.rfind("0x", 0) == 0 || path_or_addr.rfind("0X", 0) == 0) {
        have_addr = internal::parse_uint64(path_or_addr, target_addr);
    }
    bool found = false;
    auto recs = internal::load_injects(s.pid);

    uintptr_t base_to_free = 0;
    for (const auto& rec : recs) {
        if (rec.kind != "dll") continue;
        bool match = (have_addr && rec.address == target_addr) ||
                     (rec.path == path_or_addr);
        if (match) {
            base_to_free = rec.address;
            found = true;
            break;
        }
    }
    if (!found) return Result::NotFound;

    uintptr_t free_lib = kernel32_proc("FreeLibrary");
    if (!free_lib) return Result::Error;
    uint32_t tid = 0;
    Result r = internal::CreateRemoteThreadEx(s.handle, free_lib, base_to_free, &tid);
    if (r != Result::Ok) return r;
    uint32_t code = 0;
    internal::WaitRemoteThread(s.handle, tid, 5000, &code);

    // remove the record
    std::vector<internal::InjectRecord> rest;
    for (const auto& rec : recs) {
        if (rec.kind == "dll" &&
            ((have_addr && rec.address == target_addr) || rec.path == path_or_addr)) {
            continue;
        }
        rest.push_back(rec);
    }
    internal::save_injects(s.pid, rest);
    return Result::Ok;
}

Result dll_list(std::vector<InjectInfo>& out) {
    out.clear();
    auto& s = internal::session();
    if (!s.pid) return Result::NotAttached;
    auto recs = internal::load_injects(s.pid);
    for (const auto& rec : recs) {
        if (rec.kind != "dll") continue;
        InjectInfo info;
        info.kind = "dll";
        info.path = std::wstring(rec.path.begin(), rec.path.end());
        info.remote_base = rec.address;
        info.thread_id = rec.thread_id;
        bool loaded = false;
        if (s.handle) dll_still_loaded(rec.path, &loaded);
        info.running = loaded;
        out.push_back(info);
    }
    return Result::Ok;
}

Result dll_status(std::vector<InjectInfo>& out) {
    return dll_list(out);
}

Result shellcode_inject(const std::vector<uint8_t>& bytes, InjectInfo& out) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    if (bytes.empty()) return Result::InvalidArg;

    uintptr_t remote = 0;
    Result r = internal::RemoteAlloc(s.handle, bytes.size(), PAGE_EXECUTE_READWRITE,
                                     &remote);
    if (r != Result::Ok) return r;

    Result err;
    size_t n = internal::WriteRemoteMemory(s.handle, remote, bytes.data(), bytes.size(),
                                           &err);
    if (err != Result::Ok || n != bytes.size()) {
        internal::RemoteFree(s.handle, remote);
        return err != Result::Ok ? err : Result::WriteFault;
    }

    uint32_t tid = 0;
    r = internal::CreateRemoteThreadEx(s.handle, remote, 0, &tid);
    if (r != Result::Ok) {
        internal::RemoteFree(s.handle, remote);
        return r;
    }

    out.kind = "shellcode";
    out.remote_base = remote;
    out.thread_id = tid;
    out.running = true;
    out.size = bytes.size();

    auto recs = internal::load_injects(s.pid);
    recs.push_back(internal::InjectRecord{
        "shellcode", internal::hex_encode(bytes.data(), bytes.size()), remote, tid});
    internal::save_injects(s.pid, recs);
    return Result::Ok;
}

Result shellcode_inject_at(uintptr_t addr, const std::vector<uint8_t>& bytes,
                           InjectInfo& out) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    if (bytes.empty() || addr == 0) return Result::InvalidArg;

    Result err;
    size_t n = internal::WriteRemoteMemory(s.handle, addr, bytes.data(), bytes.size(),
                                           &err);
    if (err != Result::Ok || n != bytes.size()) {
        return err != Result::Ok ? err : Result::WriteFault;
    }

    uint32_t tid = 0;
    Result r = internal::CreateRemoteThreadEx(s.handle, addr, 0, &tid);
    if (r != Result::Ok) return r;

    out.kind = "shellcode";
    out.remote_base = addr;
    out.thread_id = tid;
    out.running = true;
    out.size = bytes.size();

    auto recs = internal::load_injects(s.pid);
    recs.push_back(internal::InjectRecord{
        "shellcode", internal::hex_encode(bytes.data(), bytes.size()), addr, tid});
    internal::save_injects(s.pid, recs);
    return Result::Ok;
}

Result shellcode_alloc(const std::vector<uint8_t>& bytes, InjectInfo& out) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    if (bytes.empty()) return Result::InvalidArg;

    uintptr_t remote = 0;
    Result r = internal::RemoteAlloc(s.handle, bytes.size(), PAGE_EXECUTE_READWRITE,
                                     &remote);
    if (r != Result::Ok) return r;

    Result err;
    size_t n = internal::WriteRemoteMemory(s.handle, remote, bytes.data(), bytes.size(),
                                           &err);
    if (err != Result::Ok || n != bytes.size()) {
        internal::RemoteFree(s.handle, remote);
        return err != Result::Ok ? err : Result::WriteFault;
    }

    out.kind = "shellcode";
    out.remote_base = remote;
    out.thread_id = 0;
    out.running = false;
    out.size = bytes.size();

    auto recs = internal::load_injects(s.pid);
    recs.push_back(internal::InjectRecord{
        "shellcode", internal::hex_encode(bytes.data(), bytes.size()), remote, 0});
    internal::save_injects(s.pid, recs);
    return Result::Ok;
}

Result shellcode_run(uintptr_t addr, InjectInfo& out) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    if (addr == 0) return Result::InvalidArg;

    auto recs = internal::load_injects(s.pid);
    internal::InjectRecord* rec = nullptr;
    for (auto& r : recs) {
        if (r.kind == "shellcode" && r.address == addr) {
            rec = &r;
            break;
        }
    }
    if (!rec) return Result::NotFound;

    uint32_t tid = 0;
    Result r = internal::CreateRemoteThreadEx(s.handle, addr, 0, &tid);
    if (r != Result::Ok) return r;

    rec->thread_id = tid;
    internal::save_injects(s.pid, recs);

    out.kind = "shellcode";
    out.remote_base = addr;
    out.thread_id = tid;
    out.running = true;
    out.size = rec->path.size() / 2;
    return Result::Ok;
}

Result shellcode_free(uintptr_t addr) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    if (addr == 0) return Result::InvalidArg;

    auto recs = internal::load_injects(s.pid);
    bool found = false;
    for (const auto& rec : recs) {
        if (rec.kind == "shellcode" && rec.address == addr) {
            found = true;
            break;
        }
    }
    if (!found) return Result::NotFound;

    Result r = internal::RemoteFree(s.handle, addr);
    if (r != Result::Ok) return r;

    std::vector<internal::InjectRecord> rest;
    for (const auto& rec : recs) {
        if (rec.kind == "shellcode" && rec.address == addr) continue;
        rest.push_back(rec);
    }
    internal::save_injects(s.pid, rest);
    return Result::Ok;
}

Result shellcode_status(std::vector<InjectInfo>& out) {
    out.clear();
    auto& s = internal::session();
    if (!s.pid) return Result::NotAttached;
    auto recs = internal::load_injects(s.pid);
    for (const auto& rec : recs) {
        if (rec.kind != "shellcode") continue;
        InjectInfo info;
        info.kind = "shellcode";
        info.remote_base = rec.address;
        info.thread_id = rec.thread_id;
        info.size = rec.path.size() / 2;
        bool running = false;
        uint32_t code = 0;
        if (s.handle) {
            internal::IsRemoteThreadRunning(s.handle, rec.thread_id, &running, &code);
        }
        info.running = running;
        out.push_back(info);
    }
    return Result::Ok;
}

}  // namespace deeptrace

#include "service/script.h"
#include "service/session.h"
#include "service/store.h"
#include "infrastructure/memory/memory.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <string>
#include <vector>

namespace deeptrace {

namespace {

// ASCII printable name (letters/digits/underscore), non-empty.
bool valid_symbol_name(const std::string& name) {
    if (name.empty()) return false;
    for (unsigned char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_')) {
            return false;
        }
    }
    return true;
}

bool symbol_exists(const std::vector<internal::ScriptSymbolRecord>& recs,
                   const std::string& name) {
    return std::any_of(recs.begin(), recs.end(),
                       [&](const internal::ScriptSymbolRecord& r) {
                           return r.name == name;
                       });
}

}  // namespace

Result script_alloc(const std::string& name, size_t size, const std::string& owner,
                    uintptr_t* out_addr) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    if (!valid_symbol_name(name) || size == 0) return Result::InvalidArg;
    if (!out_addr) return Result::InvalidArg;

    auto recs = internal::load_script_symbols(s.pid);
    if (symbol_exists(recs, name)) return Result::InvalidArg;  // duplicate symbol

    uintptr_t remote = 0;
    Result r = internal::RemoteAlloc(s.handle, size, PAGE_EXECUTE_READWRITE, &remote);
    if (r != Result::Ok) return r;

    // Register symbol before returning; on record-save failure roll back the
    // allocation so no untracked memory is leaked.
    recs.push_back(internal::ScriptSymbolRecord{name, remote, owner});
    if (!internal::save_script_symbols(s.pid, recs)) {
        internal::RemoteFree(s.handle, remote);
        return Result::Error;
    }
    *out_addr = remote;
    return Result::Ok;
}

Result script_free(const std::string& name) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    if (!valid_symbol_name(name)) return Result::InvalidArg;

    auto recs = internal::load_script_symbols(s.pid);
    uintptr_t addr = 0;
    bool found = false;
    for (const auto& r : recs) {
        if (r.name == name) {
            addr = r.address;
            found = true;
            break;
        }
    }
    if (!found) return Result::NotFound;

    // Persist removal before releasing memory (failed save never leaves a
    // record pointing at freed memory).
    std::vector<internal::ScriptSymbolRecord> rest;
    for (const auto& r : recs) {
        if (r.name == name) continue;
        rest.push_back(r);
    }
    if (!internal::save_script_symbols(s.pid, rest)) return Result::Error;

    return internal::RemoteFree(s.handle, addr);
}

Result script_enable(const std::string& path) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    if (path.empty()) return Result::InvalidArg;

    auto enabled = internal::load_enabled_scripts(s.pid);
    for (const auto& p : enabled) {
        if (p == path) return Result::Ok;  // idempotent: already enabled
    }
    enabled.push_back(path);
    return internal::save_enabled_scripts(s.pid, enabled) ? Result::Ok
                                                          : Result::Error;
}

Result script_disable(const std::string& path) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    if (path.empty()) return Result::InvalidArg;

    auto enabled = internal::load_enabled_scripts(s.pid);
    bool found = false;
    for (const auto& p : enabled) {
        if (p == path) {
            found = true;
            break;
        }
    }
    if (!found) return Result::Ok;  // idempotent: already disabled

    std::vector<std::string> rest;
    for (const auto& p : enabled) {
        if (p != path) rest.push_back(p);
    }
    return internal::save_enabled_scripts(s.pid, rest) ? Result::Ok
                                                       : Result::Error;
}

Result script_status(std::vector<ScriptInfo>& out) {
    out.clear();
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;

    auto enabled = internal::load_enabled_scripts(s.pid);
    auto syms = internal::load_script_symbols(s.pid);
    auto hooks = internal::load_hooks(s.pid);

    // One ScriptInfo per enabled script path; attach only the hooks and alloc
    // symbols owned by that script (owner field). Unattached records (owner="")
    // are listed under a synthetic entry so they remain visible.
    bool have_unowned = false;
    for (const auto& h : hooks) {
        if (h.owner.empty()) have_unowned = true;
    }
    for (const auto& sym : syms) {
        if (sym.owner.empty()) have_unowned = true;
    }

    for (const auto& path : enabled) {
        ScriptInfo info;
        info.path = path;
        info.state = "enabled";
        for (const auto& h : hooks) {
            if (h.owner != path) continue;
            HookInfo hi;
            hi.target = h.target;
            hi.newmem = h.newmem;
            hi.orig_bytes = h.orig_bytes;
            hi.size = h.size;
            info.hooks.push_back(hi);
        }
        for (const auto& sym : syms) {
            if (sym.owner != path) continue;
            info.allocs.emplace_back(sym.name, sym.address);
        }
        out.push_back(std::move(info));
    }

    if (have_unowned) {
        ScriptInfo info;
        info.path = "(unowned)";
        info.state = "enabled";
        for (const auto& h : hooks) {
            if (!h.owner.empty()) continue;
            HookInfo hi;
            hi.target = h.target;
            hi.newmem = h.newmem;
            hi.orig_bytes = h.orig_bytes;
            hi.size = h.size;
            info.hooks.push_back(hi);
        }
        for (const auto& sym : syms) {
            if (!sym.owner.empty()) continue;
            info.allocs.emplace_back(sym.name, sym.address);
        }
        out.push_back(std::move(info));
    }
    return Result::Ok;
}

}  // namespace deeptrace

#include "service/module.h"
#include "service/session.h"
#include "algorithm/hex.h"
#include "infrastructure/memory/memory.h"
#include "infrastructure/module/module.h"

#include <cctype>
#include <fstream>

namespace pmem {

namespace {

std::string wtoa_ascii(const std::wstring& w) {
    std::string out;
    for (wchar_t c : w) {
        if (c >= 0x20 && c < 0x7F) out.push_back(static_cast<char>(c));
        else out.push_back('?');
    }
    return out;
}

bool name_matches(const std::wstring& mod_name, const std::wstring& mod_path,
                  const std::string& want) {
    // match against module name (case-insensitive) and full path basename
    std::string n = wtoa_ascii(mod_name);
    std::string p = wtoa_ascii(mod_path);
    std::string w = want;
    for (auto& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (auto& c : p) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (auto& c : w) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (n == w) return true;
    if (p == w) return true;
    // allow "kernel32" without extension
    std::string noext = n;
    size_t dot = noext.rfind('.');
    if (dot != std::string::npos) noext = noext.substr(0, dot);
    if (noext == w) return true;
    return false;
}

Result find_module_info(const std::string& name, ModuleInfo& out) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    std::vector<ModuleInfo> mods;
    Result r = internal::EnumModules(s.handle, mods);
    if (r != Result::Ok) return r;
    for (const auto& m : mods) {
        if (name_matches(m.name, m.path, name)) {
            out = m;
            return Result::Ok;
        }
    }
    return Result::NotFound;
}

}  // namespace

Result module_list(std::vector<ModuleInfo>& out) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    return internal::EnumModules(s.handle, out);
}

Result module_find(const std::string& name, ModuleInfo& out) {
    if (name.empty()) return Result::InvalidArg;
    return find_module_info(name, out);
}

Result module_base(const std::string& name, uintptr_t* out_base) {
    if (!out_base) return Result::InvalidArg;
    ModuleInfo info;
    Result r = find_module_info(name, info);
    if (r != Result::Ok) return r;
    *out_base = info.base;
    return Result::Ok;
}

Result module_exports(const std::string& name, std::vector<ExportInfo>& out) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    ModuleInfo info;
    Result r = find_module_info(name, info);
    if (r != Result::Ok) return r;
    return internal::ParseExports(s.handle, info.base, out);
}

Result module_dump(const std::string& name, const std::string& output_file,
                   std::string* out_hex) {
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    ModuleInfo info;
    Result r = find_module_info(name, info);
    if (r != Result::Ok) return r;

    std::vector<uint8_t> data;
    const size_t CHUNK = 1u << 20;
    data.reserve(info.size > 0 ? info.size : CHUNK);
    size_t remain = info.size > 0 ? info.size : CHUNK;
    uintptr_t addr = info.base;
    Result err;
    while (remain > 0) {
        size_t want = remain > CHUNK ? CHUNK : remain;
        std::vector<uint8_t> chunk(want);
        size_t got = internal::ReadRemoteMemory(s.handle, addr, chunk.data(), want, &err);
        if (err != Result::Ok || got == 0) break;
        data.insert(data.end(), chunk.begin(), chunk.begin() + got);
        addr += got;
        remain -= got;
    }

    if (!output_file.empty()) {
        std::ofstream f(output_file, std::ios::binary);
        if (!f.is_open()) return Result::Error;
        f.write(reinterpret_cast<const char*>(data.data()), data.size());
        return Result::Ok;
    }
    if (out_hex) {
        *out_hex = internal::hex_encode(data.data(), data.size());
    }
    return Result::Ok;
}

}  // namespace pmem

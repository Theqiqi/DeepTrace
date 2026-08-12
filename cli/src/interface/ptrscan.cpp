#include "interface/ptrscan.h"
#include "interface/json.h"

#include <cstdint>
#include <fstream>
#include <sstream>

namespace deeptrace_cli {
namespace ptrscan {

namespace {

constexpr size_t kMaxPtrscanBytes = 1024 * 1024;  // 1 MiB
constexpr uint64_t kMaxOffset = 0x10000000;       // 256 MiB
constexpr uint64_t kMaxLevel = 64;
constexpr uint64_t kMaxResults = 1000000;
constexpr uint64_t kMaxThreads = 1024;

// Address-like field: hex/decimal literal > 0.
bool parse_addr_field(const jsn::JVal& obj, const std::string& key,
                      uintptr_t& out) {
    const jsn::JVal* v = jsn::find_member(obj, key);
    if (!v) return false;
    std::string raw;
    uint64_t val = 0;
    if (!jsn::member_raw(v, raw) || !jsn::parse_uint(raw, val) || val == 0)
        return false;
    out = static_cast<uintptr_t>(val);
    return true;
}

// Numeric field within [minv, maxv]; absent -> dflt.
bool parse_uint_field(const jsn::JVal& obj, const std::string& key,
                      uint64_t dflt, uint64_t minv, uint64_t maxv,
                      uint32_t& out, std::string& err) {
    const jsn::JVal* v = jsn::find_member(obj, key);
    if (!v) {
        out = static_cast<uint32_t>(dflt);
        return true;
    }
    std::string raw;
    uint64_t val = 0;
    if (!jsn::member_raw(v, raw) || !jsn::parse_uint(raw, val) || val < minv ||
        val > maxv) {
        err = "ptrscan: invalid '" + key + "'";
        return false;
    }
    out = static_cast<uint32_t>(val);
    return true;
}

bool validate(const jsn::JVal& root, Config& out, std::string& err) {
    if (root.kind != jsn::JVal::Kind::Obj) {
        err = "ptrscan: top-level must be an object";
        return false;
    }
    for (const auto& kv : root.obj) {
        if (kv.first != "version" && kv.first != "target" &&
            kv.first != "module" && kv.first != "max_offset" &&
            kv.first != "max_level" && kv.first != "max_results" &&
            kv.first != "threads" && kv.first != "rescan") {
            err = "ptrscan: unknown top-level field '" + kv.first + "'";
            return false;
        }
    }

    // version: required, must be 1 (number or string "1").
    const jsn::JVal* ver = jsn::find_member(root, "version");
    if (ver) {
        std::string raw;
        uint64_t v = 0;
        if (!jsn::member_raw(ver, raw) || !jsn::parse_uint(raw, v) || v != 1) {
            err = "ptrscan: invalid version '" + raw + "' (expected 1)";
            return false;
        }
    }

    // target: required, > 0.
    if (!parse_addr_field(root, "target", out.target)) {
        err = "ptrscan: missing or invalid 'target' (hex/decimal address > 0)";
        return false;
    }

    // module: optional string (empty = no anchoring).
    if (const jsn::JVal* mod = jsn::find_member(root, "module")) {
        if (mod->kind != jsn::JVal::Kind::Str) {
            err = "ptrscan: 'module' must be a string";
            return false;
        }
        out.module = mod->str;
    }

    // max_offset: >= 0 (0 = exact-pointer match), default 2048.
    if (!parse_uint_field(root, "max_offset", 2048, 0, kMaxOffset,
                          out.max_offset, err))
        return false;
    // max_level / max_results / threads: positive ranges with defaults.
    if (!parse_uint_field(root, "max_level", 5, 1, kMaxLevel, out.max_level, err))
        return false;
    if (!parse_uint_field(root, "max_results", 10000, 1, kMaxResults,
                          out.max_results, err))
        return false;
    if (!parse_uint_field(root, "threads", 0, 0, kMaxThreads, out.threads, err))
        return false;

    // rescan: optional, null or { "target": address }.
    if (const jsn::JVal* rs = jsn::find_member(root, "rescan")) {
        if (rs->kind == jsn::JVal::Kind::Bool && !rs->b) {
            // null -> no rescan
        } else if (rs->kind == jsn::JVal::Kind::Obj) {
            if (!parse_addr_field(*rs, "target", out.rescan_target)) {
                err = "ptrscan: invalid 'rescan.target'";
                return false;
            }
            out.has_rescan = true;
        } else {
            err = "ptrscan: 'rescan' must be null or an object";
            return false;
        }
    }
    return true;
}

}  // namespace

bool parse_text(const std::string& text, Config& out, std::string& err) {
    if (text.size() > kMaxPtrscanBytes) {
        err = "ptrscan: file too large (max 1 MiB)";
        return false;
    }
    jsn::JVal root;
    if (!jsn::parse(text, root, err, "ptrscan")) return false;
    return validate(root, out, err);
}

bool parse_file(const std::string& path, Config& out, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        err = "ptrscan: cannot read file: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return parse_text(ss.str(), out, err);
}

}  // namespace ptrscan
}  // namespace deeptrace_cli

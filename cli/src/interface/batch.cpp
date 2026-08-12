#include "interface/batch.h"
#include "interface/json.h"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>

namespace deeptrace_cli {
namespace batch {

namespace {

constexpr size_t kMaxBatchBytes = 1024 * 1024;  // 1 MiB
constexpr uint64_t kMaxCount = 65536;

// ---- validation helpers ----

bool valid_type(const std::string& t) {
    return t == "byte" || t == "word" || t == "dword" || t == "qword" ||
           t == "float" || t == "double" || t == "string" || t == "bytes";
}

// Compact a hex byte string (continuous or space-separated, optional 0x
// prefix) and count the bytes. Returns false on any malformed digit.
bool compact_hex_count(const std::string& s, size_t& out_count) {
    std::string compact;
    for (char c : s) {
        if (c != ' ' && c != '\t') compact.push_back(c);
    }
    size_t start = 0;
    if (compact.size() >= 2 && compact[0] == '0' &&
        (compact[1] == 'x' || compact[1] == 'X'))
        start = 2;
    size_t n = compact.size() - start;
    if (n == 0 || (n % 2) != 0) return false;
    for (size_t i = start; i < compact.size(); ++i) {
        char c = compact[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F')))
            return false;
    }
    out_count = n / 2;
    return true;
}

// Write-mode value validation per type (mirrors the `convert` command rules).
bool valid_type_value(const std::string& type, const std::string& value) {
    if (value.empty()) return false;
    if (type == "bytes") {
        size_t n = 0;
        return compact_hex_count(value, n);
    }
    if (type == "string") {
        for (unsigned char c : value) {
            if (c < 0x20 || c > 0x7E) return false;  // printable ASCII only
        }
        return true;
    }
    if (type == "byte" || type == "word" || type == "dword" || type == "qword") {
        uint64_t val = 0;
        if (!jsn::parse_uint(value, val)) return false;
        uint64_t maxv = 0;
        if (type == "byte") maxv = 0xFFULL;
        else if (type == "word") maxv = 0xFFFFULL;
        else if (type == "dword") maxv = 0xFFFFFFFFULL;
        else maxv = UINT64_MAX;
        return val <= maxv;
    }
    if (type == "float" || type == "double") {
        for (char c : value) {
            if (c == 'x' || c == 'X' || c == 'p' || c == 'P') return false;
        }
        const char* p = value.c_str();
        char* end = nullptr;
        double d = std::strtod(p, &end);
        if (end == p || *end != '\0') return false;
        return std::isfinite(d);
    }
    return false;
}

bool validate(const jsn::JVal& root, bool write_mode, File& out, std::string& err) {
    if (root.kind != jsn::JVal::Kind::Obj) {
        err = "batch: top-level must be an object";
        return false;
    }
    for (const auto& kv : root.obj) {
        if (kv.first != "version" && kv.first != "process" &&
            kv.first != "values") {
            err = "batch: unknown top-level field '" + kv.first + "'";
            return false;
        }
    }

    // version: optional, must be 1 (number or string "1").
    if (const jsn::JVal* ver = jsn::find_member(root, "version")) {
        std::string raw;
        uint64_t v = 0;
        if (!jsn::member_raw(ver, raw) || !jsn::parse_uint(raw, v) || v != 1) {
            err = "batch: invalid version '" + raw + "' (expected 1)";
            return false;
        }
    }

    // process: optional string (validated against the target at execution).
    if (const jsn::JVal* proc = jsn::find_member(root, "process")) {
        if (proc->kind != jsn::JVal::Kind::Str) {
            err = "batch: 'process' must be a string";
            return false;
        }
        out.process = proc->str;
    }

    const jsn::JVal* values = jsn::find_member(root, "values");
    if (!values) {
        err = "batch: missing field 'values'";
        return false;
    }
    if (values->kind != jsn::JVal::Kind::Obj) {
        err = "batch: 'values' must be an object";
        return false;
    }

    static const char* kKnownFields[] = {"module", "symbol", "base",
                                         "offsets", "type", "count", "value"};

    for (const auto& kv : values->obj) {
        OffsetPath item;
        item.name = kv.first;
        if (item.name.empty()) {
            err = "batch: empty value name";
            return false;
        }
        if (kv.second.kind != jsn::JVal::Kind::Obj) {
            err = "item '" + item.name + "': must be an object";
            return false;
        }
        for (const auto& f : kv.second.obj) {
            bool known = false;
            for (const char* k : kKnownFields) {
                if (f.first == k) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                err = "item '" + item.name + "': unknown field '" + f.first + "'";
                return false;
            }
        }

        // Root source: module / symbol / absolute base (mutually exclusive).
        if (const jsn::JVal* mod = jsn::find_member(kv.second, "module")) {
            if (mod->kind != jsn::JVal::Kind::Str) {
                err = "item '" + item.name + "': 'module' must be a string";
                return false;
            }
            item.module = mod->str;
        }
        if (const jsn::JVal* sym = jsn::find_member(kv.second, "symbol")) {
            if (sym->kind != jsn::JVal::Kind::Str || sym->str.empty()) {
                err = "item '" + item.name + "': 'symbol' must be a non-empty string";
                return false;
            }
            item.symbol = sym->str;
        }
        if (!item.module.empty() && !item.symbol.empty()) {
            err = "item '" + item.name +
                  "': 'module' and 'symbol' are mutually exclusive";
            return false;
        }
        const jsn::JVal* base = jsn::find_member(kv.second, "base");
        if (base) {
            std::string raw;
            if (!jsn::member_raw(base, raw) || !jsn::parse_uint(raw, item.base)) {
                err = "item '" + item.name + "': invalid base";
                return false;
            }
        }
        if (item.module.empty() && item.symbol.empty() && !base) {
            err = "item '" + item.name +
                  "': missing locator (need 'base', 'module'+'base' or 'symbol')";
            return false;
        }

        // Offsets: pointer-chain levels (each a uint64).
        if (const jsn::JVal* offs = jsn::find_member(kv.second, "offsets")) {
            if (offs->kind != jsn::JVal::Kind::Arr) {
                err = "item '" + item.name + "': 'offsets' must be an array";
                return false;
            }
            for (const auto& o : offs->arr) {
                std::string raw;
                uint64_t v = 0;
                if (!jsn::member_raw(&o, raw) || !jsn::parse_uint(raw, v)) {
                    err = "item '" + item.name + "': invalid offset";
                    return false;
                }
                item.offsets.push_back(v);
            }
        }

        // Type: one of the 8 supported names.
        const jsn::JVal* type = jsn::find_member(kv.second, "type");
        std::string t;
        if (!type || type->kind != jsn::JVal::Kind::Str || !valid_type(type->str)) {
            err = "item '" + item.name + "': invalid or missing 'type'";
            return false;
        }
        item.type = type->str;

        // count: required for bytes, must be 1..kMaxCount.
        if (const jsn::JVal* cnt = jsn::find_member(kv.second, "count")) {
            std::string raw;
            uint64_t c = 0;
            if (!jsn::member_raw(cnt, raw) || !jsn::parse_uint(raw, c) || c == 0 ||
                c > kMaxCount) {
                err = "item '" + item.name + "': invalid 'count'";
                return false;
            }
            item.count = static_cast<uint32_t>(c);
        }
        if (item.type == "bytes" && item.count == 0) {
            err = "item '" + item.name + "': 'count' required for bytes type";
            return false;
        }

        // value: write mode requires + validates it; read mode ignores it.
        if (const jsn::JVal* val = jsn::find_member(kv.second, "value")) {
            if (val->kind != jsn::JVal::Kind::Str) {
                err = "item '" + item.name + "': 'value' must be a string";
                return false;
            }
            item.value = val->str;
        }
        if (write_mode) {
            if (item.value.empty()) {
                err = "item '" + item.name + "': missing 'value' (write mode)";
                return false;
            }
            if (!valid_type_value(item.type, item.value)) {
                err = "item '" + item.name + "': invalid value for type '" +
                      item.type + "'";
                return false;
            }
            if (item.type == "bytes") {
                size_t n = 0;
                compact_hex_count(item.value, n);
                if (n != item.count) {
                    err = "item '" + item.name + "': 'value' length (" +
                          std::to_string(n) + ") != 'count' (" +
                          std::to_string(item.count) + ")";
                    return false;
                }
            }
        }

        out.items.push_back(std::move(item));
    }
    return true;
}

}  // namespace

bool parse_text(const std::string& text, bool write_mode, File& out,
                std::string& err) {
    if (text.size() > kMaxBatchBytes) {
        err = "batch: file too large (max 1 MiB)";
        return false;
    }
    jsn::JVal root;
    if (!jsn::parse(text, root, err, "batch")) return false;
    return validate(root, write_mode, out, err);
}

bool parse_file(const std::string& path, bool write_mode, File& out,
                std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        err = "batch: cannot read file: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return parse_text(ss.str(), write_mode, out, err);
}

}  // namespace batch
}  // namespace deeptrace_cli

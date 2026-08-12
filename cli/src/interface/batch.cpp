#include "interface/batch.h"

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

// ---- minimal recursive-descent JSON parser (standard library only) ----
// Produces a small value tree: objects (ordered members), arrays, strings,
// numbers (kept as raw literal text), booleans, null.

struct JVal {
    enum class Kind { Null, Bool, Num, Str, Arr, Obj };
    Kind kind = Kind::Null;
    bool b = false;
    std::string str;  // string content, or raw number literal text
    std::vector<JVal> arr;
    std::vector<std::pair<std::string, JVal>> obj;
};

class JsonParser {
public:
    explicit JsonParser(const std::string& text) : text_(text) {}

    bool parse(JVal& out, std::string& err) {
        skip_ws();
        if (!parse_value(out, err)) return false;
        skip_ws();
        if (i_ != text_.size()) return fail("trailing content", err);
        return true;
    }

private:
    const std::string& text_;
    size_t i_ = 0;

    char peek() const { return i_ < text_.size() ? text_[i_] : '\0'; }

    void skip_ws() {
        while (i_ < text_.size() &&
               (text_[i_] == ' ' || text_[i_] == '\t' || text_[i_] == '\r' ||
                text_[i_] == '\n'))
            ++i_;
    }

    bool parse_value(JVal& out, std::string& err) {
        skip_ws();
        char c = peek();
        if (c == '"') {
            out.kind = JVal::Kind::Str;
            return parse_string(out.str, err);
        }
        if (c == '{') {
            out.kind = JVal::Kind::Obj;
            return parse_object(out, err);
        }
        if (c == '[') {
            out.kind = JVal::Kind::Arr;
            return parse_array(out, err);
        }
        if (c == 't') {
            return parse_literal("true", out, true, err);
        }
        if (c == 'f') {
            return parse_literal("false", out, false, err);
        }
        if (c == 'n') {
            return parse_literal("null", out, false, err);
        }
        if (c == '-' || (c >= '0' && c <= '9')) {
            out.kind = JVal::Kind::Num;
            return parse_number(out.str, err);
        }
        return fail("unexpected character", err);
    }

    // Consume a fixed keyword literal ("true"/"false"/"null").
    bool parse_literal(const char* kw, JVal& out, bool value, std::string& err) {
        size_t len = 0;
        while (kw[len]) ++len;
        if (text_.compare(i_, len, kw) != 0) return fail("invalid literal", err);
        i_ += len;
        out.kind = JVal::Kind::Bool;  // "null" is also parked here (kind Bool, b=false)
        out.b = value;
        return true;
    }

    bool parse_object(JVal& out, std::string& err) {
        ++i_;  // '{'
        skip_ws();
        if (peek() == '}') {
            ++i_;
            return true;
        }
        for (;;) {
            skip_ws();
            std::string key;
            if (!parse_string(key, err)) return false;
            skip_ws();
            if (peek() != ':') return fail("expected ':'", err);
            ++i_;
            JVal v;
            if (!parse_value(v, err)) return false;
            // Duplicate keys: last occurrence wins (JSON semantics).
            bool replaced = false;
            for (auto& kv : out.obj) {
                if (kv.first == key) {
                    kv.second = std::move(v);
                    replaced = true;
                    break;
                }
            }
            if (!replaced) out.obj.emplace_back(std::move(key), std::move(v));
            skip_ws();
            char c = peek();
            if (c == ',') {
                ++i_;
                continue;
            }
            if (c == '}') {
                ++i_;
                return true;
            }
            return fail("expected ',' or '}'", err);
        }
    }

    bool parse_array(JVal& out, std::string& err) {
        ++i_;  // '['
        skip_ws();
        if (peek() == ']') {
            ++i_;
            return true;
        }
        for (;;) {
            JVal v;
            if (!parse_value(v, err)) return false;
            out.arr.push_back(std::move(v));
            skip_ws();
            char c = peek();
            if (c == ',') {
                ++i_;
                continue;
            }
            if (c == ']') {
                ++i_;
                return true;
            }
            return fail("expected ',' or ']'", err);
        }
    }

    bool parse_number(std::string& out, std::string& err) {
        size_t start = i_;
        if (peek() == '-') ++i_;
        bool any = false;
        while (i_ < text_.size() &&
               (text_[i_] >= '0' && text_[i_] <= '9')) {
            ++i_;
            any = true;
        }
        if (!any) return fail("invalid number", err);
        if (i_ < text_.size() && text_[i_] == '.') {  // fractional part
            ++i_;
            while (i_ < text_.size() && (text_[i_] >= '0' && text_[i_] <= '9')) ++i_;
        }
        if (i_ < text_.size() && (text_[i_] == 'e' || text_[i_] == 'E')) {
            ++i_;
            if (i_ < text_.size() && (text_[i_] == '+' || text_[i_] == '-')) ++i_;
            bool exp_any = false;
            while (i_ < text_.size() && (text_[i_] >= '0' && text_[i_] <= '9')) {
                ++i_;
                exp_any = true;
            }
            if (!exp_any) return fail("invalid number exponent", err);
        }
        out = text_.substr(start, i_ - start);
        return true;
    }

    // String values; supported escapes: \" and \\ (mirrors script.cpp).
    bool parse_string(std::string& out, std::string& err) {
        skip_ws();
        if (peek() != '"') return fail("expected '\"'", err);
        ++i_;
        std::string s;
        while (i_ < text_.size()) {
            char c = text_[i_];
            if (c == '"') {
                ++i_;
                out = s;
                return true;
            }
            if (c == '\\') {
                ++i_;
                if (i_ >= text_.size()) return fail("unterminated escape", err);
                char e = text_[i_];
                if (e == '"') {
                    s.push_back('"');
                    ++i_;
                } else if (e == '\\') {
                    s.push_back('\\');
                    ++i_;
                } else {
                    return fail("unsupported escape", err);
                }
            } else {
                s.push_back(c);
                ++i_;
            }
        }
        return fail("unterminated string", err);
    }

    bool fail(const char* msg, std::string& err) {
        size_t line = 1, col = 1;
        for (size_t k = 0; k < i_ && k < text_.size(); ++k) {
            if (text_[k] == '\n') {
                ++line;
                col = 1;
            } else {
                ++col;
            }
        }
        std::ostringstream os;
        os << "batch parse error at line " << line << " col " << col << ": " << msg;
        err = os.str();
        return false;
    }
};

// ---- validation helpers ----

bool parse_uint(const std::string& s, uint64_t& out) {
    if (s.empty()) return false;
    size_t i = 0;
    int base = 10;
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        base = 16;
        i = 2;
        if (i >= s.size()) return false;
    }
    uint64_t v = 0;
    for (; i < s.size(); ++i) {
        char c = s[i];
        int d = -1;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (base == 16 && c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (base == 16 && c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return false;
        if (v > (UINT64_MAX - static_cast<uint64_t>(d)) / static_cast<uint64_t>(base))
            return false;  // overflow
        v = v * static_cast<uint64_t>(base) + static_cast<uint64_t>(d);
    }
    out = v;
    return true;
}

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
        if (!parse_uint(value, val)) return false;
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

const JVal* find_member(const JVal& o, const std::string& key) {
    for (const auto& kv : o.obj) {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

// Number-like member: returns raw literal text for Str/Num kinds.
bool member_raw(const JVal* v, std::string& raw) {
    if (!v) return false;
    if (v->kind == JVal::Kind::Str || v->kind == JVal::Kind::Num) {
        raw = v->str;
        return true;
    }
    return false;
}

bool validate(const JVal& root, bool write_mode, File& out, std::string& err) {
    if (root.kind != JVal::Kind::Obj) {
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
    if (const JVal* ver = find_member(root, "version")) {
        std::string raw;
        uint64_t v = 0;
        if (!member_raw(ver, raw) || !parse_uint(raw, v) || v != 1) {
            err = "batch: invalid version '" + raw + "' (expected 1)";
            return false;
        }
    }

    // process: optional string (validated against the target at execution).
    if (const JVal* proc = find_member(root, "process")) {
        if (proc->kind != JVal::Kind::Str) {
            err = "batch: 'process' must be a string";
            return false;
        }
        out.process = proc->str;
    }

    const JVal* values = find_member(root, "values");
    if (!values) {
        err = "batch: missing field 'values'";
        return false;
    }
    if (values->kind != JVal::Kind::Obj) {
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
        if (kv.second.kind != JVal::Kind::Obj) {
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
        if (const JVal* mod = find_member(kv.second, "module")) {
            if (mod->kind != JVal::Kind::Str) {
                err = "item '" + item.name + "': 'module' must be a string";
                return false;
            }
            item.module = mod->str;
        }
        if (const JVal* sym = find_member(kv.second, "symbol")) {
            if (sym->kind != JVal::Kind::Str || sym->str.empty()) {
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
        const JVal* base = find_member(kv.second, "base");
        if (base) {
            std::string raw;
            if (!member_raw(base, raw) || !parse_uint(raw, item.base)) {
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
        if (const JVal* offs = find_member(kv.second, "offsets")) {
            if (offs->kind != JVal::Kind::Arr) {
                err = "item '" + item.name + "': 'offsets' must be an array";
                return false;
            }
            for (const auto& o : offs->arr) {
                std::string raw;
                uint64_t v = 0;
                if (!member_raw(&o, raw) || !parse_uint(raw, v)) {
                    err = "item '" + item.name + "': invalid offset";
                    return false;
                }
                item.offsets.push_back(v);
            }
        }

        // Type: one of the 8 supported names.
        const JVal* type = find_member(kv.second, "type");
        std::string t;
        if (!type || type->kind != JVal::Kind::Str || !valid_type(type->str)) {
            err = "item '" + item.name + "': invalid or missing 'type'";
            return false;
        }
        item.type = type->str;

        // count: required for bytes, must be 1..kMaxCount.
        if (const JVal* cnt = find_member(kv.second, "count")) {
            std::string raw;
            uint64_t c = 0;
            if (!member_raw(cnt, raw) || !parse_uint(raw, c) || c == 0 ||
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
        if (const JVal* val = find_member(kv.second, "value")) {
            if (val->kind != JVal::Kind::Str) {
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
    JVal root;
    JsonParser parser(text);
    if (!parser.parse(root, err)) return false;
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

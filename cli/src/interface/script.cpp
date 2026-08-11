#include "interface/script.h"

#include <cstdint>
#include <fstream>
#include <sstream>
#include <utility>

namespace deeptrace_cli {
namespace script {

namespace {

constexpr size_t kMaxScriptBytes = 1024 * 1024;  // 1 MiB

// ---- field value validation (rules mirror command/parser.cpp) ----

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
            return false;
        v = v * static_cast<uint64_t>(base) + static_cast<uint64_t>(d);
    }
    out = v;
    return true;
}

bool valid_address(const std::string& s) {
    uint64_t v;
    if (!parse_uint(s, v)) return false;
    return v > 0 && v <= 0x7FFFFFFFFFFFULL;
}

bool valid_number(const std::string& s) {
    uint64_t v;
    return parse_uint(s, v);
}

bool valid_number_pos(const std::string& s) {
    uint64_t v;
    if (!parse_uint(s, v)) return false;
    return v > 0;
}

bool valid_tid(const std::string& s) {
    uint64_t v;
    if (!parse_uint(s, v)) return false;
    return v <= 0xFFFFFFFFULL;
}

bool valid_index(const std::string& s) {
    uint64_t v;
    return parse_uint(s, v);
}

bool valid_ascii(const std::string& s) {
    if (s.empty()) return false;
    for (unsigned char c : s) {
        if (c < 0x20 || c > 0x7E) return false;  // printable ASCII only
    }
    return true;
}

bool valid_format(const std::string& s) {
    return s == "hex" || s == "text";
}

// Hex bytes: continuous or space-separated tokens, optional 0x prefix.
bool valid_hex_bytes(const std::string& s) {
    std::string compact;
    for (char c : s) {
        if (c != ' ' && c != '\t') compact.push_back(c);
    }
    size_t start = 0;
    if (compact.size() >= 2 && compact[0] == '0' && (compact[1] == 'x' || compact[1] == 'X'))
        start = 2;
    size_t n = compact.size() - start;
    if (n == 0 || (n % 2) != 0) return false;
    for (size_t i = start; i < compact.size(); ++i) {
        char c = compact[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

bool valid_hw_type(const std::string& s) {
    return s == "0" || s == "1" || s == "2";
}

bool valid_hw_length(const std::string& s) {
    return s == "1" || s == "2" || s == "4" || s == "8";
}

bool valid_value_type(const std::string& s) {
    return s == "byte" || s == "word" || s == "dword" || s == "qword" ||
           s == "float" || s == "double";
}

bool valid_field(const std::string& kind, const std::string& v) {
    if (kind == "address") return valid_address(v);
    if (kind == "number") return valid_number(v);
    if (kind == "number-pos") return valid_number_pos(v);
    if (kind == "tid") return valid_tid(v);
    if (kind == "index") return valid_index(v);
    if (kind == "ascii") return valid_ascii(v);
    if (kind == "format") return valid_format(v);
    if (kind == "hex-bytes") return valid_hex_bytes(v);
    if (kind == "hw-type") return valid_hw_type(v);
    if (kind == "hw-length") return valid_hw_length(v);
    if (kind == "value-type") return valid_value_type(v);
    return false;
}

// ---- op rules: field name / kind / required / default ----

struct FieldRule {
    const char* name;
    const char* kind;
    bool required;
    const char* def;  // default injected when optional field is absent
};

struct OpRule {
    const char* op;
    std::vector<FieldRule> fields;
};

const std::vector<OpRule>& op_table() {
    static const std::vector<OpRule> kTable = {
        {"break", {{"addr", "address", true, nullptr}}},
        {"clear", {{"addr", "address", true, nullptr}}},
        {"hbreak", {{"addr", "address", true, nullptr},
                    {"type", "hw-type", true, nullptr},
                    {"length", "hw-length", true, nullptr}}},
        {"hclear", {{"addr", "address", true, nullptr}}},
        {"guard", {{"addr", "address", true, nullptr},
                   {"size", "number-pos", true, nullptr}}},
        {"unguard", {{"addr", "address", true, nullptr},
                     {"size", "number-pos", true, nullptr}}},
        {"pause", {}},
        {"resume", {}},
        {"step", {{"tid", "tid", false, "0"}}},
        {"next", {{"tid", "tid", false, "0"}}},
        {"registers", {{"tid", "tid", false, "0"}}},
        {"register", {{"name", "ascii", true, nullptr},
                      {"tid", "tid", false, "0"}}},
        {"status", {}},
        {"continue", {{"timeout_ms", "number-pos", false, "5000"}}},
        {"read", {{"addr", "address", true, nullptr},
                  {"size", "number", true, nullptr},
                  {"format", "format", false, "hex"}}},
        {"write", {{"addr", "address", true, nullptr},
                   {"bytes", "hex-bytes", true, nullptr}}},
        {"disasm", {{"addr", "address", true, nullptr},
                    {"count", "number-pos", false, "8"}}},
        {"watch_add", {{"desc", "ascii", true, nullptr},
                       {"addr", "address", true, nullptr},
                       {"type", "value-type", true, nullptr}}},
        {"watch_remove", {{"index", "index", true, nullptr}}},
        {"watch_clear", {}},
        {"watch_refresh", {}},
        {"watch_list", {}},
    };
    return kTable;
}

const OpRule* find_op(const std::string& op) {
    for (const auto& r : op_table()) {
        if (r.op == op) return &r;
    }
    return nullptr;
}

// ---- minimal JSON parser: top-level array of objects, string values ----

class JsonReader {
public:
    explicit JsonReader(const std::string& text) : text_(text) {}

    bool parse(std::vector<std::map<std::string, std::string>>& out, std::string& err) {
        skip_ws();
        if (peek() != '[') {
            fail("top-level must be an array", err);
            return false;
        }
        ++i_;
        skip_ws();
        if (peek() == ']') {  // empty script: valid, zero steps
            ++i_;
            return true;
        }
        for (;;) {
            std::map<std::string, std::string> obj;
            if (!parse_object(obj, err)) return false;
            out.push_back(std::move(obj));
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
            fail("expected ',' or ']'", err);
            return false;
        }
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

    bool parse_object(std::map<std::string, std::string>& obj, std::string& err) {
        skip_ws();
        if (peek() != '{') {
            fail("step must be an object", err);
            return false;
        }
        ++i_;
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
            if (peek() != ':') {
                fail("expected ':'", err);
                return false;
            }
            ++i_;
            skip_ws();
            std::string value;
            if (!parse_string(value, err)) return false;
            obj[key] = value;
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
            fail("expected ',' or '}'", err);
            return false;
        }
    }

    // String values; supported escapes: \" and \\ (per design).
    bool parse_string(std::string& out, std::string& err) {
        skip_ws();
        if (peek() != '"') {
            fail("expected '\"'", err);
            return false;
        }
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
                if (i_ >= text_.size()) {
                    fail("unterminated escape", err);
                    return false;
                }
                char e = text_[i_];
                if (e == '"') {
                    s.push_back('"');
                    ++i_;
                } else if (e == '\\') {
                    s.push_back('\\');
                    ++i_;
                } else {
                    fail("unsupported escape", err);
                    return false;
                }
            } else {
                s.push_back(c);
                ++i_;
            }
        }
        fail("unterminated string", err);
        return false;
    }

    void fail(const char* msg, std::string& err) {
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
        os << "script parse error at line " << line << " col " << col << ": " << msg;
        err = os.str();
    }
};

// ---- validation: op table + required fields + per-kind values ----

bool validate_steps(const std::vector<std::map<std::string, std::string>>& raw,
                    std::vector<Step>& out, std::string& err) {
    for (size_t i = 0; i < raw.size(); ++i) {
        const auto& obj = raw[i];
        const size_t num = i + 1;  // 1-based in messages

        auto it = obj.find("op");
        if (it == obj.end()) {
            err = "step " + std::to_string(num) + ": missing field 'op'";
            return false;
        }
        const std::string& op = it->second;
        const OpRule* rule = find_op(op);
        if (!rule) {
            err = "step " + std::to_string(num) + ": unknown op '" + op + "'";
            return false;
        }

        // Unknown fields are rejected (catches typos such as 'adr').
        for (const auto& kv : obj) {
            if (kv.first == "op") continue;
            bool known = false;
            for (const auto& f : rule->fields) {
                if (f.name == kv.first) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                err = "step " + std::to_string(num) + " (" + op +
                      "): unknown field '" + kv.first + "'";
                return false;
            }
        }

        Step step;
        step.op = op;
        for (const auto& f : rule->fields) {
            auto fit = obj.find(f.name);
            if (fit == obj.end()) {
                if (f.required) {
                    err = "step " + std::to_string(num) + " (" + op +
                          "): missing field '" + f.name + "'";
                    return false;
                }
                if (f.def) step.fields[f.name] = f.def;  // inject default
                continue;
            }
            if (!valid_field(f.kind, fit->second)) {
                err = "step " + std::to_string(num) + " (" + op + "): invalid " +
                      f.name + " '" + fit->second + "'";
                return false;
            }
            step.fields[f.name] = fit->second;
        }
        out.push_back(std::move(step));
    }
    return true;
}

}  // namespace

bool parse_text(const std::string& text, std::vector<Step>& out, std::string& err) {
    if (text.size() > kMaxScriptBytes) {
        err = "script too large (max 1 MiB)";
        return false;
    }
    std::vector<std::map<std::string, std::string>> raw;
    JsonReader reader(text);
    if (!reader.parse(raw, err)) return false;
    return validate_steps(raw, out, err);
}

bool parse_file(const std::string& path, std::vector<Step>& out, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        err = "cannot open script file: " + path;
        return false;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return parse_text(ss.str(), out, err);
}

}  // namespace script
}  // namespace deeptrace_cli

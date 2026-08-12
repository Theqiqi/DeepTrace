#include "interface/json.h"

#include <cstdint>
#include <sstream>

namespace deeptrace_cli {
namespace jsn {

namespace {

class JsonParser {
public:
    JsonParser(const std::string& text, const std::string& prefix)
        : text_(text), prefix_(prefix) {}

    bool parse(JVal& out, std::string& err) {
        skip_ws();
        if (!parse_value(out, err)) return false;
        skip_ws();
        if (i_ != text_.size()) return fail("trailing content", err);
        return true;
    }

private:
    const std::string& text_;
    const std::string& prefix_;
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
        if (c == 't') return parse_literal("true", out, true, err);
        if (c == 'f') return parse_literal("false", out, false, err);
        if (c == 'n') return parse_literal("null", out, false, err);
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
        while (i_ < text_.size() && (text_[i_] >= '0' && text_[i_] <= '9')) {
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

    // String values; supported escapes: \" and \\.
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
        os << prefix_ << " parse error at line " << line << " col " << col
           << ": " << msg;
        err = os.str();
        return false;
    }
};

}  // namespace

bool parse(const std::string& text, JVal& out, std::string& err,
           const std::string& prefix) {
    JVal root;
    JsonParser parser(text, prefix);
    if (!parser.parse(root, err)) return false;
    out = std::move(root);
    return true;
}

const JVal* find_member(const JVal& o, const std::string& key) {
    for (const auto& kv : o.obj) {
        if (kv.first == key) return &kv.second;
    }
    return nullptr;
}

bool member_raw(const JVal* v, std::string& raw) {
    if (!v) return false;
    if (v->kind == JVal::Kind::Str || v->kind == JVal::Kind::Num) {
        raw = v->str;
        return true;
    }
    return false;
}

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

}  // namespace jsn
}  // namespace deeptrace_cli

#include "command/parser.h"

#include "command/commands.h"

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace deeptrace_cli {

namespace {

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

bool parse_decimal(const std::string& s, uint64_t& out) {
    if (s.empty()) return false;
    uint64_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') return false;
        if (v > (UINT64_MAX - static_cast<uint64_t>(c - '0')) / 10) return false;
        v = v * 10 + static_cast<uint64_t>(c - '0');
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

bool valid_pid_tid(const std::string& s) {
    uint64_t v;
    if (!parse_decimal(s, v)) return false;
    return v >= 1 && v <= 0xFFFFFFFFULL;
}

bool valid_exit_code(const std::string& s) {
    uint64_t v;
    if (!parse_decimal(s, v)) return false;
    return v <= 0xFFFFFFFFULL;
}

bool valid_index(const std::string& s) {
    uint64_t v;
    return parse_decimal(s, v);
}

bool valid_string(const std::string& s) {
    return !s.empty();
}

bool valid_format(const std::string& s) {
    return s == "hex" || s == "dec" || s == "bin" || s == "ascii";
}

bool valid_format_rw(const std::string& s) {
    return s == "hex" || s == "dec";
}

bool valid_value_type(const std::string& s) {
    return s == "byte" || s == "word" || s == "dword" || s == "qword" ||
           s == "float" || s == "double";
}

bool valid_hw_type(const std::string& s) {
    return s == "0" || s == "1" || s == "2";
}

// AOB pattern: space-separated bytes; each token is "??" or exactly 2 hex chars.
bool valid_pattern(const std::string& s) {
    if (s.empty()) return false;
    size_t count = 0;
    std::string tok;
    auto flush = [&]() -> bool {
        if (tok.empty()) return true;
        if (tok == "??" || tok == "?") {
            tok.clear();
            ++count;
            return true;
        }
        if (tok.size() != 2) return false;
        for (char c : tok) {
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F')))
                return false;
        }
        tok.clear();
        ++count;
        return true;
    };
    for (char c : s) {
        if (c == ' ' || c == '\t') {
            if (!flush()) return false;
        } else {
            tok.push_back(c);
        }
    }
    if (!flush()) return false;
    return count >= 1 && count <= 64;
}

bool valid_scan_type(const std::string& s) {
    return s == "byte" || s == "word" || s == "dword" || s == "qword" ||
           s == "float" || s == "double" || s == "string" || s == "hex" ||
           s == "pattern";
}

// Value validity depends on its type; used as a cross-field check after all
// params of `resolve scan` are collected.
bool valid_scan_value(const std::string& v, const std::string& type) {
    if (v.empty()) return false;
    if (type == "pattern") return valid_pattern(v);
    if (type == "hex") return valid_hex_bytes(v);
    if (type == "string") {
        for (unsigned char c : v) {
            if (c < 0x20 || c > 0x7E) return false;  // printable ASCII only
        }
        return true;
    }
    if (type == "byte" || type == "word" || type == "dword" || type == "qword") {
        uint64_t val = 0;
        if (!parse_uint(v, val)) return false;
        uint64_t maxv = 0;
        if (type == "byte") maxv = 0xFFULL;
        else if (type == "word") maxv = 0xFFFFULL;
        else if (type == "dword") maxv = 0xFFFFFFFFULL;
        else maxv = UINT64_MAX;
        return val <= maxv;
    }
    if (type == "float") {
        const char* p = v.c_str();
        char* end = nullptr;
        float f = std::strtof(p, &end);
        if (end == p || *end != '\0') return false;
        return std::isfinite(f);
    }
    if (type == "double") {
        const char* p = v.c_str();
        char* end = nullptr;
        double d = std::strtod(p, &end);
        if (end == p || *end != '\0') return false;
        return std::isfinite(d);
    }
    return false;
}

// Hex byte string: optional 0x prefix, even number of hex digits.
bool valid_hex_bytes(const std::string& s) {
    size_t start = 0;
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) start = 2;
    size_t n = s.size() - start;
    if (n == 0 || (n % 2) != 0) return false;
    for (size_t i = start; i < s.size(); ++i) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
              (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

bool valid_param(const ParamSpec& p, const std::string& v) {
    const std::string& t = p.type;
    if (t == "address") return valid_address(v);
    if (t == "number") return valid_number(v);
    if (t == "pid" || t == "tid") return valid_pid_tid(v);
    if (t == "string") return valid_string(v);
    if (t == "format") return valid_format(v);
    if (t == "format-rw") return valid_format_rw(v);
    if (t == "value-type") return valid_value_type(v);
    if (t == "hw-type") return valid_hw_type(v);
    if (t == "pattern") return valid_pattern(v);
    if (t == "hex-bytes") return valid_hex_bytes(v);
    if (t == "scan-type") return valid_scan_type(v);
    if (t == "scan-value") return !v.empty();  // full check depends on type
    if (t == "exit-code") return valid_exit_code(v);
    if (t == "index") return valid_index(v);
    if (t == "flag") return v == p.name;
    return true;
}

}  // namespace

ParseResult parse_args(int argc, char* argv[]) {
    ParseResult res;

    std::vector<std::string> pos;  // positional tokens (command group/action/args)
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-h" || a == "--help") {
            res.req.help = true;
            continue;
        }
        if (a == "-v" || a == "--version") {
            res.req.version = true;
            continue;
        }
        if (a == "-p" || a == "--pid") {
            if (i + 1 >= argc) {
                res.ok = false;
                res.exit_code = 2;
                res.error = "missing argument for option: " + a;
                return res;
            }
            std::string pv = argv[++i];
            uint64_t pid = 0;
            if (!parse_decimal(pv, pid) || pid < 1 || pid > 0xFFFFFFFFULL) {
                res.ok = false;
                res.exit_code = 2;
                res.error = "invalid pid: '" + pv + "'";
                return res;
            }
            res.req.pid = static_cast<uint32_t>(pid);
            res.req.pid_set = true;
            continue;
        }
        if (!a.empty() && a[0] == '-' && a.size() > 1 && a != "--hex" && a != "--c-array") {
            res.ok = false;
            res.exit_code = 2;
            res.error = "unknown option: " + a;
            return res;
        }
        pos.push_back(a);
    }

    if (res.req.help || res.req.version) {
        return res;  // caller prints help/version and exits 0
    }

    if (pos.empty()) {
        res.ok = false;
        res.exit_code = 1;
        res.error = "Missing command. Use -h or --help for help.";
        return res;
    }

    res.req.group = pos[0];
    if (pos.size() < 2) {
        res.ok = false;
        res.exit_code = 2;
        res.error = "missing subcommand for group: '" + res.req.group + "'";
        return res;
    }
    res.req.action = pos[1];

    const CommandSpec* spec = find_command(res.req.group, res.req.action);
    if (!spec) {
        res.ok = false;
        res.exit_code = 2;
        if (is_group(res.req.group)) {
            res.error = "unknown command: '" + res.req.action + "'";
        } else {
            res.error = "unknown command group: '" + res.req.group + "'";
        }
        return res;
    }

    // Validate parameters against the spec.
    std::vector<std::string> args(pos.begin() + 2, pos.end());
    size_t ai = 0;
    for (const auto& p : spec->params) {
        if (p.type == "flag") {
            // optional flag: consume only if present and matching
            if (ai < args.size() && args[ai] == p.name) {
                res.req.args.push_back(args[ai]);
                ++ai;
            } else {
                res.req.args.push_back("");  // not set
            }
            continue;
        }
        if (ai < args.size()) {
            const std::string& v = args[ai];
            if (!valid_param(p, v)) {
                res.ok = false;
                res.exit_code = 2;
                res.error = "invalid " + p.name + ": '" + v + "'";
                return res;
            }
            res.req.args.push_back(v);
            ++ai;
        } else if (p.required) {
            res.ok = false;
            res.exit_code = 2;
            res.error = "missing argument: " + p.name;
            return res;
        } else {
            res.req.args.push_back(p.def);  // default
        }
    }
    if (ai < args.size()) {
        res.ok = false;
        res.exit_code = 2;
        res.error = "too many arguments: '" + args[ai] + "'";
        return res;
    }

    // Cross-field check: for `resolve scan`, value validity depends on type.
    if (res.req.group == "resolve" && res.req.action == "scan") {
        const std::string& value = res.req.args[0];
        const std::string& type = res.req.args[1];
        if (!valid_scan_value(value, type)) {
            res.ok = false;
            res.exit_code = 2;
            res.error = "invalid value for type '" + type + "': '" + value + "'";
            return res;
        }
    }

    return res;
}

}  // namespace deeptrace_cli

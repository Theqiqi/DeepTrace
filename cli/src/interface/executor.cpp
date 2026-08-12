#include "interface/executor.h"

#include "interface/cmd.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace deeptrace_cli {

namespace internal {

int report_error(deeptrace::Result r, const std::string& ctx) {
    std::string msg = deeptrace::result_message(r);
    if (!ctx.empty()) msg += "(" + ctx + ")";
    printer::print_error(msg);
    return 1;
}

uint64_t to_u64(const std::string& s) {
    return static_cast<uint64_t>(std::strtoull(s.c_str(), nullptr, 0));
}

uint32_t to_u32(const std::string& s) {
    return static_cast<uint32_t>(std::strtoull(s.c_str(), nullptr, 10));
}

int to_int(const std::string& s) {
    return static_cast<int>(std::strtoll(s.c_str(), nullptr, 10));
}

uintptr_t to_addr(const std::string& s) {
    return static_cast<uintptr_t>(std::strtoull(s.c_str(), nullptr, 0));
}

deeptrace::Result resolve_addr(const std::string& s, uintptr_t& out) {
    // v2.6.0 symbol addressing: numeric addresses keep working unchanged; a
    // non-numeric identifier is resolved as a script symbol (requires the
    // attached session, so callers must run inside an attached command).
    //
    // The parser guarantees the token is either a valid address (decimal or
    // 0x-prefixed hex, first char a digit) or a valid symbol shape (first
    // char a letter/underscore), so the first char disambiguates. Parse with
    // an explicit base to avoid strtoull base-0 octal pitfalls (010 -> 8).
    if (!s.empty() && s[0] >= '0' && s[0] <= '9') {
        bool hex = s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X');
        const char* p = s.c_str() + (hex ? 2 : 0);
        out = static_cast<uintptr_t>(std::strtoull(p, nullptr, hex ? 16 : 10));
        return deeptrace::Result::Ok;
    }
    return deeptrace::script_symbol(s, &out);
}

std::vector<uint8_t> hex_bytes(const std::string& s) {
    size_t start = 0;
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) start = 2;
    auto hexv = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    std::vector<uint8_t> out;
    for (size_t i = start; i + 1 < s.size() + 1; i += 2) {
        if (i + 1 >= s.size()) break;
        out.push_back(static_cast<uint8_t>((hexv(s[i]) << 4) | hexv(s[i + 1])));
    }
    return out;
}

bool typed_bytes(const std::string& value, const std::string& type,
                 std::vector<uint8_t>& out) {
    if (type == "hex") {
        out = hex_bytes(value);
        return !out.empty();
    }
    if (type == "string") {
        out.assign(value.begin(), value.end());
        return !out.empty();
    }
    if (type == "byte" || type == "word" || type == "dword" || type == "qword") {
        // value was validated as an unsigned integer (dec or 0x-prefixed hex);
        // parse with the same base rule as the parser to avoid octal pitfalls.
        bool hex_pref = value.size() >= 2 && value[0] == '0' &&
                        (value[1] == 'x' || value[1] == 'X');
        unsigned long long v = std::strtoull(value.c_str() + (hex_pref ? 2 : 0),
                                             nullptr, hex_pref ? 16 : 10);
        size_t width = type == "byte" ? 1 : type == "word" ? 2
                     : type == "dword" ? 4 : 8;
        out.clear();
        for (size_t i = 0; i < width; ++i) {
            out.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
        }
        return true;
    }
    if (type == "float") {
        float f = std::strtof(value.c_str(), nullptr);
        uint32_t bits = 0;
        std::memcpy(&bits, &f, sizeof(bits));
        out.clear();
        for (int i = 0; i < 4; ++i) {
            out.push_back(static_cast<uint8_t>((bits >> (8 * i)) & 0xFF));
        }
        return true;
    }
    if (type == "double") {
        double d = std::strtod(value.c_str(), nullptr);
        uint64_t bits = 0;
        std::memcpy(&bits, &d, sizeof(bits));
        out.clear();
        for (int i = 0; i < 8; ++i) {
            out.push_back(static_cast<uint8_t>((bits >> (8 * i)) & 0xFF));
        }
        return true;
    }
    return false;
}

int value_type_id(const std::string& s) {
    if (s == "byte") return 0;
    if (s == "word") return 1;
    if (s == "dword") return 2;
    if (s == "qword") return 3;
    if (s == "float") return 4;
    return 5;  // double
}

bool read_text_file(const std::string& path, std::string& out) {
    std::ifstream f(path);
    if (!f.is_open()) return false;
    std::string line;
    while (std::getline(f, line)) {
        out += line;
        out += '\n';
    }
    return true;
}

bool read_binary_file(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f.is_open()) return false;
    std::streampos end = f.tellg();
    if (end < 0) return false;
    out.resize(static_cast<size_t>(end));
    f.seekg(0, std::ios::beg);
    if (out.empty()) return true;
    f.read(reinterpret_cast<char*>(out.data()),
           static_cast<std::streamsize>(out.size()));
    return static_cast<bool>(f);
}

bool write_binary_file(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return false;
    if (!bytes.empty()) {
        f.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    }
    return static_cast<bool>(f);
}

bool write_text_file(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open()) return false;
    f << text;
    return static_cast<bool>(f);
}

bool is_hex_string(const std::string& s) {
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

bool has_suffix(const std::string& s, const char* suffix) {
    size_t sl = s.size();
    size_t fl = std::strlen(suffix);
    if (sl < fl) return false;
    size_t off = sl - fl;
    for (size_t i = 0; i < fl; ++i) {
        char a = s[off + i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = static_cast<char>(a - 'A' + 'a');
        if (a != b) return false;
    }
    return true;
}

deeptrace::Result resolve_source(const std::string& source, bool asm_ok,
                                 std::vector<uint8_t>& out) {
    if (is_hex_string(source)) {
        out = hex_bytes(source);
        return out.empty() ? deeptrace::Result::InvalidArg : deeptrace::Result::Ok;
    }
    bool asm_src = has_suffix(source, ".asm") || has_suffix(source, ".s");
    if (asm_src && !asm_ok) return deeptrace::Result::InvalidArg;  // alloc: .asm not accepted
    std::ifstream probe(source);
    if (!probe.is_open()) return deeptrace::Result::InvalidArg;
    probe.close();
    if (asm_src) {
        std::string text;
        if (!read_text_file(source, text)) return deeptrace::Result::InvalidArg;
        return deeptrace::asm_assemble(text, out, nullptr);  // BadFormat on syntax error
    }
    return read_binary_file(source, out) ? deeptrace::Result::Ok
                                         : deeptrace::Result::InvalidArg;
}

}  // namespace internal

namespace {

bool needs_session_attach(const CommandRequest& req) {
    if (!req.pid_set) return false;
    if (req.group == "ps") {
        // list/attach/detach manage the session themselves
        return req.action != "list" && req.action != "attach" && req.action != "detach";
    }
    if (req.group == "script" && req.action == "check") {
        return false;  // check is pure local validation; no target session needed
    }
    return true;
}

}  // namespace

int execute(const CommandRequest& req) {
    bool attached = false;
    if (needs_session_attach(req)) {
        deeptrace::Result r = deeptrace::attach(req.pid);
        if (r != deeptrace::Result::Ok) {
            internal::report_error(r, "attach " + std::to_string(req.pid));
            return 1;
        }
        attached = true;
    }

    int rc = 1;
    if (req.group == "ps") rc = cmd_ps(req);
    else if (req.group == "mem") rc = cmd_mem(req);
    else if (req.group == "module") rc = cmd_module(req);
    else if (req.group == "thread") rc = cmd_thread(req);
    else if (req.group == "debug") rc = cmd_debug(req);
    else if (req.group == "disasm") rc = cmd_disasm(req);
    else if (req.group == "resolve") rc = cmd_resolve(req);
    else if (req.group == "watch") rc = cmd_watch(req);
    else if (req.group == "dll") rc = cmd_dll(req);
    else if (req.group == "asm") rc = cmd_asm(req);
    else if (req.group == "shellcode") rc = cmd_shellcode(req);
    else if (req.group == "convert") rc = cmd_convert(req);
    else if (req.group == "hex2bin") rc = cmd_hex2bin(req);
    else if (req.group == "script") rc = cmd_script(req);
    else {
        printer::print_error("unknown command group: '" + req.group + "'");
    }

    if (attached) deeptrace::detach();
    return rc;
}

}  // namespace deeptrace_cli

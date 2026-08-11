#include "interface/executor.h"

#include "interface/cmd.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
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

std::string value_to_pattern(const std::string& value, const std::string& type) {
    auto to_hex = [](uint8_t b) {
        static const char* digits = "0123456789ABCDEF";
        std::string s(2, ' ');
        s[0] = digits[b >> 4];
        s[1] = digits[b & 0x0F];
        return s;
    };
    auto join = [&](const std::vector<uint8_t>& bytes) {
        std::string out;
        for (size_t i = 0; i < bytes.size(); ++i) {
            if (i) out += ' ';
            out += to_hex(bytes[i]);
        }
        return out;
    };

    if (type == "pattern") return value;  // already an AOB pattern
    if (type == "hex") return join(hex_bytes(value));
    if (type == "string") {
        std::vector<uint8_t> bytes(value.begin(), value.end());
        return join(bytes);
    }
    if (type == "byte" || type == "word" || type == "dword" || type == "qword") {
        // value was validated as an unsigned integer (dec or 0x-prefixed hex)
        unsigned long long v = std::strtoull(value.c_str(), nullptr, 0);
        size_t width = type == "byte" ? 1 : type == "word" ? 2
                     : type == "dword" ? 4 : 8;
        std::vector<uint8_t> bytes;
        for (size_t i = 0; i < width; ++i) {
            bytes.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
        }
        return join(bytes);
    }
    if (type == "float") {
        float f = std::strtof(value.c_str(), nullptr);
        uint32_t bits = 0;
        std::memcpy(&bits, &f, sizeof(bits));
        std::vector<uint8_t> bytes;
        for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<uint8_t>((bits >> (8 * i)) & 0xFF));
        return join(bytes);
    }
    // double
    double d = std::strtod(value.c_str(), nullptr);
    uint64_t bits = 0;
    std::memcpy(&bits, &d, sizeof(bits));
    std::vector<uint8_t> bytes;
    for (int i = 0; i < 8; ++i) bytes.push_back(static_cast<uint8_t>((bits >> (8 * i)) & 0xFF));
    return join(bytes);
}

int value_type_id(const std::string& s) {
    if (s == "byte") return 0;
    if (s == "word") return 1;
    if (s == "dword") return 2;
    if (s == "qword") return 3;
    if (s == "float") return 4;
    return 5;  // double
}

}  // namespace internal

namespace {

bool needs_session_attach(const CommandRequest& req) {
    if (!req.pid_set) return false;
    if (req.group == "ps") {
        // list/attach/detach manage the session themselves
        return req.action != "list" && req.action != "attach" && req.action != "detach";
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
    else {
        printer::print_error("unknown command group: '" + req.group + "'");
    }

    if (attached) deeptrace::detach();
    return rc;
}

}  // namespace deeptrace_cli

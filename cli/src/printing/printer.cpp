#include "printing/printer.h"

#include "deeptrace.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>

namespace {

const char* value_type_name(deeptrace::ValueType t) {
    switch (t) {
        case deeptrace::ValueType::Byte: return "byte";
        case deeptrace::ValueType::Word: return "word";
        case deeptrace::ValueType::Dword: return "dword";
        case deeptrace::ValueType::Qword: return "qword";
        case deeptrace::ValueType::Float: return "float";
        case deeptrace::ValueType::Double: return "double";
    }
    return "?";
}

}  // namespace

namespace deeptrace_cli {
namespace printer {

void print_error(const std::string& msg) {
    std::fprintf(stderr, "Error: %s\n", msg.c_str());
}

void print_message(const std::string& msg) {
    std::printf("%s\n", msg.c_str());
}

void print_help(const std::string& text) {
    std::printf("%s", text.c_str());
}

void print_version() {
    std::printf("deeptrace_cli v2.0.0\n");
}

std::string to_ascii(const std::wstring& s) {
    std::string out;
    for (wchar_t c : s) {
        if (c >= 0x20 && c < 0x7F) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('?');
        }
    }
    return out;
}

std::string format_address(uintptr_t a) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "0x%016llX", (unsigned long long)a);
    return buf;
}

void print_processes(const std::vector<deeptrace::ProcessInfo>& list) {
    std::printf("%-10s %-40s %-8s %s\n", "PID", "NAME", "THREADS", "PPID");
    for (const auto& p : list) {
        std::printf("%-10u %-40s %-8u %u\n", p.pid, to_ascii(p.name).c_str(),
                    p.thread_count, p.parent_pid);
    }
}

void print_process_info(const deeptrace::ProcessInfo& p) {
    std::printf("PID: %u\n", p.pid);
    std::printf("Name: %s\n", to_ascii(p.name).c_str());
    std::printf("Threads: %u\n", p.thread_count);
    std::printf("ParentPID: %u\n", p.parent_pid);
}

void print_regions(const std::vector<deeptrace::MemoryRegion>& list) {
    std::printf("%-18s %-14s %-10s %s\n", "BASE", "SIZE", "PROTECTION", "STATE");
    for (const auto& r : list) {
        std::printf("%-18s %-14s 0x%08X    %u\n",
                    format_address(r.base).c_str(),
                    std::to_string(r.size).c_str(), r.protection, r.state);
    }
}

void print_modules(const std::vector<deeptrace::ModuleInfo>& list) {
    std::printf("%-18s %-12s %s\n", "BASE", "SIZE", "NAME");
    for (const auto& m : list) print_module(m);
}

void print_module(const deeptrace::ModuleInfo& m) {
    std::printf("%-18s %-12s %s\n", format_address(m.base).c_str(),
                std::to_string(m.size).c_str(), to_ascii(m.name).c_str());
}

void print_exports(const std::vector<deeptrace::ExportInfo>& list) {
    std::printf("%-18s %s\n", "ADDRESS", "NAME");
    for (const auto& e : list) {
        std::printf("%-18s %s\n", format_address(e.address).c_str(), e.name.c_str());
    }
}

void print_threads(const std::vector<deeptrace::ThreadInfo>& list) {
    std::printf("%-10s %-10s %s\n", "TID", "PRIORITY", "START");
    for (const auto& t : list) {
        std::printf("%-10u %-10d %s\n", t.tid, t.priority,
                    format_address(t.start_address).c_str());
    }
}

void print_registers(const std::vector<deeptrace::RegisterInfo>& list) {
    std::printf("%-8s %s\n", "REG", "VALUE");
    for (const auto& r : list) {
        std::printf("%-8s %s\n", r.name.c_str(), format_address(r.value).c_str());
    }
}

void print_instructions(const std::vector<deeptrace::Instruction>& list) {
    std::printf("%-18s %-20s %s\n", "ADDRESS", "BYTES", "INSTRUCTION");
    for (const auto& i : list) {
        std::string bytes;
        for (size_t j = 0; j < i.bytes.size(); ++j) {
            char b[4];
            std::snprintf(b, sizeof b, "%02X", i.bytes[j]);
            bytes += b;
            if (j + 1 < i.bytes.size()) bytes += ' ';
        }
        std::printf("%-18s %-20s %s\n", format_address(i.address).c_str(),
                    bytes.c_str(), i.text.c_str());
    }
}

void print_watches(const std::vector<deeptrace::WatchEntry>& list) {
    std::printf("%-6s %-24s %-18s %-8s %-20s %s\n", "IDX", "DESCRIPTION", "ADDRESS",
                "TYPE", "VALUE", "VALID");
    for (const auto& w : list) {
        std::printf("%-6u %-24s %-18s %-8s %-20s %s\n", w.index,
                    w.description.c_str(), format_address(w.address).c_str(),
                    value_type_name(w.type), w.value.c_str(),
                    w.valid ? "yes" : "no");
    }
}

void print_hex_dump(uintptr_t base, const std::vector<uint8_t>& bytes) {
    const size_t per = 16;
    for (size_t off = 0; off < bytes.size(); off += per) {
        size_t n = bytes.size() - off;
        if (n > per) n = per;
        std::printf("%s  ", format_address(base + off).c_str());
        for (size_t i = 0; i < per; ++i) {
            if (i < n) {
                std::printf("%02X ", bytes[off + i]);
            } else {
                std::printf("   ");
            }
        }
        std::printf(" |");
        for (size_t i = 0; i < n; ++i) {
            uint8_t c = bytes[off + i];
            std::printf("%c", (c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : '.');
        }
        std::printf("|\n");
    }
}

void print_bytes(const std::vector<uint8_t>& bytes) {
    print_bytes_formatted(bytes, "hex");
}

void print_bytes_formatted(const std::vector<uint8_t>& bytes, const std::string& format) {
    if (format == "dec") {
        for (size_t i = 0; i < bytes.size(); ++i) {
            if (i) std::printf(" ");
            std::printf("%u", bytes[i]);
        }
        std::printf("\n");
        return;
    }
    if (format == "bin") {
        for (size_t i = 0; i < bytes.size(); ++i) {
            if (i) std::printf(" ");
            for (int b = 7; b >= 0; --b) std::printf("%d", (bytes[i] >> b) & 1);
        }
        std::printf("\n");
        return;
    }
    if (format == "ascii") {
        for (uint8_t c : bytes) {
            std::printf("%c", (c >= 0x20 && c < 0x7F) ? static_cast<char>(c) : '.');
        }
        std::printf("\n");
        return;
    }
    // hex
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i) std::printf(" ");
        std::printf("%02X", bytes[i]);
    }
    std::printf("\n");
}

void print_status(const deeptrace::DebugStatus& st) {
    std::printf("attached: %s\n", st.attached ? "yes" : "no");
    std::printf("pid: %u\n", st.pid);
    std::printf("breakpoints: %u\n", st.breakpoint_count);
    std::printf("hw_breakpoints: %u\n", st.hw_breakpoint_count);
}

void print_breakpoint(const deeptrace::BreakpointInfo& bp) {
    std::printf("breakpoint set at %s (orig 0x%02X)\n",
                format_address(bp.address).c_str(), bp.original_byte);
}

void print_injects(const std::vector<deeptrace::InjectInfo>& list) {
    std::printf("%-8s %-40s %-18s %-10s %s\n", "KIND", "PATH", "ADDRESS", "TID",
                "RUNNING");
    for (const auto& i : list) print_inject(i);
}

void print_inject(const deeptrace::InjectInfo& info) {
    std::printf("%-8s %-40s %-18s %-10u %s\n", info.kind.c_str(),
                to_ascii(info.path).c_str(), format_address(info.remote_base).c_str(),
                info.thread_id, info.running ? "yes" : "no");
}

}  // namespace printer
}  // namespace deeptrace_cli

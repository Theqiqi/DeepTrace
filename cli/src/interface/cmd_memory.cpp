#include "interface/cmd.h"

#include "interface/batch.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

namespace deeptrace_cli {

namespace {

deeptrace::ValueType to_value_type(const std::string& s) {
    return static_cast<deeptrace::ValueType>(internal::value_type_id(s));
}

// ---- v2.9.0 batch locators (mem batch read/write) ----

bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
        if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
        if (x != y) return false;
    }
    return true;
}

// Resolve a locator to its final address: root source (module / symbol /
// absolute base), then walk the pointer-chain offsets (each level reads a
// qword pointer and adds the offset).
deeptrace::Result resolve_locator(const batch::OffsetPath& it, uintptr_t& out) {
    uintptr_t addr = 0;
    if (!it.module.empty()) {
        uintptr_t base = 0;
        deeptrace::Result r = deeptrace::resolve_base(it.module, &base);
        if (r != deeptrace::Result::Ok) return r;
        addr = base + static_cast<uintptr_t>(it.base);
    } else if (!it.symbol.empty()) {
        deeptrace::Result r = deeptrace::script_symbol(it.symbol, &addr);
        if (r != deeptrace::Result::Ok) return r;
        addr += static_cast<uintptr_t>(it.base);
    } else {
        addr = static_cast<uintptr_t>(it.base);
    }
    for (uint64_t off : it.offsets) {
        uint64_t ptr = 0;
        size_t n = 0;
        deeptrace::Result r = deeptrace::memory_read(addr, &ptr, sizeof(ptr), &n);
        if (r != deeptrace::Result::Ok) return r;
        if (n != sizeof(ptr)) return deeptrace::Result::ReadFault;
        addr = static_cast<uintptr_t>(ptr) + static_cast<uintptr_t>(off);
    }
    out = addr;
    return deeptrace::Result::Ok;
}

// Read the final value per type. Ints/floats reuse memory_readval (same
// presentation as `watch`); string reads ASCII up to NUL (cap 256); bytes
// reads count bytes as a hex list.
deeptrace::Result read_typed_value(uintptr_t addr, const batch::OffsetPath& it,
                                   std::string& out_text) {
    using deeptrace::Result;
    if (it.type == "string") {
        uint8_t buf[256];
        size_t n = 0;
        Result r = deeptrace::memory_read(addr, buf, sizeof(buf), &n);
        if (r != Result::Ok) return r;
        if (n == 0) return Result::ReadFault;
        std::string s;
        for (size_t i = 0; i < n; ++i) {
            if (buf[i] == 0) break;
            char c = static_cast<char>(buf[i]);
            s.push_back((c >= 0x20 && c < 0x7F) ? c : '.');
        }
        out_text = s;
        return Result::Ok;
    }
    if (it.type == "bytes") {
        std::vector<uint8_t> buf(it.count);
        size_t n = 0;
        Result r = deeptrace::memory_read(addr, buf.data(), buf.size(), &n);
        if (r != Result::Ok) return r;
        if (n == 0) return Result::ReadFault;
        buf.resize(n);
        std::ostringstream os;
        for (size_t i = 0; i < buf.size(); ++i) {
            if (i) os << ' ';
            char b[4];
            std::snprintf(b, sizeof b, "%02X", buf[i]);
            os << b;
        }
        out_text = os.str();
        return Result::Ok;
    }
    return deeptrace::memory_readval(
        addr, static_cast<deeptrace::ValueType>(internal::value_type_id(it.type)),
        out_text);
}

// bytes write value: hex bytes, space-separated tokens allowed, optional 0x.
std::vector<uint8_t> bytes_value(const std::string& value) {
    std::string compact;
    for (char c : value) {
        if (c != ' ' && c != '\t') compact.push_back(c);
    }
    return internal::hex_bytes(compact);
}

int cmd_mem_batch(const CommandRequest& req) {
    using deeptrace::Result;
    const bool write_mode = (req.args[0] == "write");

    batch::File file;
    std::string err;
    if (!batch::parse_file(req.args[1], write_mode, file, err)) {
        printer::print_error(err);
        return 2;
    }

    // Optional process-name validation (case-insensitive, exe basename).
    if (!file.process.empty()) {
        deeptrace::ProcessInfo pi;
        Result r = deeptrace::process_info(req.pid, pi);
        if (r != Result::Ok) return internal::report_error(r, "process check");
        std::string actual = printer::to_ascii(pi.name);
        size_t slash = actual.find_last_of("/\\");
        if (slash != std::string::npos) actual = actual.substr(slash + 1);
        if (!iequals(actual, file.process)) {
            printer::print_error("process mismatch: config expects '" +
                                 file.process + "', attached process is '" +
                                 actual + "'");
            return 1;
        }
    }

    // Per-item: resolve -> read value (read mode) or write value (write
    // mode). A failing item reports its error and the rest continue; the
    // final exit code reflects whether any item failed (design R8).
    std::vector<BatchRow> rows;
    bool any_failed = false;
    for (const auto& it : file.items) {
        uintptr_t addr = 0;
        Result r = resolve_locator(it, addr);
        if (r != Result::Ok) {
            any_failed = true;
            internal::report_error(r, it.name);
            if (!write_mode) rows.push_back({it.name, 0, "error"});
            continue;
        }
        if (write_mode) {
            std::vector<uint8_t> data;
            if (it.type == "bytes") {
                data = bytes_value(it.value);
            } else if (!internal::typed_bytes(it.value, it.type, data)) {
                any_failed = true;
                internal::report_error(Result::InvalidArg, it.name + " value");
                continue;
            }
            size_t n = 0;
            r = deeptrace::memory_write(addr, data.data(), data.size(), &n);
            if (r != Result::Ok || n != data.size()) {
                any_failed = true;
                internal::report_error(r != Result::Ok ? r : Result::WriteFault,
                                       it.name);
                continue;
            }
            printer::print_message("OK " + it.name);
            continue;
        }
        std::string text;
        r = read_typed_value(addr, it, text);
        if (r != Result::Ok) {
            any_failed = true;
            internal::report_error(r, it.name);
            rows.push_back({it.name, 0, "error"});
            continue;
        }
        rows.push_back({it.name, addr, text});
    }
    if (!write_mode) printer::print_batch_read(rows);
    return any_failed ? 1 : 0;
}

}  // namespace

int cmd_mem(const CommandRequest& req) {
    using deeptrace::Result;
    if (req.action == "batch") return cmd_mem_batch(req);
    if (req.action == "read") {
        uintptr_t addr = 0;
        Result r = internal::resolve_addr(req.args[0], addr);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        size_t size = static_cast<size_t>(internal::to_u64(req.args[1]));
        std::string format = req.args[2];
        if (size == 0) return internal::report_error(Result::InvalidArg, req.args[1]);
        std::vector<uint8_t> buf(size);
        size_t n = 0;
        r = deeptrace::memory_read(addr, buf.data(), size, &n);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        buf.resize(n);
        printer::print_bytes_formatted(buf, format);
        return 0;
    }
    if (req.action == "write") {
        uintptr_t addr = 0;
        Result r = internal::resolve_addr(req.args[0], addr);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        std::string format = req.args[2];
        std::vector<uint8_t> data;
        if (format == "dec") {
            uint64_t v = internal::to_u64(req.args[1]);
            for (int i = 0; i < 8; ++i) data.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
        } else {
            data = internal::hex_bytes(req.args[1]);
        }
        if (data.empty()) return internal::report_error(Result::InvalidArg, req.args[1]);
        size_t n = 0;
        r = deeptrace::memory_write(addr, data.data(), data.size(), &n);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "dump") {
        uintptr_t addr = 0;
        Result r = internal::resolve_addr(req.args[0], addr);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        size_t size = static_cast<size_t>(internal::to_u64(req.args[1]));
        std::vector<uint8_t> out;
        r = deeptrace::memory_dump(addr, size, out);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_hex_dump(addr, out);
        return 0;
    }
    if (req.action == "regions") {
        std::vector<deeptrace::MemoryRegion> regions;
        Result r = deeptrace::memory_regions(regions);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_regions(regions);
        return 0;
    }
    if (req.action == "readval") {
        uintptr_t addr = 0;
        Result r = internal::resolve_addr(req.args[0], addr);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        std::string text;
        r = deeptrace::memory_readval(addr, to_value_type(req.args[1]), text);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_message(text);
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace deeptrace_cli

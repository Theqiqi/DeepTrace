#include "interface/cmd.h"

#include "printing/printer.h"

#include "pmem.h"

#include <cstring>
#include <string>
#include <vector>

namespace pmem_cli {

namespace {

pmem::ValueType to_value_type(const std::string& s) {
    return static_cast<pmem::ValueType>(internal::value_type_id(s));
}

}  // namespace

int cmd_mem(const CommandRequest& req) {
    using pmem::Result;
    if (req.action == "read") {
        uintptr_t addr = internal::to_addr(req.args[0]);
        size_t size = static_cast<size_t>(internal::to_u64(req.args[1]));
        std::string format = req.args[2];
        if (size == 0) return internal::report_error(Result::InvalidArg, req.args[1]);
        std::vector<uint8_t> buf(size);
        size_t n = 0;
        Result r = pmem::memory_read(addr, buf.data(), size, &n);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        buf.resize(n);
        printer::print_bytes_formatted(buf, format);
        return 0;
    }
    if (req.action == "write") {
        uintptr_t addr = internal::to_addr(req.args[0]);
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
        Result r = pmem::memory_write(addr, data.data(), data.size(), &n);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "dump") {
        uintptr_t addr = internal::to_addr(req.args[0]);
        size_t size = static_cast<size_t>(internal::to_u64(req.args[1]));
        std::vector<uint8_t> out;
        Result r = pmem::memory_dump(addr, size, out);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_hex_dump(addr, out);
        return 0;
    }
    if (req.action == "regions") {
        std::vector<pmem::MemoryRegion> regions;
        Result r = pmem::memory_regions(regions);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_regions(regions);
        return 0;
    }
    if (req.action == "readval") {
        uintptr_t addr = internal::to_addr(req.args[0]);
        std::string text;
        Result r = pmem::memory_readval(addr, to_value_type(req.args[1]), text);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_message(text);
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace pmem_cli

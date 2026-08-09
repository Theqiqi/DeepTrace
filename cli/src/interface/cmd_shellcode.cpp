#include "interface/cmd.h"

#include "printing/printer.h"

#include "pmem.h"

#include <string>
#include <vector>

namespace pmem_cli {

int cmd_shellcode(const CommandRequest& req) {
    using pmem::Result;
    if (req.action == "inject") {
        std::vector<uint8_t> bytes = internal::hex_bytes(req.args[0]);
        if (bytes.empty()) return internal::report_error(Result::InvalidArg, req.args[0]);
        pmem::InjectInfo info;
        Result r = pmem::shellcode_inject(bytes, info);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_inject(info);
        return 0;
    }
    if (req.action == "injectat") {
        uintptr_t addr = internal::to_addr(req.args[0]);
        std::vector<uint8_t> bytes = internal::hex_bytes(req.args[1]);
        if (bytes.empty()) return internal::report_error(Result::InvalidArg, req.args[1]);
        pmem::InjectInfo info;
        Result r = pmem::shellcode_inject_at(addr, bytes, info);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_inject(info);
        return 0;
    }
    if (req.action == "status") {
        std::vector<pmem::InjectInfo> list;
        Result r = pmem::shellcode_status(list);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_injects(list);
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace pmem_cli

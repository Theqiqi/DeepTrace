#include "interface/cmd.h"

#include "printing/printer.h"

#include "pmem.h"

#include <string>
#include <vector>

namespace pmem_cli {

int cmd_disasm(const CommandRequest& req) {
    using pmem::Result;
    if (req.action == "at") {
        uintptr_t addr = internal::to_addr(req.args[0]);
        uint32_t count = static_cast<uint32_t>(internal::to_u64(req.args[1]));
        std::vector<pmem::Instruction> insns;
        Result r = pmem::disasm_at(addr, count, insns);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_instructions(insns);
        return 0;
    }
    if (req.action == "range") {
        uintptr_t start = internal::to_addr(req.args[0]);
        uintptr_t end = internal::to_addr(req.args[1]);
        std::vector<pmem::Instruction> insns;
        Result r = pmem::disasm_range(start, end, insns);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_instructions(insns);
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace pmem_cli

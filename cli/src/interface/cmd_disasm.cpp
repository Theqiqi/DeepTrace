#include "interface/cmd.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <string>
#include <vector>

namespace deeptrace_cli {

int cmd_disasm(const CommandRequest& req) {
    using deeptrace::Result;
    if (req.action == "at") {
        uintptr_t addr = internal::to_addr(req.args[0]);
        uint32_t count = static_cast<uint32_t>(internal::to_u64(req.args[1]));
        std::vector<deeptrace::Instruction> insns;
        Result r = deeptrace::disasm_at(addr, count, insns);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_instructions(insns);
        return 0;
    }
    if (req.action == "range") {
        uintptr_t start = internal::to_addr(req.args[0]);
        uintptr_t end = internal::to_addr(req.args[1]);
        std::vector<deeptrace::Instruction> insns;
        Result r = deeptrace::disasm_range(start, end, insns);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_instructions(insns);
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace deeptrace_cli

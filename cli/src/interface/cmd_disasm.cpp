#include "interface/cmd.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <string>
#include <vector>

namespace deeptrace_cli {

int cmd_disasm(const CommandRequest& req) {
    using deeptrace::Result;
    if (req.action == "at") {
        uintptr_t addr = 0;
        Result r = internal::resolve_addr(req.args[0], addr);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        uint32_t count = static_cast<uint32_t>(internal::to_u64(req.args[1]));
        std::vector<deeptrace::Instruction> insns;
        r = deeptrace::disasm_at(addr, count, insns);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_instructions(insns);
        return 0;
    }
    if (req.action == "range") {
        uintptr_t start = 0;
        Result r = internal::resolve_addr(req.args[0], start);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        uintptr_t end = 0;
        r = internal::resolve_addr(req.args[1], end);
        if (r != Result::Ok) return internal::report_error(r, req.args[1]);
        std::vector<deeptrace::Instruction> insns;
        r = deeptrace::disasm_range(start, end, insns);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_instructions(insns);
        return 0;
    }
    if (req.action == "file") {
        // v2.13.0: disassemble a local binary file (no session needed).
        std::vector<uint8_t> bytes;
        if (!internal::read_binary_file(req.args[0], bytes)) {
            printer::print_error("cannot read file: " + req.args[0]);
            return 2;
        }
        uint32_t count = static_cast<uint32_t>(internal::to_u64(req.args[1]));
        std::vector<deeptrace::Instruction> insns;
        Result r = deeptrace::disasm_buffer(bytes.data(), bytes.size(), 0,
                                            count, insns);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_instructions(insns);
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace deeptrace_cli

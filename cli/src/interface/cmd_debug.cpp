#include "interface/cmd.h"

#include "printing/printer.h"

#include "pmem.h"

#include <string>
#include <vector>

namespace pmem_cli {

namespace {

uint32_t tid_arg(const CommandRequest& req) {
    return req.args.empty() ? 0 : internal::to_u32(req.args[0]);
}

}  // namespace

int cmd_debug(const CommandRequest& req) {
    using pmem::Result;
    if (req.action == "attach") {
        Result r = pmem::debug_attach();
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "detach") {
        Result r = pmem::debug_detach();
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "pause") {
        Result r = pmem::debug_pause();
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "resume") {
        Result r = pmem::debug_resume();
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "step" || req.action == "next") {
        uint32_t tid = tid_arg(req);
        uintptr_t rip = 0;
        Result r = (req.action == "step") ? pmem::debug_step(tid, &rip)
                                          : pmem::debug_step_over(tid, &rip);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_message("rip = " + printer::format_address(rip));
        return 0;
    }
    if (req.action == "break") {
        uintptr_t addr = internal::to_addr(req.args[0]);
        pmem::BreakpointInfo bp;
        Result r = pmem::breakpoint_set(addr, bp);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_breakpoint(bp);
        return 0;
    }
    if (req.action == "clear") {
        uintptr_t addr = internal::to_addr(req.args[0]);
        Result r = pmem::breakpoint_clear(addr);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "hbreak") {
        uintptr_t addr = internal::to_addr(req.args[0]);
        uint32_t type = internal::to_u32(req.args[1]);
        uint32_t len = internal::to_u32(req.args[2]);
        Result r = pmem::hw_breakpoint_set(addr, type, len);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "hclear") {
        uintptr_t addr = internal::to_addr(req.args[0]);
        Result r = pmem::hw_breakpoint_clear(addr);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "guard" || req.action == "unguard") {
        uintptr_t addr = internal::to_addr(req.args[0]);
        size_t size = static_cast<size_t>(internal::to_u64(req.args[1]));
        Result r = (req.action == "guard") ? pmem::guard_set(addr, size)
                                           : pmem::guard_clear(addr, size);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "status") {
        pmem::DebugStatus st;
        Result r = pmem::debug_status(st);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_status(st);
        return 0;
    }
    if (req.action == "registers") {
        uint32_t tid = tid_arg(req);
        std::vector<pmem::RegisterInfo> regs;
        Result r = pmem::registers_get(regs, tid);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_registers(regs);
        return 0;
    }
    if (req.action == "register") {
        std::string name = req.args[0];
        uint32_t tid = req.args.size() > 1 ? internal::to_u32(req.args[1]) : 0;
        uint64_t value = 0;
        Result r = pmem::register_get(name, &value, tid);
        if (r != Result::Ok) return internal::report_error(r, name);
        printer::print_message(name + " = " + printer::format_address(value));
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace pmem_cli

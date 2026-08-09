#include "interface/cmd.h"

#include "printing/printer.h"

#include "pmem.h"

#include <string>
#include <vector>

namespace pmem_cli {

int cmd_ps(const CommandRequest& req) {
    using pmem::Result;
    if (req.action == "list") {
        std::vector<pmem::ProcessInfo> procs;
        Result r = pmem::enumerate_processes(procs);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_processes(procs);
        return 0;
    }
    if (req.action == "attach") {
        uint32_t pid = internal::to_u32(req.args[0]);
        Result r = pmem::attach(pid);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "detach") {
        Result r = pmem::detach();
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "info") {
        uint32_t pid = 0;
        if (pmem::session_pid(&pid) != Result::Ok || pid == 0)
            return internal::report_error(Result::NotAttached, "");
        pmem::ProcessInfo info;
        Result r = pmem::process_info(pid, info);
        if (r != Result::Ok) return internal::report_error(r, std::to_string(pid));
        printer::print_process_info(info);
        return 0;
    }
    if (req.action == "suspend" || req.action == "resume" || req.action == "kill") {
        uint32_t pid = 0;
        if (pmem::session_pid(&pid) != Result::Ok || pid == 0)
            return internal::report_error(Result::NotAttached, "");
        Result r = Result::Ok;
        if (req.action == "suspend") {
            r = pmem::suspend_process(pid);
        } else if (req.action == "resume") {
            r = pmem::resume_process(pid);
        } else {
            uint32_t code = internal::to_u32(req.args[0]);
            r = pmem::terminate_process(pid, code);
        }
        if (r != Result::Ok) return internal::report_error(r, std::to_string(pid));
        printer::print_message("OK");
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace pmem_cli

#include "interface/cmd.h"

#include "printing/printer.h"

#include "pmem.h"

#include <string>
#include <vector>

namespace pmem_cli {

int cmd_thread(const CommandRequest& req) {
    using pmem::Result;
    if (req.action == "list") {
        std::vector<pmem::ThreadInfo> threads;
        Result r = pmem::thread_list(threads);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_threads(threads);
        return 0;
    }
    if (req.action == "suspend" || req.action == "resume" || req.action == "kill") {
        uint32_t tid = internal::to_u32(req.args[0]);
        Result r = Result::Ok;
        if (req.action == "suspend") r = pmem::thread_suspend(tid);
        else if (req.action == "resume") r = pmem::thread_resume(tid);
        else r = pmem::thread_terminate(tid, 0);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_message("OK");
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace pmem_cli

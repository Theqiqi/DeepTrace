#include "interface/cmd.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <string>
#include <vector>

namespace deeptrace_cli {

int cmd_thread(const CommandRequest& req) {
    using deeptrace::Result;
    if (req.action == "list") {
        std::vector<deeptrace::ThreadInfo> threads;
        Result r = deeptrace::thread_list(threads);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_threads(threads);
        return 0;
    }
    if (req.action == "suspend" || req.action == "resume" || req.action == "kill") {
        uint32_t tid = internal::to_u32(req.args[0]);
        Result r = Result::Ok;
        if (req.action == "suspend") r = deeptrace::thread_suspend(tid);
        else if (req.action == "resume") r = deeptrace::thread_resume(tid);
        else r = deeptrace::thread_terminate(tid, 0);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_message("OK");
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace deeptrace_cli

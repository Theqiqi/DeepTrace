#include "interface/cmd.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <string>
#include <vector>

namespace deeptrace_cli {

int cmd_dll(const CommandRequest& req) {
    using deeptrace::Result;
    if (req.action == "inject") {
        deeptrace::InjectInfo info;
        Result r = deeptrace::dll_inject(req.args[0], info);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_inject(info);
        return 0;
    }
    if (req.action == "eject") {
        Result r = deeptrace::dll_eject(req.args[0]);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "list" || req.action == "status") {
        std::vector<deeptrace::InjectInfo> list;
        Result r = (req.action == "list") ? deeptrace::dll_list(list) : deeptrace::dll_status(list);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_injects(list);
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace deeptrace_cli

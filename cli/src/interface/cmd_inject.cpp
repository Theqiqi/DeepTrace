#include "interface/cmd.h"

#include "printing/printer.h"

#include "pmem.h"

#include <string>
#include <vector>

namespace pmem_cli {

int cmd_dll(const CommandRequest& req) {
    using pmem::Result;
    if (req.action == "inject") {
        pmem::InjectInfo info;
        Result r = pmem::dll_inject(req.args[0], info);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_inject(info);
        return 0;
    }
    if (req.action == "eject") {
        Result r = pmem::dll_eject(req.args[0]);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "list" || req.action == "status") {
        std::vector<pmem::InjectInfo> list;
        Result r = (req.action == "list") ? pmem::dll_list(list) : pmem::dll_status(list);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_injects(list);
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace pmem_cli

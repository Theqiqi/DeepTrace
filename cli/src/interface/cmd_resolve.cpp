#include "interface/cmd.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <string>
#include <vector>

namespace deeptrace_cli {

int cmd_resolve(const CommandRequest& req) {
    using deeptrace::Result;
    if (req.action == "base") {
        uintptr_t base = 0;
        Result r = deeptrace::resolve_base(req.args[0], &base);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_message(printer::format_address(base));
        return 0;
    }
    if (req.action == "scan") {
        std::vector<uintptr_t> hits;
        Result r = deeptrace::pattern_scan(req.args[0], hits);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        if (hits.empty()) {
            printer::print_message("No match found");
            return 0;
        }
        for (uintptr_t h : hits) {
            printer::print_message(printer::format_address(h));
        }
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace deeptrace_cli

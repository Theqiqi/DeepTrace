#include "interface/cmd.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <string>
#include <vector>

namespace deeptrace_cli {

int cmd_module(const CommandRequest& req) {
    using deeptrace::Result;
    if (req.action == "list") {
        std::vector<deeptrace::ModuleInfo> mods;
        Result r = deeptrace::module_list(mods);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_modules(mods);
        return 0;
    }
    if (req.action == "find") {
        deeptrace::ModuleInfo info;
        Result r = deeptrace::module_find(req.args[0], info);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_module(info);
        return 0;
    }
    if (req.action == "base") {
        uintptr_t base = 0;
        Result r = deeptrace::module_base(req.args[0], &base);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_message(printer::format_address(base));
        return 0;
    }
    if (req.action == "exports") {
        std::vector<deeptrace::ExportInfo> exps;
        Result r = deeptrace::module_exports(req.args[0], exps);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_exports(exps);
        return 0;
    }
    if (req.action == "dump") {
        std::string file = req.args[1];
        std::string hex;
        Result r = deeptrace::module_dump(req.args[0], file, file.empty() ? &hex : nullptr);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        if (file.empty()) {
            printer::print_message(hex);
        } else {
            printer::print_message("OK");
        }
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace deeptrace_cli

#include "interface/cmd.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <string>
#include <vector>

namespace deeptrace_cli {

int cmd_watch(const CommandRequest& req) {
    using deeptrace::Result;
    if (req.action == "list") {
        std::vector<deeptrace::WatchEntry> entries;
        Result r = deeptrace::watch_list(entries);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_watches(entries);
        return 0;
    }
    if (req.action == "add") {
        std::string desc = req.args[0];
        uintptr_t addr = 0;
        Result r = internal::resolve_addr(req.args[1], addr);
        if (r != Result::Ok) return internal::report_error(r, req.args[1]);
        deeptrace::ValueType type = static_cast<deeptrace::ValueType>(internal::value_type_id(req.args[2]));
        r = deeptrace::watch_add(desc, addr, type);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "remove") {
        uint32_t index = internal::to_u32(req.args[0]);
        Result r = deeptrace::watch_remove(index);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "refresh") {
        std::vector<deeptrace::WatchEntry> entries;
        Result r = deeptrace::watch_refresh(entries);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_watches(entries);
        return 0;
    }
    if (req.action == "clear") {
        Result r = deeptrace::watch_clear();
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_message("OK");
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace deeptrace_cli

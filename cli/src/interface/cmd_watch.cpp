#include "interface/cmd.h"

#include "printing/printer.h"

#include "pmem.h"

#include <string>
#include <vector>

namespace pmem_cli {

int cmd_watch(const CommandRequest& req) {
    using pmem::Result;
    if (req.action == "list") {
        std::vector<pmem::WatchEntry> entries;
        Result r = pmem::watch_list(entries);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_watches(entries);
        return 0;
    }
    if (req.action == "add") {
        std::string desc = req.args[0];
        uintptr_t addr = internal::to_addr(req.args[1]);
        pmem::ValueType type = static_cast<pmem::ValueType>(internal::value_type_id(req.args[2]));
        Result r = pmem::watch_add(desc, addr, type);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "remove") {
        uint32_t index = internal::to_u32(req.args[0]);
        Result r = pmem::watch_remove(index);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "refresh") {
        std::vector<pmem::WatchEntry> entries;
        Result r = pmem::watch_refresh(entries);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_watches(entries);
        return 0;
    }
    if (req.action == "clear") {
        Result r = pmem::watch_clear();
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_message("OK");
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace pmem_cli

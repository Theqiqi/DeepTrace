#include "interface/cmd.h"
#include "interface/ptrscan.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace_cli {

namespace {

// Render one chain as "ModuleName+ROOT +off1 +off2..." (or raw 0x root when
// the root address is not inside any module).
std::string chain_line(const deeptrace::PointerChain& c,
                       const std::vector<deeptrace::ModuleInfo>& mods) {
    return printer::format_pointer_chain(c.root, c.offsets, mods);
}

}  // namespace

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
    if (req.action == "ptrscan") {
        // JSON config: {version, target, module, max_offset, max_level,
        // max_results, threads, rescan}. Parse errors -> usage error (2);
        // runtime errors (not attached / module not loaded) -> 1.
        ptrscan::Config cfg;
        std::string err;
        if (!ptrscan::parse_file(req.args[0], cfg, err)) {
            printer::print_error(err);  // usage error -> stderr (like mem batch)
            return 2;
        }

        deeptrace::PointerScanConfig pc;
        pc.target = cfg.target;
        pc.max_offset = cfg.max_offset;
        pc.max_level = cfg.max_level;
        pc.max_results = cfg.max_results;
        pc.module = cfg.module;
        pc.thread_count = cfg.threads;

        std::vector<deeptrace::PointerChain> chains;
        Result r = deeptrace::pointer_map_snapshot(pc, chains);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        const size_t before = chains.size();

        if (cfg.has_rescan) {
            std::vector<deeptrace::PointerChain> filtered;
            r = deeptrace::pointer_map_rescan(chains, cfg.rescan_target, pc,
                                              filtered);
            if (r != Result::Ok) return internal::report_error(r, req.args[0]);
            chains = std::move(filtered);
        }

        if (chains.empty()) {
            printer::print_message("No chains found");
            return 0;
        }

        // Resolve module containment once for root rendering.
        std::vector<deeptrace::ModuleInfo> mods;
        deeptrace::module_list(mods);  // failure -> raw-address rendering
        for (const auto& c : chains) {
            printer::print_message(chain_line(c, mods));
        }
        if (cfg.has_rescan) {
            printer::print_message("(" + std::to_string(before) + " chains, " +
                                   std::to_string(chains.size()) +
                                   " after rescan)");
        } else {
            printer::print_message("(" + std::to_string(chains.size()) +
                                   " chains)");
        }
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace deeptrace_cli

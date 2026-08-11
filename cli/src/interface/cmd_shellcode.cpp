#include "interface/cmd.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <string>
#include <vector>

namespace deeptrace_cli {

namespace {

// Resolve source into bytes; on failure print a usage-style error and return
// the exit code (2). asm_ok allows .asm/.s files (exec only).
int resolve_or_error(const std::string& source, bool asm_ok, std::vector<uint8_t>& out) {
    deeptrace::Result r = internal::resolve_source(source, asm_ok, out);
    if (r == deeptrace::Result::Ok && !out.empty()) return 0;
    printer::print_error("invalid shellcode source: '" + source + "'");
    return 2;
}

// alloc bytes in the target and trigger once at the resulting address;
// on failure after allocation, roll back via shellcode_free (no residue).
int alloc_and_run(const std::vector<uint8_t>& bytes) {
    using deeptrace::Result;
    deeptrace::InjectInfo info;
    Result r = deeptrace::shellcode_alloc(bytes, info);
    if (r != Result::Ok) return internal::report_error(r, "");
    deeptrace::InjectInfo run_info;
    r = deeptrace::shellcode_run(info.remote_base, run_info);
    if (r != Result::Ok) {
        deeptrace::shellcode_free(info.remote_base);  // rollback
        return internal::report_error(r, printer::format_address(info.remote_base));
    }
    printer::print_inject(run_info);
    return 0;
}

}  // namespace

int cmd_shellcode(const CommandRequest& req) {
    using deeptrace::Result;
    if (req.action == "inject") {
        std::vector<uint8_t> bytes = internal::hex_bytes(req.args[0]);
        if (bytes.empty()) return internal::report_error(Result::InvalidArg, req.args[0]);
        deeptrace::InjectInfo info;
        Result r = deeptrace::shellcode_inject(bytes, info);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_inject(info);
        return 0;
    }
    if (req.action == "injectat") {
        uintptr_t addr = internal::to_addr(req.args[0]);
        std::vector<uint8_t> bytes = internal::hex_bytes(req.args[1]);
        if (bytes.empty()) return internal::report_error(Result::InvalidArg, req.args[1]);
        deeptrace::InjectInfo info;
        Result r = deeptrace::shellcode_inject_at(addr, bytes, info);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_inject(info);
        return 0;
    }
    if (req.action == "injectfile") {
        std::vector<uint8_t> bytes;
        if (!internal::read_binary_file(req.args[0], bytes)) {
            printer::print_error("cannot read file: " + req.args[0]);
            return 2;
        }
        if (bytes.empty()) {
            printer::print_error("invalid shellcode source: '" + req.args[0] + "'");
            return 2;
        }
        deeptrace::InjectInfo info;
        Result r = deeptrace::shellcode_inject(bytes, info);
        if (r != Result::Ok) return internal::report_error(r, req.args[0]);
        printer::print_inject(info);
        return 0;
    }
    if (req.action == "alloc") {
        std::vector<uint8_t> bytes;
        int rc = resolve_or_error(req.args[0], false, bytes);
        if (rc != 0) return rc;
        deeptrace::InjectInfo info;
        Result r = deeptrace::shellcode_alloc(bytes, info);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_inject(info);
        return 0;
    }
    if (req.action == "run") {
        uintptr_t addr = internal::to_addr(req.args[0]);
        deeptrace::InjectInfo info;
        Result r = deeptrace::shellcode_run(addr, info);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_inject(info);
        return 0;
    }
    if (req.action == "free") {
        uintptr_t addr = internal::to_addr(req.args[0]);
        Result r = deeptrace::shellcode_free(addr);
        if (r != Result::Ok) return internal::report_error(r, printer::format_address(addr));
        printer::print_message("OK");
        return 0;
    }
    if (req.action == "exec") {
        std::vector<uint8_t> bytes;
        int rc = resolve_or_error(req.args[0], true, bytes);
        if (rc != 0) return rc;
        return alloc_and_run(bytes);
    }
    if (req.action == "status") {
        std::vector<deeptrace::InjectInfo> list;
        Result r = deeptrace::shellcode_status(list);
        if (r != Result::Ok) return internal::report_error(r, "");
        printer::print_injects(list);
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace deeptrace_cli

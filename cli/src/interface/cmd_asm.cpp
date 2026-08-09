#include "interface/cmd.h"

#include "printing/printer.h"

#include "pmem.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace pmem_cli {

namespace {

// Flags are order-independent: scan the raw arg list for --hex/--c-array.
bool has_flag(const CommandRequest& req, const char* name) {
    return std::find(req.args.begin(), req.args.end(), name) != req.args.end();
}

}  // namespace

int cmd_asm(const CommandRequest& req) {
    using pmem::Result;
    if (req.action == "assemble") {
        std::string code = req.args[0];
        std::vector<uint8_t> bytes;
        std::string text;
        Result r = pmem::asm_assemble(code, bytes, &text);
        if (r != Result::Ok) return internal::report_error(r, code);

        bool c_array = has_flag(req, "--c-array");
        bool hex_flag = has_flag(req, "--hex");
        if (c_array) {
            std::string out;
            for (size_t i = 0; i < bytes.size(); ++i) {
                char b[16];
                std::snprintf(b, sizeof b, "0x%02X, ", bytes[i]);
                out += b;
            }
            if (!out.empty()) out.resize(out.size() - 2);
            printer::print_message("unsigned char code[] = { " + out + " };");
            return 0;
        }
        if (hex_flag) {
            printer::print_message(text);
            return 0;
        }
        printer::print_message(text);
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace pmem_cli

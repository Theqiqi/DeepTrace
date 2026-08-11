#include "interface/cmd.h"

#include "printing/printer.h"

#include "deeptrace.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace deeptrace_cli {

namespace {

// Flags are order-independent: scan the raw arg list for --hex/--c-array.
bool has_flag(const CommandRequest& req, const char* name) {
    return std::find(req.args.begin(), req.args.end(), name) != req.args.end();
}

}  // namespace

namespace {

// Print assembled bytes as hex text or a C array (same output as assemble).
void print_bytes_as(const std::vector<uint8_t>& bytes, bool c_array) {
    if (c_array) {
        std::string out;
        for (size_t i = 0; i < bytes.size(); ++i) {
            char b[16];
            std::snprintf(b, sizeof b, "0x%02X, ", bytes[i]);
            out += b;
        }
        if (!out.empty()) out.resize(out.size() - 2);
        printer::print_message("unsigned char code[] = { " + out + " };");
        return;
    }
    std::string out;
    for (size_t i = 0; i < bytes.size(); ++i) {
        char b[8];
        std::snprintf(b, sizeof b, "%02X", bytes[i]);
        out += b;
    }
    printer::print_message(out);
}

}  // namespace

int cmd_asm(const CommandRequest& req) {
    using deeptrace::Result;
    if (req.action == "assemble") {
        std::string code = req.args[0];
        std::vector<uint8_t> bytes;
        std::string text;
        Result r = deeptrace::asm_assemble(code, bytes, &text);
        if (r != Result::Ok) return internal::report_error(r, code);

        bool c_array = has_flag(req, "--c-array");
        print_bytes_as(bytes, c_array);
        return 0;
    }
    if (req.action == "file") {
        std::string path = req.args[0];
        std::string code;
        if (!internal::read_text_file(path, code)) {
            printer::print_error("cannot read file: " + path);
            return 2;
        }
        std::vector<uint8_t> bytes;
        std::string text;
        Result r = deeptrace::asm_assemble(code, bytes, &text);
        if (r != Result::Ok) return internal::report_error(r, path);

        // optional --out <path.bin>: write raw bytes to file
        std::string out_path;
        for (size_t i = 0; i + 1 < req.args.size(); ++i) {
            if (req.args[i] == "--out" && !req.args[i + 1].empty()) {
                out_path = req.args[i + 1];
                break;
            }
        }
        if (!out_path.empty() && !internal::write_binary_file(out_path, bytes)) {
            printer::print_error("cannot write file: " + out_path);
            return 1;
        }
        if (!out_path.empty()) {
            char buf[64];
            std::snprintf(buf, sizeof buf, "wrote %s (%zu bytes)", out_path.c_str(),
                          bytes.size());
            printer::print_message(buf);
        }

        bool c_array = has_flag(req, "--c-array");
        print_bytes_as(bytes, c_array);
        return 0;
    }
    return internal::report_error(Result::InvalidArg, req.action);
}

}  // namespace deeptrace_cli

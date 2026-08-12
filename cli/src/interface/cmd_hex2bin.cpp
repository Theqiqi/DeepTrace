#include "interface/cmd.h"

#include "printing/printer.h"

#include <cstdio>
#include <string>
#include <vector>

namespace deeptrace_cli {

int cmd_hex2bin(const CommandRequest& req) {
    // parser validated: args[0] = hex-bytes, args[1] = output path (non-empty).
    std::vector<uint8_t> bytes = internal::hex_bytes(req.args[0]);
    if (bytes.empty()) {
        printer::print_error("invalid hex-bytes: '" + req.args[0] + "'");
        return 2;
    }
    if (!internal::write_binary_file(req.args[1], bytes)) {
        printer::print_error("cannot write file: " + req.args[1]);
        return 1;
    }
    char buf[64];
    std::snprintf(buf, sizeof buf, "wrote %s (%zu bytes)", req.args[1].c_str(),
                  bytes.size());
    printer::print_message(buf);
    return 0;
}

// v2.13.0: hex2bin inverse - print a raw binary file as hex bytes (or other
// formats). Output feeds back into `shellcode inject` / `hex2bin`.
int cmd_bin2hex(const CommandRequest& req) {
    std::vector<uint8_t> bytes;
    if (!internal::read_binary_file(req.args[0], bytes)) {
        printer::print_error("cannot read file: " + req.args[0]);
        return 2;
    }
    if (bytes.empty()) {
        printer::print_error("empty file: " + req.args[0]);
        return 2;
    }
    std::string format = req.args[1].empty() ? "hex" : req.args[1];
    printer::print_bytes_formatted(bytes, format);
    return 0;
}

}  // namespace deeptrace_cli

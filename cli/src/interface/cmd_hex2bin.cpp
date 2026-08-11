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

}  // namespace deeptrace_cli

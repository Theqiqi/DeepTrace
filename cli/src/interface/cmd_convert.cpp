#include "interface/cmd.h"

#include "printing/printer.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace_cli {

int cmd_convert(const CommandRequest& req) {
    // req.args[0] = type, req.args[1] = value (both validated by the parser).
    std::vector<uint8_t> bytes;
    if (!internal::typed_bytes(req.args[1], req.args[0], bytes)) {
        printer::print_error("invalid value for type '" + req.args[0] + "': '" +
                             req.args[1] + "'");
        return 2;
    }
    printer::print_bytes(bytes);
    return 0;
}

}  // namespace deeptrace_cli

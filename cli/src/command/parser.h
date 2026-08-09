#pragma once
// command layer: parser interface. Parses argv into a CommandRequest.
// Errors are returned (not printed) so the caller decides how to present them.

#include "command/request.h"

#include <string>

namespace deeptrace_cli {

struct ParseResult {
    CommandRequest req;
    bool ok = true;
    int exit_code = 0;    // 0 success; 1 missing command; 2 usage error
    std::string error;    // error message (without "Error: " prefix)
};

// Parse command line. On help/version the request flags are set (ok=true).
ParseResult parse_args(int argc, char* argv[]);

}  // namespace deeptrace_cli

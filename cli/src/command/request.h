#pragma once
// command layer: parsed command request.
// Only standard C++ types; no deeptrace / platform types.

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace_cli {

struct CommandRequest {
    std::string group;               // ps / mem / module / ...
    std::string action;              // list / read / ...
    std::vector<std::string> args;   // validated positional args
    uint32_t pid = 0;                // -p/--pid
    bool pid_set = false;
    bool help = false;               // -h/--help
    bool version = false;            // -v/--version
};

}  // namespace deeptrace_cli

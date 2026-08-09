#pragma once
// command layer: command table (group/action/parameter spec) and help text.

#include <string>
#include <vector>

namespace pmem_cli {

// One positional/optional parameter of a subcommand.
struct ParamSpec {
    std::string name;    // e.g. "address"
    std::string type;    // address|number|pid|tid|string|format|format-rw|
                         // value-type|hw-type|pattern|hex-bytes|exit-code|index|flag
    bool required = false;
    std::string def;     // default value text (empty if none)
};

struct CommandSpec {
    std::string group;
    std::string action;
    std::string usage;   // e.g. "mem read <address> [size] [format]"
    std::string brief;   // one-line description (pure ASCII)
    std::vector<ParamSpec> params;
};

// Full command table (group, action, usage, brief, params).
const std::vector<CommandSpec>& command_table();

// Find a command spec; returns nullptr if not found.
const CommandSpec* find_command(const std::string& group, const std::string& action);

// True if the string is a known command group name.
bool is_group(const std::string& group);

// Grouped help text, pure ASCII.
std::string build_help_text();

// Full usage line for a single command (for usage errors).
std::string command_usage(const CommandSpec& spec);

}  // namespace pmem_cli

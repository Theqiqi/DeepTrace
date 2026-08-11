#pragma once
// script module: parse and validate a debug-run script (JSON array of steps).
// Pure standard library; no deeptrace or platform types.

#include <map>
#include <string>
#include <vector>

namespace deeptrace_cli {
namespace script {

// One script step: op name + string-valued fields.
struct Step {
    std::string op;
    std::map<std::string, std::string> fields;  // field name -> value (validated)
};

// Parse and validate a script file. On failure returns false and fills err
// with a message including line/col (JSON) or step index (validation).
bool parse_file(const std::string& path, std::vector<Step>& out, std::string& err);

// Parse and validate script text (used by tests). Same semantics as parse_file.
bool parse_text(const std::string& text, std::vector<Step>& out, std::string& err);

}  // namespace script
}  // namespace deeptrace_cli

#include "command/commands.h"
#include "command/parser.h"
#include "interface/executor.h"
#include "printing/printer.h"

#include <exception>
#include <string>

int main(int argc, char* argv[]) {
    try {
        deeptrace_cli::ParseResult pr = deeptrace_cli::parse_args(argc, argv);
        if (!pr.ok) {
            if (pr.exit_code == 2) {
                deeptrace_cli::printer::print_error(pr.error);
                std::string usage = "Usage: deeptrace_cli [options] <command> [args...]";
                std::fprintf(stderr, "%s\n", usage.c_str());
                std::fprintf(stderr, "Try 'deeptrace_cli -h' for help.\n");
            } else {
                // missing command: single-line hint, no usage dump
                deeptrace_cli::printer::print_error(pr.error);
            }
            return pr.exit_code;
        }
        if (pr.req.help) {
            deeptrace_cli::printer::print_help(deeptrace_cli::build_help_text());
            return 0;
        }
        if (pr.req.version) {
            deeptrace_cli::printer::print_version();
            return 0;
        }
        return deeptrace_cli::execute(pr.req);
    } catch (const std::exception& e) {
        std::string msg = "internal exception: ";
        msg += e.what();
        deeptrace_cli::printer::print_error(msg);
        return 1;
    }
}

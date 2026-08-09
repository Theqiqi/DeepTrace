#include "command/commands.h"
#include "command/parser.h"
#include "interface/executor.h"
#include "printing/printer.h"

#include <exception>
#include <string>

int main(int argc, char* argv[]) {
    try {
        pmem_cli::ParseResult pr = pmem_cli::parse_args(argc, argv);
        if (!pr.ok) {
            if (pr.exit_code == 2) {
                pmem_cli::printer::print_error(pr.error);
                std::string usage = "Usage: pmem_cli [options] <command> [args...]";
                std::fprintf(stderr, "%s\n", usage.c_str());
                std::fprintf(stderr, "Try 'pmem_cli -h' for help.\n");
            } else {
                // missing command: single-line hint, no usage dump
                pmem_cli::printer::print_error(pr.error);
            }
            return pr.exit_code;
        }
        if (pr.req.help) {
            pmem_cli::printer::print_help(pmem_cli::build_help_text());
            return 0;
        }
        if (pr.req.version) {
            pmem_cli::printer::print_version();
            return 0;
        }
        return pmem_cli::execute(pr.req);
    } catch (const std::exception& e) {
        std::string msg = "internal exception: ";
        msg += e.what();
        pmem_cli::printer::print_error(msg);
        return 1;
    }
}

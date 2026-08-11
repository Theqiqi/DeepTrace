#include "interface/cmd.h"

#include "deeptrace.h"

namespace deeptrace_cli {

int cmd_debug(const CommandRequest& req) {
    // Single entry: debug run. All debug operations happen inside the scripted
    // session (one invocation = one session). Standalone debug commands were
    // removed in v2.1.0: they are semantically invalid without a live debug
    // session (fake single-step, residual 0xCC breakpoints, misleading
    // registers/status). The parser rejects any other action with exit 2, so
    // reaching here with a non-run action is defensive only.
    if (req.action == "run") {
        return cmd_debug_run(req);
    }
    return internal::report_error(deeptrace::Result::InvalidArg, req.action);
}

}  // namespace deeptrace_cli

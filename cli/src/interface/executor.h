#pragma once
// interface layer: executes a parsed command by calling pmem public APIs,
// formats results through the printing layer. Returns the process exit code.

#include "command/request.h"

namespace pmem_cli {

// Execute the command described by req. Returns 0 on success, 1 on failure.
int execute(const CommandRequest& req);

}  // namespace pmem_cli

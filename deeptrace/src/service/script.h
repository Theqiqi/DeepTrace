#pragma once
#include "domain/types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace deeptrace {

// Allocate remote memory of given size and bind it to a script symbol name.
// The symbol is persisted per-PID (scripts.dat). owner is the script path
// that owns the allocation ("" = unattached). Duplicate name -> InvalidArg.
Result script_alloc(const std::string& name, size_t size, const std::string& owner,
                    uintptr_t* out_addr);

// Release the remote memory bound to a script symbol and remove the symbol.
// Symbol not found -> NotFound.
Result script_free(const std::string& name);

// Persist script enable state (per PID + path). Idempotent: enabling an
// already-enabled script returns Ok (no-op).
Result script_enable(const std::string& path);

// Remove the script enable state. Idempotent: disabling a script that is not
// enabled returns Ok (no-op).
Result script_disable(const std::string& path);

// List enabled scripts and their hooks/alloc symbols for the current process.
Result script_status(std::vector<ScriptInfo>& out);

}  // namespace deeptrace

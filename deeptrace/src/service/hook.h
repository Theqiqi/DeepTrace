#pragma once
#include "domain/types.h"

#include <cstdint>

namespace deeptrace {

// Patch the target address to `jmp newmem` (5-byte E9 rel32), saving the
// original bytes into a per-PID hook record owned by the given script path
// ("" = not script-owned). Idempotent re-set of the same (addr,newmem) pair
// rewrites identical bytes, keeps the original bytes and the owner.
Result hook_set(uintptr_t addr, uintptr_t newmem, const std::string& owner,
                HookInfo& out);

// Restore the original bytes of a hooked target and remove its record.
// No record for addr -> NotFound.
Result hook_clear(uintptr_t addr);

}  // namespace deeptrace

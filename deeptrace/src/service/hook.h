#pragma once
#include "domain/types.h"

#include <cstdint>

namespace deeptrace {

// Patch the target address to `jmp newmem` (5-byte E9 rel32), saving the
// original bytes into a per-PID hook record. Idempotent re-set of the same
// (addr,newmem) pair rewrites identical bytes and keeps the record.
Result hook_set(uintptr_t addr, uintptr_t newmem, HookInfo& out);

// Restore the original bytes of a hooked target and remove its record.
// No record for addr -> NotFound.
Result hook_clear(uintptr_t addr);

}  // namespace deeptrace

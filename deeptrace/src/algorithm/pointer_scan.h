#pragma once
// Pointer-chain reverse-walk primitives (v2.12.0). Pure computation on an
// in-memory buffer: no process I/O. The service layer feeds it memory chunks
// and re-assembles results across regions/levels.
//
// Level-0 semantics: an address A in the buffer "points at" target T if the
// qword value V at A satisfies |V - T| <= max_offset. The chain offset that
// reaches T from A is (T - V), so the caller records offsets as the final
// dereference (A + (T - V) == T).

#include "domain/types.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

namespace deeptrace::internal {

// A single hit: absolute address of the pointer slot, the address it points
// at (value), and the offset added after dereferencing to reach the matched
// target (delta = target - value, signed: may be negative when the pointer
// points above the target). For level 0 the matched target is the requested
// target; for deeper levels it is one of the previous-level slot addresses.
struct PointerHit {
    uintptr_t address = 0;  // absolute address of the qword slot
    uintptr_t value = 0;    // qword value stored at the slot (pointed address)
    uintptr_t target = 0;   // matched target (value + delta)
    int64_t delta = 0;      // target - value (signed)
};

// Find every unaligned qword in [data, data+len) whose value lies within
// +/-max_offset of target. base_addr is the absolute address of data[0].
// Results are unsorted (caller filters/sorts).
std::vector<PointerHit> scan_pointers_to(const uint8_t* data, size_t len,
                                         uintptr_t base_addr,
                                         uintptr_t target,
                                         uint32_t max_offset);

// Level-k filter: given the candidate target addresses from the previous
// level, find every unaligned qword whose value points within +/-max_offset
// of ANY candidate. Returns the absolute slot address and the delta to the
// matched candidate (delta = matched - value). A value may match multiple
// candidates; the first (lowest) matched candidate is used and only ONE hit
// is produced for that slot, so when max_offset spans several targets the
// slot may be attributed to the wrong target. Chains are therefore
// candidates: `pointer_map_rescan` re-verifies them against the live value
// address to intersect away such coincidences.
std::vector<PointerHit> scan_pointers_to_any(const uint8_t* data, size_t len,
                                             uintptr_t base_addr,
                                             const std::vector<uintptr_t>& targets,
                                             uint32_t max_offset);

// Evaluate a chain against the live memory reader: addr = root, then for each
// offset read the qword at addr and add the offset. `read_fn` reads size bytes
// at addr into buf and returns bytes read. Returns false when a read fails
// (chain invalid at runtime) or the final address overflows.
using ReadFn = std::function<size_t(uintptr_t addr, void* buf, size_t size)>;
bool eval_chain(const PointerChain& chain, const ReadFn& read_fn, uintptr_t& out);

}  // namespace deeptrace::internal

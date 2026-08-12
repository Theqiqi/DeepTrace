#include "algorithm/pointer_scan.h"

#include <algorithm>
#include <cstring>

namespace deeptrace::internal {

namespace {

// Unaligned qword load (x86 unaligned loads are fine).
inline uint64_t load_qword(const uint8_t* p) {
    uint64_t v = 0;
    std::memcpy(&v, p, sizeof(v));
    return v;
}

// Find the largest index i in targets where targets[i] <= value. Returns
// targets.end() if all targets > value.
std::vector<uintptr_t>::const_iterator
lower_bound_le(const std::vector<uintptr_t>& targets, uintptr_t value) {
    return std::upper_bound(targets.begin(), targets.end(), value);
}

}  // namespace

std::vector<PointerHit> scan_pointers_to(const uint8_t* data, size_t len,
                                         uintptr_t base_addr,
                                         uintptr_t target,
                                         uint32_t max_offset) {
    std::vector<PointerHit> out;
    if (len < sizeof(uint64_t)) return out;
    const uint64_t lo = target >= max_offset ? target - max_offset : 0;
    const uint64_t hi = static_cast<uint64_t>(target) + max_offset;
    for (size_t i = 0; i + sizeof(uint64_t) <= len; ++i) {
        uint64_t v = load_qword(data + i);
        if (v >= lo && v <= hi) {
            PointerHit h;
            h.address = base_addr + i;
            h.value = static_cast<uintptr_t>(v);
            h.target = target;
            h.delta = static_cast<int64_t>(target) - static_cast<int64_t>(v);
            out.push_back(h);
        }
    }
    return out;
}

std::vector<PointerHit> scan_pointers_to_any(const uint8_t* data, size_t len,
                                             uintptr_t base_addr,
                                             const std::vector<uintptr_t>& targets,
                                             uint32_t max_offset) {
    std::vector<PointerHit> out;
    if (len < sizeof(uint64_t) || targets.empty()) return out;
    // sorted copy for binary search
    std::vector<uintptr_t> sorted = targets;
    std::sort(sorted.begin(), sorted.end());
    for (size_t i = 0; i + sizeof(uint64_t) <= len; ++i) {
        uint64_t v = load_qword(data + i);
        // first candidate >= v - max_offset
        uintptr_t lo = v >= max_offset ? static_cast<uintptr_t>(v - max_offset) : 0;
        auto it = std::lower_bound(sorted.begin(), sorted.end(), lo);
        if (it != sorted.end() && *it <= static_cast<uintptr_t>(v) + max_offset) {
            PointerHit h;
            h.address = base_addr + i;
            h.value = static_cast<uintptr_t>(v);
            h.target = *it;
            h.delta = static_cast<int64_t>(*it) - static_cast<int64_t>(v);
            out.push_back(h);
        }
    }
    return out;
}

bool eval_chain(const PointerChain& chain, const ReadFn& read_fn, uintptr_t& out) {
    uintptr_t addr = chain.root;
    for (int64_t off : chain.offsets) {
        uint64_t v = 0;
        size_t n = read_fn(addr, &v, sizeof(v));
        if (n != sizeof(v)) return false;
        addr = static_cast<uintptr_t>(static_cast<int64_t>(v) + off);
    }
    out = addr;
    return true;
}

}  // namespace deeptrace::internal

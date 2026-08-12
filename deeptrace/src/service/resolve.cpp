#include "service/resolve.h"
#include "service/module.h"
#include "service/session.h"
#include "algorithm/pointer_scan.h"
#include "algorithm/scan.h"
#include "infrastructure/memory/memory.h"
#include "infrastructure/threadpool/threadpool.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cstring>
#include <map>
#include <mutex>
#include <set>
#include <vector>

namespace deeptrace {

Result resolve_base(const std::string& name, uintptr_t* out_base) {
    return module_base(name, out_base);
}

// ---- v2.12.0: pointer-chain reverse-walk ----

namespace {

// A partial chain candidate discovered during the reverse walk: the slot
// address (the pointer's own location) and the offsets accumulated from that
// slot down to the target. slot is the *source* address (where the pointer
// lives); offsets[0] is the first dereference offset.
struct Candidate {
    uintptr_t slot = 0;                 // where the pointer lives
    std::vector<int64_t> offsets;       // [off0, off1, ...] to reach target
};

// Collect readable committed regions (same filter as pattern_scan).
void collect_regions(void* hprocess, std::vector<MemoryRegion>& out) {
    std::vector<MemoryRegion> regions;
    if (internal::EnumMemoryRegions(hprocess, regions) != Result::Ok) return;
    for (const auto& region : regions) {
        if (region.state != MEM_COMMIT) continue;
        uint32_t prot = region.protection & 0xFF;
        if (prot == PAGE_NOACCESS) continue;
        if ((prot & PAGE_GUARD) != 0) continue;
        bool readable = (prot & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                                 PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                                 PAGE_EXECUTE_WRITECOPY)) != 0;
        if (!readable) continue;
        out.push_back(region);
    }
}

// For each region, enqueue a parallel scan that appends hits (slot address +
// delta) into `out`. The predicate is level-dependent: level 0 scans for a
// single target, deeper levels scan for any of the previous-level targets.
void scan_all_regions(void* hprocess, const std::vector<MemoryRegion>& regions,
                      const std::vector<uintptr_t>& targets,
                      uint32_t max_offset, internal::ThreadPool& pool,
                      std::vector<internal::PointerHit>& out) {
    std::mutex mu;
    const size_t CHUNK = 1u << 20;  // 1 MiB
    for (const auto& region : regions) {
        const size_t region_size = region.size;
        const uintptr_t base = region.base;
        size_t offset = 0;
        while (offset < region_size) {
            size_t want = region_size - offset;
            if (want > CHUNK) want = CHUNK;
            uintptr_t chunk_base = base + offset;
            // capture region/chunk by value for the lambda
            pool.enqueue([hprocess, chunk_base, want, max_offset, &targets,
                          &out, &mu] {
                std::vector<uint8_t> buf(want);
                Result err;
                size_t got = internal::ReadRemoteMemory(hprocess, chunk_base,
                                                        buf.data(), want, &err);
                if (err != Result::Ok || got == 0) return;
                std::vector<internal::PointerHit> hits;
                if (targets.size() == 1) {
                    hits = internal::scan_pointers_to(buf.data(), got, chunk_base,
                                                      targets[0], max_offset);
                } else {
                    hits = internal::scan_pointers_to_any(buf.data(), got,
                                                          chunk_base, targets,
                                                          max_offset);
                }
                if (hits.empty()) return;
                {
                    std::lock_guard<std::mutex> lk(mu);
                    out.insert(out.end(), hits.begin(), hits.end());
                }
            });
            offset += want;
        }
    }
    pool.wait();
}

}  // namespace

Result pointer_map_snapshot(const PointerScanConfig& cfg,
                            std::vector<PointerChain>& out) {
    out.clear();
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    // max_offset == 0 is legal: exact-pointer match (delta must be zero).
    if (cfg.target == 0 || cfg.max_level == 0)
        return Result::InvalidArg;

    // Anchor module bounds (optional).
    uintptr_t mod_base = 0, mod_end = 0;
    if (!cfg.module.empty()) {
        ModuleInfo info;
        Result r = module_find(cfg.module, info);
        if (r != Result::Ok) return r;  // NotFound when module not loaded
        mod_base = info.base;
        mod_end = info.base + info.size;
    }

    std::vector<MemoryRegion> regions;
    collect_regions(s.handle, regions);
    if (regions.empty()) return Result::Ok;

    internal::ThreadPool pool(cfg.thread_count);

    // Reverse walk (CE semantics): level 0 scans for qword slots pointing at
    // cfg.target; each deeper level treats the previous level's slots as new
    // targets. Every level's hits become candidate chains; only chains whose
    // root (the outermost slot) falls inside the anchor module are stable and
    // are emitted. All slots still feed the next level regardless of anchor.
    std::vector<Candidate> results;
    std::set<std::pair<uintptr_t, int64_t>> seen;  // (slot, first_offset)
    // current level candidates: slot -> all offset paths from that slot down
    // to the target. A slot may lie within max_offset of several targets, so
    // each slot carries a vector of paths (each a distinct chain prefix).
    using OffsetPath = std::vector<int64_t>;
    std::map<uintptr_t, std::vector<OffsetPath>> level_chains;
    level_chains[cfg.target] = {OffsetPath{}};  // level 0 target = value addr

    for (uint32_t level = 0; level < cfg.max_level; ++level) {
        std::vector<uintptr_t> targets;
        for (const auto& kv : level_chains) targets.push_back(kv.first);

        std::vector<internal::PointerHit> hits;
        scan_all_regions(s.handle, regions, targets, cfg.max_offset, pool, hits);
        if (hits.empty()) break;

        std::map<uintptr_t, std::vector<OffsetPath>> next_chains;
        size_t next_count = 0;
        for (const auto& h : hits) {
            if (results.size() + next_count >= cfg.max_results) break;
            auto it = level_chains.find(h.target);
            if (it == level_chains.end()) continue;  // not one of our targets
            if (!seen.insert({h.address, h.delta}).second) continue;

            // this slot may point at several level targets (multi-path);
            // each path yields a distinct chain prefix, all preserved
            for (const auto& path : it->second) {
                Candidate c;
                c.slot = h.address;
                c.offsets.push_back(h.delta);
                c.offsets.insert(c.offsets.end(), path.begin(), path.end());

                // emit only when the root slot is anchored inside the module
                if (mod_base == 0 ||
                    (c.slot >= mod_base && c.slot < mod_end)) {
                    results.push_back(c);
                }
                // this slot becomes a target for the next level either way
                if (level + 1 < cfg.max_level) {
                    next_chains[h.address].push_back(c.offsets);
                    ++next_count;
                }
            }
        }
        if (next_chains.empty()) break;
        level_chains = std::move(next_chains);
    }

    // Dedupe chains by (root, offsets) and cap.
    std::set<std::pair<uintptr_t, std::string>> chain_seen;
    for (const auto& c : results) {
        if (out.size() >= cfg.max_results) break;
        std::string key;
        for (uintptr_t o : c.offsets) {
            key.append(reinterpret_cast<const char*>(&o), sizeof(o));
        }
        if (!chain_seen.insert({c.slot, key}).second) continue;
        PointerChain ch;
        ch.root = c.slot;
        ch.offsets = c.offsets;
        out.push_back(std::move(ch));
    }
    return Result::Ok;
}

Result pointer_map_rescan(const std::vector<PointerChain>& base,
                          uintptr_t new_target,
                          const PointerScanConfig& cfg,
                          std::vector<PointerChain>& out) {
    out.clear();
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;
    // max_offset == 0 is legal: exact-pointer match.
    if (new_target == 0) return Result::InvalidArg;
    if (base.empty()) return Result::Ok;

    internal::ThreadPool pool(cfg.thread_count);
    std::mutex mu;
    const uint32_t max_offset = cfg.max_offset;

    for (const auto& chain : base) {
        // capture chain by value: the loop variable dies before wait()
        pool.enqueue([chain, new_target, max_offset, &out, &mu] {
            auto read_fn = [](uintptr_t addr, void* buf, size_t size) -> size_t {
                Result err;
                return internal::ReadRemoteMemory(internal::session().handle,
                                                  addr, buf, size, &err);
            };
            uintptr_t final_addr = 0;
            if (!internal::eval_chain(chain, read_fn, final_addr)) return;
            uintptr_t diff = final_addr > new_target ? final_addr - new_target
                                                     : new_target - final_addr;
            if (diff > max_offset) return;
            std::lock_guard<std::mutex> lk(mu);
            out.push_back(chain);
        });
    }
    pool.wait();
    return Result::Ok;
}


Result pattern_scan(const std::string& pattern, std::vector<uintptr_t>& out) {
    out.clear();
    auto& s = internal::session();
    if (!s.handle) return Result::NotAttached;

    std::vector<internal::PatternByte> pat;
    if (!internal::parse_pattern(pattern, pat)) return Result::BadFormat;
    if (pat.empty()) return Result::InvalidArg;

    std::vector<MemoryRegion> regions;
    Result r = internal::EnumMemoryRegions(s.handle, regions);
    if (r != Result::Ok) return r;

    const size_t CHUNK = 1u << 20;
    const size_t plen = pat.size();
    std::vector<uint8_t> buf;

    for (const auto& region : regions) {
        if (region.state != MEM_COMMIT) continue;
        uint32_t prot = region.protection & 0xFF;
        if (prot == PAGE_NOACCESS) continue;
        if ((prot & PAGE_GUARD) != 0) continue;
        bool readable = (prot & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                                 PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                                 PAGE_EXECUTE_WRITECOPY)) != 0;
        if (!readable) continue;

        size_t region_size = region.size;
        uintptr_t base = region.base;
        size_t offset = 0;
        // overlap handling across chunk boundaries
        size_t overlap = plen > 1 ? plen - 1 : 0;
        std::vector<uint8_t> carry;  // tail of previous chunk
        while (offset < region_size) {
            size_t want = region_size - offset;
            if (want > CHUNK) want = CHUNK;
            buf.assign(want + overlap, 0);
            Result err;
            size_t got = internal::ReadRemoteMemory(s.handle, base + offset, buf.data(),
                                                    want, &err);
            if (err != Result::Ok || got == 0) break;
            // copy carry from previous read to handle cross-chunk matches
            if (!carry.empty() && carry.size() <= want) {
                std::memcpy(buf.data(), carry.data(), carry.size());
            }
            std::vector<size_t> hits =
                internal::scan_bytes(buf.data(), got, pat);
            for (size_t h : hits) {
                if (h + plen <= got) {
                    out.push_back(base + offset + h);
                }
            }
            offset += got;
            // save tail for next chunk
            carry.assign(buf.begin() + (got > overlap ? got - overlap : 0),
                         buf.begin() + got);
            if (got < want) break;
        }
    }
    return Result::Ok;
}

}  // namespace deeptrace

// Example: pointer-chain reverse scan (v2.12.0).
// Usage: pointer_chain_scan.exe <pid> <value_addr> [module]
//   pid        - target process id (attach target)
//   value_addr - address of the value to reverse-walk from (hex, e.g. 0x1400D008)
//   module     - optional anchor module name (e.g. Game.exe); chains whose root
//                is outside this module are filtered out
// Flow: attach -> pointer_map_snapshot -> print chains -> detach.
#include "deeptrace.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("usage: %s <pid> <value_addr> [module]\n", argv[0]);
        return 1;
    }
    const uint32_t pid = static_cast<uint32_t>(std::strtoul(argv[1], nullptr, 10));
    const uintptr_t value_addr =
        static_cast<uintptr_t>(std::strtoull(argv[2], nullptr, 0));

    if (deeptrace::attach(pid) != deeptrace::Result::Ok) {
        std::fprintf(stderr, "attach failed (may need administrator privileges)\n");
        return 1;
    }

    // 1. configure the scan
    deeptrace::PointerScanConfig cfg;
    cfg.target = value_addr;      // reverse-walk from this value address
    cfg.max_offset = 2048;        // pointer within target +/- 2048 counts
    cfg.max_level = 5;            // up to 5 offset levels
    cfg.max_results = 10000;      // output cap
    if (argc >= 4) cfg.module = argv[3];  // anchor chains inside this module

    // 2. snapshot: find chains that currently reach the value
    std::vector<deeptrace::PointerChain> chains;
    if (deeptrace::pointer_map_snapshot(cfg, chains) != deeptrace::Result::Ok) {
        std::fprintf(stderr, "snapshot failed\n");
        deeptrace::detach();
        return 1;
    }
    std::printf("snapshot: %zu chain(s)\n", chains.size());
    for (const auto& c : chains) {
        std::printf("  root=%p", reinterpret_cast<void*>(c.root));
        for (int64_t off : c.offsets) std::printf(" %+lld", static_cast<long long>(off));
        std::printf("\n");
    }

    // 3. (workflow) after a game restart the value address moves: re-locate it,
    //    then pointer_map_rescan(chains, new_addr, cfg, stable) keeps only the
    //    chains that still resolve to the new address (false-positive filter).

    deeptrace::detach();
    return 0;
}

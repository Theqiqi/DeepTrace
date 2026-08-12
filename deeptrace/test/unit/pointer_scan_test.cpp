#include "algorithm/pointer_scan.h"

#include <gtest/gtest.h>

#include <cstring>
#include <vector>

using namespace deeptrace;
using namespace deeptrace::internal;

namespace {

// Build a buffer where qword at byte `at` stores the value `val`.
// No-op when the value would not fully fit (keeps short-buffer tests safe).
std::vector<uint8_t> make_buf(size_t size, size_t at, uint64_t val) {
    std::vector<uint8_t> buf(size, 0);
    if (at + sizeof(val) <= size) std::memcpy(buf.data() + at, &val, sizeof(val));
    return buf;
}

}  // namespace

TEST(PointerScan, HitBelowTarget) {
    // target at 0x1000, pointer value 0xFE0 -> delta +0x20
    std::vector<uint8_t> buf = make_buf(32, 0, 0xFE0);
    auto hits = scan_pointers_to(buf.data(), buf.size(), 0x5000000, 0x1000, 2048);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].address, 0x5000000u);
    EXPECT_EQ(hits[0].value, 0xFE0u);
    EXPECT_EQ(hits[0].target, 0x1000u);
    EXPECT_EQ(hits[0].delta, 0x20);
}

TEST(PointerScan, HitAboveTargetNegativeDelta) {
    // pointer value 0x1010 > target 0x1000 -> delta -0x10
    std::vector<uint8_t> buf = make_buf(32, 0, 0x1010);
    auto hits = scan_pointers_to(buf.data(), buf.size(), 0x100, 0x1000, 2048);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].delta, -0x10);
}

TEST(PointerScan, OffsetBoundariesInclusive) {
    // value exactly target - max_offset and target + max_offset both match
    std::vector<uint8_t> buf(48, 0);
    uint64_t lo = 0x1000 - 2048;
    uint64_t hi = 0x1000 + 2048;
    std::memcpy(buf.data() + 0, &lo, 8);
    std::memcpy(buf.data() + 8, &hi, 8);
    auto hits = scan_pointers_to(buf.data(), buf.size(), 0, 0x1000, 2048);
    ASSERT_EQ(hits.size(), 2u);
}

TEST(PointerScan, OutsideOffsetNoMatch) {
    std::vector<uint8_t> buf = make_buf(32, 0, 0x1000 + 2049);
    EXPECT_TRUE(scan_pointers_to(buf.data(), buf.size(), 0, 0x1000, 2048).empty());
}

TEST(PointerScan, UnalignedSlots) {
    // qword stored at odd offset must still be found (unaligned scan)
    std::vector<uint8_t> buf(48, 0);
    std::memcpy(buf.data() + 5, "\x00\x10\x00\x00\x00\x00\x00\x00", 8);  // 0x1000 at offset 5
    auto hits = scan_pointers_to(buf.data(), buf.size(), 0x100000, 0x1000, 2048);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].address, 0x100000u + 5);
}

TEST(PointerScan, TooShortBuffer) {
    std::vector<uint8_t> buf = make_buf(7, 0, 0x1000);  // < 8 bytes
    EXPECT_TRUE(scan_pointers_to(buf.data(), buf.size(), 0, 0x1000, 2048).empty());
}

TEST(PointerScan, AnyMatchesMultipleTargets) {
    // two targets; value matches the first candidate in sorted order
    std::vector<uint8_t> buf = make_buf(32, 0, 0x1000);
    std::vector<uintptr_t> targets = {0x1010, 0x1000};
    auto hits = scan_pointers_to_any(buf.data(), buf.size(), 0x5000000, targets, 2048);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0].target, 0x1000u);   // lower_bound over sorted {0x1000, 0x1010}
    EXPECT_EQ(hits[0].delta, 0);
}

TEST(PointerScan, AnyNoMatch) {
    std::vector<uint8_t> buf = make_buf(32, 0, 0x9000);
    std::vector<uintptr_t> targets = {0x1000};
    EXPECT_TRUE(scan_pointers_to_any(buf.data(), buf.size(), 0, targets, 2048).empty());
}

TEST(PointerScan, AnyEmptyTargets) {
    std::vector<uint8_t> buf = make_buf(32, 0, 0x1000);
    EXPECT_TRUE(scan_pointers_to_any(buf.data(), buf.size(), 0, {}, 2048).empty());
}

namespace {

size_t FakeRead(std::vector<uint8_t>& mem, uintptr_t addr, void* buf, size_t size) {
    size_t off = addr - 0x1000000;
    if (off + size > mem.size()) return 0;
    std::memcpy(buf, mem.data() + off, size);
    return size;
}

}  // namespace

TEST(PointerScan, EvalChainTwoHops) {
    // root at 0x1000000 stores hop1 (0x1000100); reading at 0x1000100+0x40
    // (i.e. mem[0x140]) yields hop2 (0x1000200); +0x20 -> 0x1000220.
    std::vector<uint8_t> mem(0x1000300, 0);
    uint64_t hop1 = 0x1000100, hop2 = 0x1000200;
    std::memcpy(mem.data() + 0x0, &hop1, 8);
    std::memcpy(mem.data() + 0x140, &hop2, 8);

    PointerChain chain;
    chain.root = 0x1000000;
    chain.offsets = {0x40, 0x20};
    uintptr_t out = 0;
    auto read_fn = [&mem](uintptr_t addr, void* buf, size_t size) -> size_t {
        return FakeRead(mem, addr, buf, size);
    };
    ASSERT_TRUE(eval_chain(chain, read_fn, out));
    EXPECT_EQ(out, 0x1000220u);
}

TEST(PointerScan, EvalChainReadFailure) {
    PointerChain chain;
    chain.root = 0x1000100;  // offset 0x100 is beyond the 0x40-byte buffer
    chain.offsets = {0x0};
    std::vector<uint8_t> mem(0x40, 0);
    auto read_fn = [&mem](uintptr_t addr, void* buf, size_t size) -> size_t {
        return FakeRead(mem, addr, buf, size);
    };
    uintptr_t out = 0;
    EXPECT_FALSE(eval_chain(chain, read_fn, out));
}

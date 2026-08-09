#include "algorithm/scan.h"

#include <gtest/gtest.h>

#include <vector>

using namespace deeptrace::internal;

TEST(Scan, ParseValid) {
    std::vector<PatternByte> p;
    EXPECT_TRUE(parse_pattern("48 8B ?? ?? 00", p));
    ASSERT_EQ(p.size(), 5u);
    EXPECT_FALSE(p[0].wildcard);
    EXPECT_EQ(p[0].value, 0x48);
    EXPECT_TRUE(p[2].wildcard);
    EXPECT_FALSE(p[4].wildcard);
    EXPECT_EQ(p[4].value, 0x00);
}

TEST(Scan, ParseBadFormat) {
    std::vector<PatternByte> p;
    EXPECT_FALSE(parse_pattern("48 8B ZZ", p));
    EXPECT_FALSE(parse_pattern("48 8", p));
}

TEST(Scan, ParseEmpty) {
    std::vector<PatternByte> p;
    EXPECT_FALSE(parse_pattern("", p));
}

TEST(Scan, FindExact) {
    const uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x02, 0x03, 0x04};
    std::vector<PatternByte> p;
    ASSERT_TRUE(parse_pattern("02 03 04", p));
    auto hits = scan_bytes(data, sizeof(data), p);
    ASSERT_EQ(hits.size(), 2u);
    EXPECT_EQ(hits[0], 1u);
    EXPECT_EQ(hits[1], 5u);
}

TEST(Scan, FindWildcard) {
    const uint8_t data[] = {0xAA, 0x48, 0x8B, 0xFF, 0x12, 0x48, 0x8B, 0x99, 0x00};
    std::vector<PatternByte> p;
    ASSERT_TRUE(parse_pattern("48 8B ?? 00", p));
    auto hits = scan_bytes(data, sizeof(data), p);
    ASSERT_EQ(hits.size(), 1u);
    EXPECT_EQ(hits[0], 5u);
}

TEST(Scan, NoMatch) {
    const uint8_t data[] = {0x01, 0x02, 0x03};
    std::vector<PatternByte> p;
    ASSERT_TRUE(parse_pattern("FF FF FF", p));
    EXPECT_TRUE(scan_bytes(data, sizeof(data), p).empty());
}

TEST(Scan, PatternLongerThanData) {
    const uint8_t data[] = {0x01, 0x02};
    std::vector<PatternByte> p;
    ASSERT_TRUE(parse_pattern("01 02 03", p));
    EXPECT_TRUE(scan_bytes(data, sizeof(data), p).empty());
}

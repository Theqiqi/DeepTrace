#include "algorithm/hex.h"

#include <gtest/gtest.h>

#include <vector>

using namespace deeptrace::internal;

TEST(Hex, Encode) {
    const uint8_t data[] = {0x48, 0x8B, 0x45, 0x08, 0xCC};
    EXPECT_EQ(hex_encode(data, sizeof(data)), "488B4508CC");
}

TEST(Hex, EncodeEmpty) {
    EXPECT_EQ(hex_encode(nullptr, 0), "");
}

TEST(Hex, DecodeValid) {
    std::vector<uint8_t> out;
    EXPECT_TRUE(hex_decode("48 8B 45 08", out));
    ASSERT_EQ(out.size(), 4u);
    EXPECT_EQ(out[0], 0x48);
    EXPECT_EQ(out[3], 0x08);
}

TEST(Hex, DecodeWithPrefix) {
    std::vector<uint8_t> out;
    EXPECT_TRUE(hex_decode("0xDEADBEEF", out));
    ASSERT_EQ(out.size(), 4u);
    EXPECT_EQ(out[0], 0xDE);
    EXPECT_EQ(out[3], 0xEF);
}

TEST(Hex, DecodeOddLength) {
    std::vector<uint8_t> out;
    EXPECT_FALSE(hex_decode("48 8B 4", out));
}

TEST(Hex, DecodeInvalidChar) {
    std::vector<uint8_t> out;
    EXPECT_FALSE(hex_decode("48 8G", out));
}

TEST(Hex, ParseUint64Hex) {
    uint64_t v = 0;
    EXPECT_TRUE(parse_uint64("0x140001000", v));
    EXPECT_EQ(v, 0x140001000ULL);
}

TEST(Hex, ParseUint64Dec) {
    uint64_t v = 0;
    EXPECT_TRUE(parse_uint64("5368709120", v));
    EXPECT_EQ(v, 5368709120ULL);
}

TEST(Hex, ParseUint64Overflow) {
    uint64_t v = 0;
    EXPECT_FALSE(parse_uint64("0xFFFFFFFFFFFFFFFFF", v));
}

TEST(Hex, ParseUint64Invalid) {
    uint64_t v = 0;
    EXPECT_FALSE(parse_uint64("0xZZ", v));
    EXPECT_FALSE(parse_uint64("", v));
}

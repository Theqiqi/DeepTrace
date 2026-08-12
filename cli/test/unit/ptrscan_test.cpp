#include "interface/ptrscan.h"

#include <gtest/gtest.h>

#include <string>

using namespace deeptrace_cli;

namespace {

bool parse_ok(const std::string& text, ptrscan::Config& out) {
    std::string err;
    return ptrscan::parse_text(text, out, err);
}

bool parse_fail(const std::string& text, std::string& err) {
    ptrscan::Config cfg;
    return !ptrscan::parse_text(text, cfg, err);
}

}  // namespace

TEST(Ptrscan, ValidMinimal) {
    ptrscan::Config cfg;
    ASSERT_TRUE(parse_ok("{\"version\":1,\"target\":\"0x1000\"}", cfg));
    EXPECT_EQ(cfg.target, 0x1000u);
    EXPECT_EQ(cfg.max_offset, 2048u);   // defaults
    EXPECT_EQ(cfg.max_level, 5u);
    EXPECT_EQ(cfg.max_results, 10000u);
    EXPECT_EQ(cfg.threads, 0u);
    EXPECT_TRUE(cfg.module.empty());
    EXPECT_FALSE(cfg.has_rescan);
}

TEST(Ptrscan, ValidFull) {
    ptrscan::Config cfg;
    ASSERT_TRUE(parse_ok(
        "{\"version\":1,\"target\":\"0x7FF62A1B2100\",\"module\":\"Game.exe\","
        "\"max_offset\":1024,\"max_level\":3,\"max_results\":500,"
        "\"threads\":4,\"rescan\":{\"target\":\"0x100\"}}",
        cfg));
    EXPECT_EQ(cfg.target, 0x7FF62A1B2100ULL);
    EXPECT_EQ(cfg.module, "Game.exe");
    EXPECT_EQ(cfg.max_offset, 1024u);
    EXPECT_EQ(cfg.max_level, 3u);
    EXPECT_EQ(cfg.max_results, 500u);
    EXPECT_EQ(cfg.threads, 4u);
    EXPECT_TRUE(cfg.has_rescan);
    EXPECT_EQ(cfg.rescan_target, 0x100u);
}

TEST(Ptrscan, ValidRescanNull) {
    ptrscan::Config cfg;
    ASSERT_TRUE(parse_ok("{\"target\":\"0x10\",\"rescan\":null}", cfg));
    EXPECT_FALSE(cfg.has_rescan);
}

TEST(Ptrscan, ValidMaxOffsetZero) {
    // max_offset 0 = exact-pointer match (v2.12.0 lib semantics).
    ptrscan::Config cfg;
    ASSERT_TRUE(parse_ok("{\"target\":\"0x10\",\"max_offset\":0}", cfg));
    EXPECT_EQ(cfg.max_offset, 0u);
}

TEST(Ptrscan, DecimalTarget) {
    ptrscan::Config cfg;
    ASSERT_TRUE(parse_ok("{\"target\":\"123456\"}", cfg));
    EXPECT_EQ(cfg.target, 123456u);
}

TEST(Ptrscan, MissingTargetFails) {
    std::string err;
    EXPECT_TRUE(parse_fail("{\"version\":1}", err));
    EXPECT_NE(err.find("target"), std::string::npos);
}

TEST(Ptrscan, ZeroTargetFails) {
    std::string err;
    EXPECT_TRUE(parse_fail("{\"target\":\"0x0\"}", err));
}

TEST(Ptrscan, BadVersionFails) {
    std::string err;
    EXPECT_TRUE(parse_fail("{\"version\":2,\"target\":\"0x10\"}", err));
    EXPECT_NE(err.find("invalid version"), std::string::npos);
}

TEST(Ptrscan, UnknownFieldFails) {
    std::string err;
    EXPECT_TRUE(parse_fail("{\"target\":\"0x10\",\"bogus\":1}", err));
    EXPECT_NE(err.find("unknown top-level field 'bogus'"), std::string::npos);
}

TEST(Ptrscan, BadMaxLevelFails) {
    std::string err;
    EXPECT_TRUE(parse_fail("{\"target\":\"0x10\",\"max_level\":0}", err));
    EXPECT_NE(err.find("invalid 'max_level'"), std::string::npos);
}

TEST(Ptrscan, BadThreadsFails) {
    std::string err;
    EXPECT_TRUE(parse_fail("{\"target\":\"0x10\",\"threads\":-1}", err));
}

TEST(Ptrscan, BadRescanShapeFails) {
    std::string err;
    EXPECT_TRUE(parse_fail("{\"target\":\"0x10\",\"rescan\":42}", err));
    EXPECT_NE(err.find("'rescan' must be null or an object"), std::string::npos);
}

TEST(Ptrscan, ParseErrorHasLineCol) {
    std::string err;
    EXPECT_TRUE(parse_fail("not json", err));
    EXPECT_NE(err.find("ptrscan parse error at line 1 col 1"), std::string::npos);
}

TEST(Ptrscan, NonObjectRootFails) {
    std::string err;
    EXPECT_TRUE(parse_fail("[1,2,3]", err));
    EXPECT_NE(err.find("top-level must be an object"), std::string::npos);
}

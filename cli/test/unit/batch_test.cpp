// Unit tests for the v2.9.0 batch-locator JSON parser (interface/batch.cpp):
// JSON syntax, locator validation (three root sources), type/count/value
// rules, and read vs write mode semantics. Pure text parsing; no session.

#include "interface/batch.h"

#include <gtest/gtest.h>

#include <string>

namespace {

using deeptrace_cli::batch::File;
using deeptrace_cli::batch::OffsetPath;
using deeptrace_cli::batch::parse_text;

bool parse_ok(const std::string& text, File& out, bool write_mode = false) {
    std::string err;
    return parse_text(text, write_mode, out, err);
}

bool parse_fail(const std::string& text, std::string& err, bool write_mode = false) {
    File out;
    return !parse_text(text, write_mode, out, err);
}

// ---- happy paths ----

TEST(BatchParse, FullFileAllSources) {
    File f;
    ASSERT_TRUE(parse_ok(
        "{ \"version\": 1, \"process\": \"Game.exe\",\n"
        "  \"values\": {\n"
        "    \"hp\":   { \"module\": \"Game.exe\", \"base\": \"0x123456\",\n"
        "               \"offsets\": [\"0x10\", \"0x20\"], \"type\": \"float\" },\n"
        "    \"ptr\":  { \"symbol\": \"sunObjPtr\", \"base\": \"8\",\n"
        "               \"offsets\": [\"0x8\"], \"type\": \"qword\" },\n"
        "    \"abs\":  { \"base\": \"0x140001000\", \"type\": \"dword\" },\n"
        "    \"name\": { \"base\": \"0x100\", \"offsets\": [\"0x40\"],\n"
        "               \"type\": \"string\" },\n"
        "    \"buf\":  { \"base\": \"0x200\", \"type\": \"bytes\", \"count\": 16 }\n"
        "  } }",
        f));
    EXPECT_EQ(f.process, "Game.exe");
    ASSERT_EQ(f.items.size(), 5u);

    EXPECT_EQ(f.items[0].name, "hp");
    EXPECT_EQ(f.items[0].module, "Game.exe");
    EXPECT_TRUE(f.items[0].symbol.empty());
    EXPECT_EQ(f.items[0].base, 0x123456u);
    ASSERT_EQ(f.items[0].offsets.size(), 2u);
    EXPECT_EQ(f.items[0].offsets[0], 0x10u);
    EXPECT_EQ(f.items[0].offsets[1], 0x20u);
    EXPECT_EQ(f.items[0].type, "float");

    EXPECT_EQ(f.items[1].name, "ptr");
    EXPECT_TRUE(f.items[1].module.empty());
    EXPECT_EQ(f.items[1].symbol, "sunObjPtr");
    EXPECT_EQ(f.items[1].base, 8u);
    ASSERT_EQ(f.items[1].offsets.size(), 1u);
    EXPECT_EQ(f.items[1].offsets[0], 0x8u);
    EXPECT_EQ(f.items[1].type, "qword");

    EXPECT_EQ(f.items[2].name, "abs");
    EXPECT_TRUE(f.items[2].module.empty());
    EXPECT_TRUE(f.items[2].symbol.empty());
    EXPECT_EQ(f.items[2].base, 0x140001000u);
    EXPECT_TRUE(f.items[2].offsets.empty());
    EXPECT_EQ(f.items[2].type, "dword");

    EXPECT_EQ(f.items[3].type, "string");
    EXPECT_EQ(f.items[4].type, "bytes");
    EXPECT_EQ(f.items[4].count, 16u);
}

TEST(BatchParse, VersionAndProcessOptional) {
    File f;
    ASSERT_TRUE(parse_ok(
        "{ \"values\": { \"a\": { \"base\": \"0x1000\", \"type\": \"byte\" } } }",
        f));
    EXPECT_TRUE(f.process.empty());
    ASSERT_EQ(f.items.size(), 1u);
}

TEST(BatchParse, NumbersAcceptedAsBareLiterals) {
    File f;
    // bare JSON numbers (decimal) and hex strings both work as offsets
    ASSERT_TRUE(parse_ok(
        "{ \"version\": 1,\n"
        "  \"values\": { \"a\": { \"base\": 4096, \"offsets\": [16, \"0x20\"],\n"
        "                        \"type\": \"word\", \"count\": 2 } } }",
        f));
    EXPECT_EQ(f.items[0].base, 4096u);
    ASSERT_EQ(f.items[0].offsets.size(), 2u);
    EXPECT_EQ(f.items[0].offsets[0], 16u);
    EXPECT_EQ(f.items[0].offsets[1], 0x20u);
}

TEST(BatchParse, EmptyValuesAllowed) {
    File f;
    ASSERT_TRUE(parse_ok("{ \"version\": 1, \"values\": {} }", f));
    EXPECT_TRUE(f.items.empty());
}

// ---- read mode: value field optional and ignored ----

TEST(BatchParse, ReadModeIgnoresValue) {
    File f;
    ASSERT_TRUE(parse_ok(
        "{ \"values\": { \"a\": { \"base\": \"0x1000\", \"type\": \"qword\",\n"
        "                        \"value\": \"1122334455667788\" } } }",
        f));
    EXPECT_EQ(f.items[0].value, "1122334455667788");
}

// ---- write mode: value required + validated ----

TEST(BatchParse, WriteModeValueRequired) {
    std::string err;
    EXPECT_TRUE(parse_fail(
        "{ \"values\": { \"a\": { \"base\": \"0x1000\", \"type\": \"qword\" } } }",
        err, /*write_mode=*/true));
    EXPECT_NE(err.find("missing 'value'"), std::string::npos);
}

TEST(BatchParse, WriteModeValueValidatedPerType) {
    File f;
    // byte range exceeded
    std::string err;
    EXPECT_TRUE(parse_fail(
        "{ \"values\": { \"a\": { \"base\": \"0x1000\", \"type\": \"byte\",\n"
        "                        \"value\": \"0x100\" } } }",
        err, true));
    EXPECT_NE(err.find("invalid value"), std::string::npos);
    // float rejects hex
    EXPECT_TRUE(parse_fail(
        "{ \"values\": { \"a\": { \"base\": \"0x1000\", \"type\": \"float\",\n"
        "                        \"value\": \"0x10\" } } }",
        err, true));
    // bytes length must match count
    EXPECT_TRUE(parse_fail(
        "{ \"values\": { \"a\": { \"base\": \"0x1000\", \"type\": \"bytes\",\n"
        "                        \"count\": 4, \"value\": \"68656C\" } } }",
        err, true));
    EXPECT_NE(err.find("!= 'count'"), std::string::npos);
    // valid bytes (space-separated) passes
    ASSERT_TRUE(parse_ok(
        "{ \"values\": { \"a\": { \"base\": \"0x1000\", \"type\": \"bytes\",\n"
        "                        \"count\": 3, \"value\": \"68 65 6C\" } } }",
        f, true));
    // valid qword dec/hex passes
    ASSERT_TRUE(parse_ok(
        "{ \"values\": { \"a\": { \"base\": \"0x1000\", \"type\": \"qword\",\n"
        "                        \"value\": \"0x1122334455667788\" } } }",
        f, true));
}

// ---- locator root-source rules ----

TEST(BatchParse, RootSourceConflict) {
    std::string err;
    EXPECT_TRUE(parse_fail(
        "{ \"values\": { \"a\": { \"module\": \"m.dll\", \"symbol\": \"s\",\n"
        "                        \"type\": \"qword\" } } }",
        err));
    EXPECT_NE(err.find("mutually exclusive"), std::string::npos);
}

TEST(BatchParse, RootSourceMissing) {
    std::string err;
    EXPECT_TRUE(parse_fail(
        "{ \"values\": { \"a\": { \"type\": \"qword\" } } }", err));
    EXPECT_NE(err.find("missing locator"), std::string::npos);
}

TEST(BatchParse, ModuleWithNoBaseDefaultsZero) {
    File f;
    ASSERT_TRUE(parse_ok(
        "{ \"values\": { \"a\": { \"module\": \"Game.exe\", \"type\": \"qword\" } } }",
        f));
    EXPECT_EQ(f.items[0].base, 0u);
}

TEST(BatchParse, SymbolWithNoBaseDefaultsZero) {
    File f;
    ASSERT_TRUE(parse_ok(
        "{ \"values\": { \"a\": { \"symbol\": \"slot\", \"type\": \"qword\" } } }",
        f));
    EXPECT_EQ(f.items[0].base, 0u);
}

// ---- type / count rules ----

TEST(BatchParse, InvalidTypeRejected) {
    std::string err;
    EXPECT_TRUE(parse_fail(
        "{ \"values\": { \"a\": { \"base\": \"0x1000\", \"type\": \"int32\" } } }",
        err));
    EXPECT_NE(err.find("invalid or missing 'type'"), std::string::npos);
    EXPECT_TRUE(parse_fail(
        "{ \"values\": { \"a\": { \"base\": \"0x1000\" } } }", err));
}

TEST(BatchParse, BytesRequiresCount) {
    std::string err;
    EXPECT_TRUE(parse_fail(
        "{ \"values\": { \"a\": { \"base\": \"0x1000\", \"type\": \"bytes\" } } }",
        err));
    EXPECT_NE(err.find("'count' required"), std::string::npos);
}

TEST(BatchParse, CountBounds) {
    std::string err;
    // zero
    EXPECT_TRUE(parse_fail(
        "{ \"values\": { \"a\": { \"base\": \"0x1000\", \"type\": \"bytes\",\n"
        "                        \"count\": 0 } } }",
        err));
    // beyond cap
    EXPECT_TRUE(parse_fail(
        "{ \"values\": { \"a\": { \"base\": \"0x1000\", \"type\": \"bytes\",\n"
        "                        \"count\": 70000 } } }",
        err));
}

// ---- unknown fields / structure ----

TEST(BatchParse, UnknownFieldRejected) {
    std::string err;
    EXPECT_TRUE(parse_fail(
        "{ \"values\": { \"a\": { \"base\": \"0x1000\", \"type\": \"qword\",\n"
        "                        \"adr\": \"0x1\" } } }",
        err));
    EXPECT_NE(err.find("unknown field"), std::string::npos);
    EXPECT_TRUE(parse_fail(
        "{ \"versoin\": 1, \"values\": {} }", err));
    EXPECT_NE(err.find("unknown top-level field"), std::string::npos);
}

TEST(BatchParse, VersionMustBeOne) {
    std::string err;
    EXPECT_TRUE(parse_fail(
        "{ \"version\": 2, \"values\": {} }", err));
    EXPECT_NE(err.find("expected 1"), std::string::npos);
}

TEST(BatchParse, TopLevelShape) {
    std::string err;
    EXPECT_TRUE(parse_fail("[1,2]", err));               // not an object
    EXPECT_NE(err.find("top-level must be an object"), std::string::npos);
    EXPECT_TRUE(parse_fail("{ \"values\": [] }", err));  // values not an object
    EXPECT_NE(err.find("'values' must be an object"), std::string::npos);
    EXPECT_TRUE(parse_fail("{ \"values\": {} ", err));   // unbalanced brace
}

TEST(BatchParse, JsonSyntaxErrorHasLineCol) {
    std::string err;
    EXPECT_TRUE(parse_fail(
        "{\n  \"values\": { \"a\": { \"base\": \"0x1000\" ,, \"type\": \"qword\" } }\n}",
        err));
    EXPECT_NE(err.find("batch parse error at line"), std::string::npos);
    EXPECT_NE(err.find("col"), std::string::npos);
}

}  // namespace

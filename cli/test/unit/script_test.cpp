#include "interface/script.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace deeptrace_cli;

namespace {

bool parse_ok(const std::string& text, std::vector<script::Step>& out) {
    std::string err;
    if (!script::parse_text(text, out, err)) {
        ADD_FAILURE() << "parse failed: " << err;
        return false;
    }
    return true;
}

std::string parse_err(const std::string& text) {
    std::vector<script::Step> steps;
    std::string err;
    if (script::parse_text(text, steps, err)) return "";
    return err;
}

}  // namespace

// ---- happy path ----

TEST(Script, MinimalEmptyScript) {
    std::vector<script::Step> steps;
    ASSERT_TRUE(parse_ok("[]", steps));
    EXPECT_EQ(steps.size(), 0u);
}

TEST(Script, SingleBreakStep) {
    std::vector<script::Step> steps;
    ASSERT_TRUE(parse_ok("[{\"op\": \"break\", \"addr\": \"0x140001000\"}]", steps));
    ASSERT_EQ(steps.size(), 1u);
    EXPECT_EQ(steps[0].op, "break");
    EXPECT_EQ(steps[0].fields.at("addr"), "0x140001000");
}

TEST(Script, DefaultsInjected) {
    std::vector<script::Step> steps;
    ASSERT_TRUE(parse_ok(
        "[{\"op\": \"continue\"}, {\"op\": \"read\", \"addr\": \"0x1000\", \"size\": \"16\"},"
        " {\"op\": \"disasm\", \"addr\": \"0x1000\"}, {\"op\": \"step\"}]",
        steps));
    ASSERT_EQ(steps.size(), 4u);
    EXPECT_EQ(steps[0].fields.at("timeout_ms"), "5000");
    EXPECT_EQ(steps[1].fields.at("format"), "hex");
    EXPECT_EQ(steps[2].fields.at("count"), "8");
    EXPECT_EQ(steps[3].fields.at("tid"), "0");
}

TEST(Script, Escapes) {
    std::vector<script::Step> steps;
    ASSERT_TRUE(parse_ok(
        "[{\"op\": \"watch_add\", \"desc\": \"a \\\"q\\\" \\\\ w\", \"addr\": \"0x1000\","
        " \"type\": \"dword\"}]",
        steps));
    ASSERT_EQ(steps.size(), 1u);
    EXPECT_EQ(steps[0].fields.at("desc"), "a \"q\" \\ w");
}

TEST(Script, MultiLine) {
    std::vector<script::Step> steps;
    ASSERT_TRUE(parse_ok(
        "[\n"
        "  {\"op\": \"break\", \"addr\": \"0x1000\"},\n"
        "  {\"op\": \"continue\", \"timeout_ms\": \"1000\"},\n"
        "  {\"op\": \"registers\"}\n"
        "]\n",
        steps));
    ASSERT_EQ(steps.size(), 3u);
}

TEST(Script, TrailingCommaRejected) {
    std::string err = parse_err("[{\"op\": \"break\", \"addr\": \"0x1000\"},]");
    EXPECT_NE(err.find("script parse error at line"), std::string::npos);
}

// ---- parse errors ----

TEST(Script, TopLevelNotArray) {
    std::string err = parse_err("{\"op\": \"break\"}");
    EXPECT_NE(err.find("top-level must be an array"), std::string::npos);
}

TEST(Script, StepNotObject) {
    std::string err = parse_err("[\"break\"]");
    EXPECT_NE(err.find("step must be an object"), std::string::npos);
}

TEST(Script, NonStringValue) {
    std::string err = parse_err("[{\"op\": \"break\", \"addr\": 123}]");
    EXPECT_NE(err.find("expected '\"'"), std::string::npos);
}

TEST(Script, MissingOp) {
    std::string err = parse_err("[{\"addr\": \"0x1000\"}]");
    EXPECT_NE(err.find("step 1: missing field 'op'"), std::string::npos);
}

TEST(Script, UnknownOp) {
    std::string err = parse_err("[{\"op\": \"frobnicate\"}]");
    EXPECT_NE(err.find("step 1: unknown op 'frobnicate'"), std::string::npos);
}

TEST(Script, UnknownField) {
    std::string err = parse_err("[{\"op\": \"break\", \"addr\": \"0x1000\", \"foo\": \"1\"}]");
    EXPECT_NE(err.find("step 1 (break): unknown field 'foo'"), std::string::npos);
}

TEST(Script, MissingRequiredField) {
    std::string err = parse_err("[{\"op\": \"break\"}]");
    EXPECT_NE(err.find("step 1 (break): missing field 'addr'"), std::string::npos);
}

TEST(Script, InvalidAddress) {
    std::string err = parse_err("[{\"op\": \"break\", \"addr\": \"xyz\"}]");
    EXPECT_NE(err.find("step 1 (break): invalid addr 'xyz'"), std::string::npos);
}

TEST(Script, InvalidHexBytes) {
    std::string err = parse_err("[{\"op\": \"write\", \"addr\": \"0x1000\", \"bytes\": \"GG\"}]");
    EXPECT_NE(err.find("invalid bytes"), std::string::npos);
}

TEST(Script, InvalidValueType) {
    std::string err = parse_err(
        "[{\"op\": \"watch_add\", \"desc\": \"x\", \"addr\": \"0x1000\", \"type\": \"int\"}]");
    EXPECT_NE(err.find("invalid type"), std::string::npos);
}

TEST(Script, InvalidHwLength) {
    std::string err = parse_err(
        "[{\"op\": \"hbreak\", \"addr\": \"0x1000\", \"type\": \"0\", \"length\": \"3\"}]");
    EXPECT_NE(err.find("invalid length"), std::string::npos);
}

TEST(Script, InvalidNumber) {
    std::string err =
        parse_err("[{\"op\": \"continue\", \"timeout_ms\": \"abc\"}]");
    EXPECT_NE(err.find("invalid timeout_ms"), std::string::npos);
}

TEST(Script, StepIndexInError) {
    std::string err = parse_err(
        "[{\"op\": \"break\", \"addr\": \"0x1000\"},\n"
        " {\"op\": \"status\"},\n"
        " {\"op\": \"step\", \"tid\": \"oops\"}]");
    EXPECT_NE(err.find("step 3 (step): invalid tid 'oops'"), std::string::npos);
}

// ---- hex bytes with spaces ----

TEST(Script, HexBytesSpaced) {
    std::vector<script::Step> steps;
    ASSERT_TRUE(parse_ok(
        "[{\"op\": \"write\", \"addr\": \"0x1000\", \"bytes\": \"DE AD BE EF\"}]", steps));
    EXPECT_EQ(steps[0].fields.at("bytes"), "DE AD BE EF");
}

// ---- file ----

TEST(Script, MissingFile) {
    std::vector<script::Step> steps;
    std::string err;
    EXPECT_FALSE(script::parse_file("nonexistent_script.json", steps, err));
    EXPECT_NE(err.find("cannot open script file"), std::string::npos);
}

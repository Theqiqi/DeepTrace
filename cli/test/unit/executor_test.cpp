#include "interface/cmd.h"
#include "interface/executor.h"

#include "command/parser.h"
#include "printing/printer.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <vector>

using namespace deeptrace_cli;

namespace {

ParseResult parse(const std::vector<const char*>& argv) {
    std::vector<char*> args;
    for (auto* a : argv) args.push_back(const_cast<char*>(a));
    return parse_args(static_cast<int>(args.size()), args.data());
}

}  // namespace

// ---- internal conversions ----

TEST(Internal, ToU64) {
    EXPECT_EQ(internal::to_u64("0x1000"), 0x1000ULL);
    EXPECT_EQ(internal::to_u64("4096"), 4096ULL);
    EXPECT_EQ(internal::to_u64("0"), 0ULL);
}

TEST(Internal, ToU32) {
    EXPECT_EQ(internal::to_u32("1234"), 1234u);
}

TEST(Internal, ToAddr) {
    EXPECT_EQ(internal::to_addr("0x140001000"), 0x140001000ULL);
    EXPECT_EQ(internal::to_addr("100"), 100ULL);
}

TEST(Internal, HexBytes) {
    auto b = internal::hex_bytes("DEADBEEF");
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0xDE);
    EXPECT_EQ(b[3], 0xEF);
    auto p = internal::hex_bytes("0x4831C0");
    ASSERT_EQ(p.size(), 3u);
    EXPECT_EQ(p[0], 0x48);
}

TEST(Internal, ValueTypeId) {
    EXPECT_EQ(internal::value_type_id("byte"), 0);
    EXPECT_EQ(internal::value_type_id("word"), 1);
    EXPECT_EQ(internal::value_type_id("dword"), 2);
    EXPECT_EQ(internal::value_type_id("qword"), 3);
    EXPECT_EQ(internal::value_type_id("float"), 4);
    EXPECT_EQ(internal::value_type_id("double"), 5);
}

// ---- value_to_pattern (typed value -> AOB pattern string) ----

TEST(Internal, ValueToPatternIntLittleEndian) {
    EXPECT_EQ(internal::value_to_pattern("100", "dword"), "64 00 00 00");
    EXPECT_EQ(internal::value_to_pattern("0x11223344", "dword"),
              "44 33 22 11");
    EXPECT_EQ(internal::value_to_pattern("255", "byte"), "FF");
    EXPECT_EQ(internal::value_to_pattern("0x1234", "word"), "34 12");
    EXPECT_EQ(internal::value_to_pattern("0x1122334455667788", "qword"),
              "88 77 66 55 44 33 22 11");
    // decimal with leading zero must NOT be parsed as octal
    EXPECT_EQ(internal::value_to_pattern("08", "byte"), "08");
}

TEST(Internal, ValueToPatternFloat) {
    // 1.0f little-endian IEEE754 single
    EXPECT_EQ(internal::value_to_pattern("1.0", "float"), "00 00 80 3F");
    // 3.14159f as in the test target
    EXPECT_EQ(internal::value_to_pattern("3.14159", "float"), "D0 0F 49 40");
}

TEST(Internal, ValueToPatternDouble) {
    // 1.0 little-endian IEEE754 double
    EXPECT_EQ(internal::value_to_pattern("1.0", "double"),
              "00 00 00 00 00 00 F0 3F");
    // 2.71828 as in the test target
    EXPECT_EQ(internal::value_to_pattern("2.71828", "double"),
              "90 F7 AA 95 09 BF 05 40");
}

TEST(Internal, ValueToPatternString) {
    EXPECT_EQ(internal::value_to_pattern("hi", "string"), "68 69");
    EXPECT_EQ(internal::value_to_pattern("AB", "string"), "41 42");
}

TEST(Internal, ValueToPatternHex) {
    EXPECT_EQ(internal::value_to_pattern("DEADBEEF", "hex"),
              "DE AD BE EF");
    EXPECT_EQ(internal::value_to_pattern("0x4831C0", "hex"),
              "48 31 C0");
}

TEST(Internal, ValueToPatternPatternPassthrough) {
    EXPECT_EQ(internal::value_to_pattern("48 8B ?? ?? 00", "pattern"),
              "48 8B ?? ?? 00");
}

// ---- no-process error paths (execute without -p / without session) ----

TEST(Executor, MemReadWithoutSession) {
    auto pr = parse({"deeptrace_cli", "mem", "read", "0x1000", "4"});
    ASSERT_TRUE(pr.ok);
    // no -p given: no session -> deeptrace returns NotAttached -> exit 1
    EXPECT_EQ(execute(pr.req), 1);
}

TEST(Executor, ThreadListWithoutSession) {
    auto pr = parse({"deeptrace_cli", "thread", "list"});
    ASSERT_TRUE(pr.ok);
    EXPECT_EQ(execute(pr.req), 1);
}

TEST(Executor, ModuleBaseWithoutSession) {
    auto pr = parse({"deeptrace_cli", "module", "base", "kernel32.dll"});
    ASSERT_TRUE(pr.ok);
    EXPECT_EQ(execute(pr.req), 1);
}

TEST(Executor, PsAttachBadPid) {
    // pid 99999999 likely does not exist -> non-zero exit
    auto pr = parse({"deeptrace_cli", "ps", "attach", "99999999"});
    ASSERT_TRUE(pr.ok);
    EXPECT_EQ(execute(pr.req), 1);
}

TEST(Executor, PsListWithoutPid) {
    auto pr = parse({"deeptrace_cli", "ps", "list"});
    ASSERT_TRUE(pr.ok);
    EXPECT_EQ(execute(pr.req), 0);
}

TEST(Executor, AsmAssemble) {
    auto pr = parse({"deeptrace_cli", "asm", "assemble", "nop; nop; ret"});
    ASSERT_TRUE(pr.ok);
    EXPECT_EQ(execute(pr.req), 0);
}

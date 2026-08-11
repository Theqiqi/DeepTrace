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

// ---- convert: typed value -> bytes (little-endian) ----

TEST(TypedBytes, IntegerLittleEndian) {
    std::vector<uint8_t> b;
    EXPECT_TRUE(internal::typed_bytes("255", "byte", b));
    ASSERT_EQ(b.size(), 1u);
    EXPECT_EQ(b[0], 0xFF);
    EXPECT_TRUE(internal::typed_bytes("0x7F", "byte", b));
    ASSERT_EQ(b.size(), 1u);
    EXPECT_EQ(b[0], 0x7F);

    EXPECT_TRUE(internal::typed_bytes("0x0102", "word", b));
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0x02);
    EXPECT_EQ(b[1], 0x01);

    EXPECT_TRUE(internal::typed_bytes("287454020", "dword", b));  // 0x11223344
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x44);
    EXPECT_EQ(b[1], 0x33);
    EXPECT_EQ(b[2], 0x22);
    EXPECT_EQ(b[3], 0x11);

    EXPECT_TRUE(internal::typed_bytes("0x1122334455667788", "qword", b));
    ASSERT_EQ(b.size(), 8u);
    EXPECT_EQ(b[0], 0x88);
    EXPECT_EQ(b[1], 0x77);
    EXPECT_EQ(b[2], 0x66);
    EXPECT_EQ(b[3], 0x55);
    EXPECT_EQ(b[4], 0x44);
    EXPECT_EQ(b[5], 0x33);
    EXPECT_EQ(b[6], 0x22);
    EXPECT_EQ(b[7], 0x11);
}

TEST(TypedBytes, FloatDoubleIeee754) {
    std::vector<uint8_t> b;
    EXPECT_TRUE(internal::typed_bytes("1.0", "float", b));  // 0x3F800000 LE
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x00);
    EXPECT_EQ(b[1], 0x00);
    EXPECT_EQ(b[2], 0x80);
    EXPECT_EQ(b[3], 0x3F);

    EXPECT_TRUE(internal::typed_bytes("1.0", "double", b));  // 0x3FF0000000000000 LE
    ASSERT_EQ(b.size(), 8u);
    EXPECT_EQ(b[0], 0x00);
    EXPECT_EQ(b[1], 0x00);
    EXPECT_EQ(b[2], 0x00);
    EXPECT_EQ(b[3], 0x00);
    EXPECT_EQ(b[4], 0x00);
    EXPECT_EQ(b[5], 0x00);
    EXPECT_EQ(b[6], 0xF0);
    EXPECT_EQ(b[7], 0x3F);
}

TEST(TypedBytes, StringAscii) {
    std::vector<uint8_t> b;
    EXPECT_TRUE(internal::typed_bytes("hi", "string", b));
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0x68);
    EXPECT_EQ(b[1], 0x69);
}

TEST(TypedBytes, HexPassthrough) {
    std::vector<uint8_t> b;
    EXPECT_TRUE(internal::typed_bytes("DEADBEEF", "hex", b));
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0xDE);
    EXPECT_EQ(b[1], 0xAD);
    EXPECT_EQ(b[2], 0xBE);
    EXPECT_EQ(b[3], 0xEF);
    EXPECT_TRUE(internal::typed_bytes("0x4831C0", "hex", b));
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x31);
    EXPECT_EQ(b[2], 0xC0);
}

TEST(TypedBytes, UnknownTypeFails) {
    std::vector<uint8_t> b;
    EXPECT_FALSE(internal::typed_bytes("1", "pattern", b));
    EXPECT_FALSE(internal::typed_bytes("1", "bogus", b));
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

TEST(Executor, ConvertNoProcess) {
    // pure data conversion: no -p, no session, exits 0
    auto pr = parse({"deeptrace_cli", "convert", "dword", "100"});
    ASSERT_TRUE(pr.ok);
    EXPECT_EQ(execute(pr.req), 0);
    auto bad = parse({"deeptrace_cli", "convert", "dword", "xyz"});
    ASSERT_FALSE(bad.ok);
    EXPECT_EQ(bad.exit_code, 2);
}

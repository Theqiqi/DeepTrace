#include "command/commands.h"
#include "command/parser.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace deeptrace_cli;

namespace {

ParseResult parse(const std::vector<const char*>& argv) {
    // build argc/argv like main() would receive
    std::vector<char*> args;
    for (auto* a : argv) args.push_back(const_cast<char*>(a));
    return parse_args(static_cast<int>(args.size()), args.data());
}

ParseResult parse1(const char* a1) { return parse({"deeptrace_cli", a1}); }

}  // namespace

// ---- no args / help / version ----

TEST(Parser, NoArgs) {
    auto r = parse({"deeptrace_cli"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 1);
    EXPECT_NE(r.error.find("Missing command"), std::string::npos);
}

TEST(Parser, HelpShort) {
    auto r = parse({"deeptrace_cli", "-h"});
    EXPECT_TRUE(r.ok);
    EXPECT_TRUE(r.req.help);
}

TEST(Parser, HelpLong) {
    auto r = parse({"deeptrace_cli", "--help"});
    EXPECT_TRUE(r.ok);
    EXPECT_TRUE(r.req.help);
}

TEST(Parser, Version) {
    auto r = parse({"deeptrace_cli", "-v"});
    EXPECT_TRUE(r.ok);
    EXPECT_TRUE(r.req.version);
}

// ---- global option -p ----

TEST(Parser, PidShort) {
    auto r = parse({"deeptrace_cli", "-p", "1234", "ps", "list"});
    ASSERT_TRUE(r.ok);
    EXPECT_TRUE(r.req.pid_set);
    EXPECT_EQ(r.req.pid, 1234u);
    EXPECT_EQ(r.req.group, "ps");
    EXPECT_EQ(r.req.action, "list");
}

TEST(Parser, PidLong) {
    auto r = parse({"deeptrace_cli", "--pid", "999", "ps", "list"});
    ASSERT_TRUE(r.ok);
    EXPECT_TRUE(r.req.pid_set);
    EXPECT_EQ(r.req.pid, 999u);
}

TEST(Parser, PidAfterCommand) {
    auto r = parse({"deeptrace_cli", "ps", "list", "-p", "42"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.pid, 42u);
}

TEST(Parser, PidMissingValue) {
    auto r = parse({"deeptrace_cli", "-p"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
}

TEST(Parser, PidInvalid) {
    auto r = parse({"deeptrace_cli", "-p", "0xZZ", "ps", "list"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
    EXPECT_NE(r.error.find("invalid pid"), std::string::npos);
}

TEST(Parser, UnknownOption) {
    auto r = parse({"deeptrace_cli", "-x", "ps", "list"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
}

// ---- routing ----

TEST(Parser, UnknownGroup) {
    auto r = parse({"deeptrace_cli", "bogus", "list"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
    EXPECT_NE(r.error.find("unknown command group"), std::string::npos);
}

TEST(Parser, UnknownSubcommand) {
    auto r = parse({"deeptrace_cli", "ps", "frobnicate"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
    EXPECT_NE(r.error.find("unknown command"), std::string::npos);
}

TEST(Parser, MissingSubcommand) {
    auto r = parse({"deeptrace_cli", "ps"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
}

TEST(Parser, AllGroupsKnown) {
    const char* groups[] = {"ps", "mem", "module", "thread", "debug",
                            "disasm", "resolve", "watch", "dll", "asm",
                            "shellcode"};
    for (const char* g : groups) {
        EXPECT_TRUE(is_group(g)) << g;
    }
}

// ---- parameter validation ----

TEST(Parser, MissingRequiredArg) {
    auto r = parse({"deeptrace_cli", "mem", "read"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
    EXPECT_NE(r.error.find("missing argument: address"), std::string::npos);
}

TEST(Parser, InvalidAddress) {
    auto r = parse({"deeptrace_cli", "mem", "read", "not_an_addr"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
    EXPECT_NE(r.error.find("invalid address"), std::string::npos);
}

TEST(Parser, AddressHexAndDec) {
    auto r = parse({"deeptrace_cli", "mem", "read", "0x140001000"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args.size(), 3u);  // addr, size=1, format=hex
    EXPECT_EQ(r.req.args[0], "0x140001000");
    EXPECT_EQ(r.req.args[1], "1");
    EXPECT_EQ(r.req.args[2], "hex");
}

TEST(Parser, MemReadDefaults) {
    auto r = parse({"deeptrace_cli", "mem", "read", "5368709120"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[0], "5368709120");
    EXPECT_EQ(r.req.args[1], "1");
    EXPECT_EQ(r.req.args[2], "hex");
}

TEST(Parser, MemReadExplicitArgs) {
    auto r = parse({"deeptrace_cli", "mem", "read", "0x1000", "16", "dec"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[1], "16");
    EXPECT_EQ(r.req.args[2], "dec");
}

TEST(Parser, MemReadBadFormat) {
    auto r = parse({"deeptrace_cli", "mem", "read", "0x1000", "4", "octal"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
}

TEST(Parser, MemWriteHexBytes) {
    auto r = parse({"deeptrace_cli", "mem", "write", "0x1000", "DEADBEEF"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[1], "DEADBEEF");
    EXPECT_EQ(r.req.args[2], "hex");
}

TEST(Parser, MemWriteOddHex) {
    auto r = parse({"deeptrace_cli", "mem", "write", "0x1000", "ABC"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
}

TEST(Parser, MemWriteBadChar) {
    auto r = parse({"deeptrace_cli", "mem", "write", "0x1000", "DEADGG"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
}

TEST(Parser, MemReadvalType) {
    auto r = parse({"deeptrace_cli", "mem", "readval", "0x1000", "qword"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[1], "qword");
    auto bad = parse({"deeptrace_cli", "mem", "readval", "0x1000", "int"});
    EXPECT_FALSE(bad.ok);
}

TEST(Parser, PsAttachPid) {
    auto r = parse({"deeptrace_cli", "ps", "attach", "1234"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args.size(), 1u);
    EXPECT_EQ(r.req.args[0], "1234");
    auto bad = parse({"deeptrace_cli", "ps", "attach", "0"});
    EXPECT_FALSE(bad.ok);
    auto bad2 = parse({"deeptrace_cli", "ps", "attach", "1.5"});
    EXPECT_FALSE(bad2.ok);
}

TEST(Parser, PsKillDefaultExitCode) {
    auto r = parse({"deeptrace_cli", "ps", "kill"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args.size(), 1u);
    EXPECT_EQ(r.req.args[0], "0");
}

TEST(Parser, DebugStepDefaultTid) {
    auto r = parse({"deeptrace_cli", "debug", "step"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args.size(), 1u);
    EXPECT_EQ(r.req.args[0], "0");
    auto r2 = parse({"deeptrace_cli", "debug", "step", "77"});
    ASSERT_TRUE(r2.ok);
    EXPECT_EQ(r2.req.args[0], "77");
}

TEST(Parser, DebugHbreakDefaults) {
    auto r = parse({"deeptrace_cli", "debug", "hbreak", "0x1000"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args.size(), 3u);
    EXPECT_EQ(r.req.args[1], "0");
    EXPECT_EQ(r.req.args[2], "1");
}

TEST(Parser, HwTypeValid) {
    auto r = parse({"deeptrace_cli", "debug", "hbreak", "0x1000", "2", "8"});
    ASSERT_TRUE(r.ok);
    auto bad = parse({"deeptrace_cli", "debug", "hbreak", "0x1000", "9", "1"});
    EXPECT_FALSE(bad.ok);
}

TEST(Parser, DisasmAtDefaultCount) {
    auto r = parse({"deeptrace_cli", "disasm", "at", "0x1000"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[1], "10");
}

TEST(Parser, PatternValid) {
    auto r = parse({"deeptrace_cli", "resolve", "scan", "48 8B ?? ?? 00"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args.size(), 2u);      // value + type(default pattern)
    EXPECT_EQ(r.req.args[1], "pattern");  // backward-compatible default
    auto bad = parse({"deeptrace_cli", "resolve", "scan", "48 Z"});
    EXPECT_FALSE(bad.ok);
    EXPECT_EQ(bad.exit_code, 2);
    auto empty = parse({"deeptrace_cli", "resolve", "scan", ""});
    EXPECT_FALSE(empty.ok);
}

// ---- resolve scan typed values ----

TEST(Parser, ScanTypedValueInt) {
    auto r = parse({"deeptrace_cli", "resolve", "scan", "100", "dword"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[0], "100");
    EXPECT_EQ(r.req.args[1], "dword");
    auto hex = parse({"deeptrace_cli", "resolve", "scan", "0x11223344", "dword"});
    ASSERT_TRUE(hex.ok);
    auto q = parse({"deeptrace_cli", "resolve", "scan", "18446744073709551615", "qword"});
    ASSERT_TRUE(q.ok);
}

TEST(Parser, ScanTypedValueRange) {
    auto ok = parse({"deeptrace_cli", "resolve", "scan", "255", "byte"});
    ASSERT_TRUE(ok.ok);
    auto over = parse({"deeptrace_cli", "resolve", "scan", "256", "byte"});
    EXPECT_FALSE(over.ok);
    EXPECT_EQ(over.exit_code, 2);
    auto over_dw = parse({"deeptrace_cli", "resolve", "scan", "4294967296", "dword"});
    EXPECT_FALSE(over_dw.ok);
    auto neg = parse({"deeptrace_cli", "resolve", "scan", "-1", "dword"});
    EXPECT_FALSE(neg.ok);
    EXPECT_EQ(neg.exit_code, 2);
}

TEST(Parser, ScanTypedValueFloat) {
    auto r = parse({"deeptrace_cli", "resolve", "scan", "3.14", "float"});
    ASSERT_TRUE(r.ok);
    auto sci = parse({"deeptrace_cli", "resolve", "scan", "1e5", "double"});
    ASSERT_TRUE(sci.ok);
    auto bad = parse({"deeptrace_cli", "resolve", "scan", "abc", "float"});
    EXPECT_FALSE(bad.ok);
    EXPECT_EQ(bad.exit_code, 2);
    auto hexf = parse({"deeptrace_cli", "resolve", "scan", "0x10", "float"});
    EXPECT_FALSE(hexf.ok);  // hex float literals not supported
    auto inf = parse({"deeptrace_cli", "resolve", "scan", "1e400", "float"});
    EXPECT_FALSE(inf.ok);   // overflows to inf -> not finite
}

TEST(Parser, ScanTypedValueString) {
    auto r = parse({"deeptrace_cli", "resolve", "scan", "hello", "string"});
    ASSERT_TRUE(r.ok);
    auto bad = parse({"deeptrace_cli", "resolve", "scan", "h\x01i", "string"});
    EXPECT_FALSE(bad.ok);  // non-printable ASCII rejected
}

TEST(Parser, ScanTypedValueHex) {
    auto r = parse({"deeptrace_cli", "resolve", "scan", "DEADBEEF", "hex"});
    ASSERT_TRUE(r.ok);
    auto pfx = parse({"deeptrace_cli", "resolve", "scan", "0xDEADBEEF", "hex"});
    ASSERT_TRUE(pfx.ok);
    auto odd = parse({"deeptrace_cli", "resolve", "scan", "ABC", "hex"});
    EXPECT_FALSE(odd.ok);
    EXPECT_EQ(odd.exit_code, 2);
}

TEST(Parser, ScanTypedValueBadType) {
    auto r = parse({"deeptrace_cli", "resolve", "scan", "100", "bogus"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
    EXPECT_NE(r.error.find("invalid type"), std::string::npos);
}

TEST(Parser, ScanTypedValueErrorMsg) {
    auto r = parse({"deeptrace_cli", "resolve", "scan", "xyz", "dword"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
    EXPECT_NE(r.error.find("invalid value for type 'dword'"), std::string::npos);
}

TEST(Parser, TooManyArguments) {
    auto r = parse({"deeptrace_cli", "ps", "list", "extra"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
    EXPECT_NE(r.error.find("too many arguments"), std::string::npos);
}

TEST(Parser, AsmAssembleFlags) {
    auto r = parse({"deeptrace_cli", "asm", "assemble", "mov rax, 1", "--hex"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[0], "mov rax, 1");
    EXPECT_EQ(r.req.args[1], "--hex");
    auto r2 = parse({"deeptrace_cli", "asm", "assemble", "nop", "--c-array"});
    ASSERT_TRUE(r2.ok);
    EXPECT_EQ(r2.req.args[1], "");
    EXPECT_EQ(r2.req.args[2], "--c-array");
}

TEST(Parser, ShellcodeHexBytes) {
    auto r = parse({"deeptrace_cli", "shellcode", "inject", "4831C0C3"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[0], "4831C0C3");
    auto bad = parse({"deeptrace_cli", "shellcode", "inject", "4831C0C"});
    EXPECT_FALSE(bad.ok);
}

TEST(Parser, WatchAddTypes) {
    auto r = parse({"deeptrace_cli", "watch", "add", "mywatch", "0x1000", "float"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[2], "float");
    auto bad = parse({"deeptrace_cli", "watch", "add", "mywatch", "0x1000", "bogus"});
    EXPECT_FALSE(bad.ok);
}

TEST(Parser, ModuleDumpDefaultFile) {
    auto r = parse({"deeptrace_cli", "module", "dump", "kernel32.dll"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[1], "");
}

TEST(Parser, HelpTextIsAscii) {
    std::string help = build_help_text();
    for (char c : help) {
        // only printable ASCII or whitespace (\n, space, \t) allowed
        unsigned char uc = static_cast<unsigned char>(c);
        bool ws = (c == '\n' || c == '\t' || c == ' ' || c == '\r');
        bool printable = uc >= 0x20 && uc <= 0x7E;
        EXPECT_TRUE(ws || printable) << "non-ascii byte 0x" << std::hex << (int)uc;
    }
    EXPECT_NE(help.find("deeptrace_cli"), std::string::npos);
    EXPECT_NE(help.find("mem read"), std::string::npos);
    EXPECT_NE(help.find("shellcode inject"), std::string::npos);
}

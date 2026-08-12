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
                            "shellcode", "convert", "hex2bin"};
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

// v2.6.0: address args accept a numeric address OR a script symbol name
// (symbol addressing). Symbol-shaped tokens now parse; only tokens that are
// neither a valid number nor a valid symbol shape are rejected at parse time.
TEST(Parser, InvalidAddress) {
    auto bad = parse({"deeptrace_cli", "mem", "read", "foo bar"});  // space
    EXPECT_FALSE(bad.ok);
    EXPECT_EQ(bad.exit_code, 2);
    EXPECT_NE(bad.error.find("invalid address"), std::string::npos);
    auto bad2 = parse({"deeptrace_cli", "mem", "read", "a-b"});  // '-' not identifier
    EXPECT_FALSE(bad2.ok);
    EXPECT_EQ(bad2.exit_code, 2);
    auto bad3 = parse({"deeptrace_cli", "mem", "read", "0xZZ"});  // neither number nor shape
    EXPECT_FALSE(bad3.ok);
    EXPECT_EQ(bad3.exit_code, 2);
}

TEST(Parser, AddressAcceptsSymbolShape) {
    // v2.6.0 symbol addressing: symbol-shaped tokens pass the parser; actual
    // existence is resolved later by the interface layer (requires attach).
    auto r = parse({"deeptrace_cli", "mem", "read", "sunObjPtr"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[0], "sunObjPtr");
    auto w = parse({"deeptrace_cli", "watch", "add", "desc", "slotA", "qword"});
    ASSERT_TRUE(w.ok);
    EXPECT_EQ(w.req.args[1], "slotA");
    auto d = parse({"deeptrace_cli", "disasm", "at", "code_newmem"});
    ASSERT_TRUE(d.ok);
    EXPECT_EQ(d.req.args[0], "code_newmem");
    auto s = parse({"deeptrace_cli", "shellcode", "run", "slotB"});
    ASSERT_TRUE(s.ok);
    EXPECT_EQ(s.req.args[0], "slotB");
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

TEST(Parser, DebugRunScriptPath) {
    auto r = parse({"deeptrace_cli", "-p", "1234", "debug", "run", "demo.json"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.group, "debug");
    EXPECT_EQ(r.req.action, "run");
    EXPECT_EQ(r.req.args.size(), 1u);
    EXPECT_EQ(r.req.args[0], "demo.json");
    auto missing = parse({"deeptrace_cli", "debug", "run"});
    EXPECT_FALSE(missing.ok);
    EXPECT_EQ(missing.exit_code, 2);
    EXPECT_NE(missing.error.find("missing argument: script"), std::string::npos);
}

// v2.1.0: debug group has a single entry `debug run`; all standalone debug
// commands (step/break/registers/attach/...) were removed and must be rejected
// with exit 2. Their capabilities live in script steps only.
TEST(Parser, DebugSingleCommandsRejected) {
    const char* removed[] = {"attach", "detach", "pause", "resume", "step",
                             "next", "break", "clear", "hbreak", "hclear",
                             "guard", "unguard", "status", "registers",
                             "register"};
    for (const char* a : removed) {
        auto r = parse({"deeptrace_cli", "debug", a});
        EXPECT_FALSE(r.ok) << a;
        EXPECT_EQ(r.exit_code, 2) << a;
        EXPECT_NE(r.error.find("unknown command"), std::string::npos) << a;
    }
}

TEST(Parser, DisasmAtDefaultCount) {
    auto r = parse({"deeptrace_cli", "disasm", "at", "0x1000"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[1], "10");
}

TEST(Parser, PatternValid) {
    auto r = parse({"deeptrace_cli", "resolve", "scan", "48 8B ?? ?? 00"});
    ASSERT_TRUE(r.ok);
    auto bad = parse({"deeptrace_cli", "resolve", "scan", "48 Z"});
    EXPECT_FALSE(bad.ok);
    auto empty = parse({"deeptrace_cli", "resolve", "scan", ""});
    EXPECT_FALSE(empty.ok);
}

// resolve scan is pattern-only again (v1.4.0 typed-value change was reverted).
TEST(Parser, ScanTypedValueRejected) {
    // a non-pattern token fails pattern validation first
    auto r = parse({"deeptrace_cli", "resolve", "scan", "100", "dword"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
    EXPECT_NE(r.error.find("invalid pattern"), std::string::npos);
    // a valid pattern with an extra arg hits the too-many-arguments path
    auto r2 = parse({"deeptrace_cli", "resolve", "scan", "48 8B", "dword"});
    EXPECT_FALSE(r2.ok);
    EXPECT_EQ(r2.exit_code, 2);
    EXPECT_NE(r2.error.find("too many arguments"), std::string::npos);
}

// ---- convert (standalone command, type first) ----

TEST(Parser, ConvertParsesTypeFirst) {
    auto r = parse({"deeptrace_cli", "convert", "dword", "100"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.group, "convert");
    EXPECT_EQ(r.req.action, "");  // standalone command, no sub-action
    EXPECT_EQ(r.req.args.size(), 2u);
    EXPECT_EQ(r.req.args[0], "dword");  // type first
    EXPECT_EQ(r.req.args[1], "100");
}

TEST(Parser, ConvertAllTypes) {
    const char* types[] = {"byte", "word", "dword", "qword", "float", "double",
                           "string", "hex"};
    // "1" is valid for every type except hex (odd length); hex needs "11".
    const char* vals[] = {"1", "1", "1", "1", "1", "1", "1", "11"};
    for (size_t i = 0; i < 8; ++i) {
        auto r = parse({"deeptrace_cli", "convert", types[i], vals[i]});
        ASSERT_TRUE(r.ok) << types[i];
        EXPECT_EQ(r.req.args[0], types[i]) << types[i];
    }
}

TEST(Parser, ConvertInvalidType) {
    auto r = parse({"deeptrace_cli", "convert", "bogus", "1"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
    EXPECT_NE(r.error.find("invalid type"), std::string::npos);
    auto r2 = parse({"deeptrace_cli", "convert", "pattern", "48 8B"});
    EXPECT_FALSE(r2.ok);  // AOB wildcards are scan-only, not a convert type
}

TEST(Parser, ConvertMissingArgs) {
    auto r = parse({"deeptrace_cli", "convert"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
    EXPECT_NE(r.error.find("missing argument: type"), std::string::npos);
    auto r2 = parse({"deeptrace_cli", "convert", "dword"});
    EXPECT_FALSE(r2.ok);
    EXPECT_EQ(r2.exit_code, 2);
    EXPECT_NE(r2.error.find("missing argument: value"), std::string::npos);
}

TEST(Parser, ConvertTooManyArgs) {
    auto r = parse({"deeptrace_cli", "convert", "dword", "1", "extra"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
}

TEST(Parser, ConvertIntRanges) {
    auto ok = [&](const char* t, const char* v) {
        auto r = parse({"deeptrace_cli", "convert", t, v});
        EXPECT_TRUE(r.ok) << t << " " << v;
    };
    auto bad = [&](const char* t, const char* v) {
        auto r = parse({"deeptrace_cli", "convert", t, v});
        EXPECT_FALSE(r.ok) << t << " " << v;
        EXPECT_EQ(r.exit_code, 2);
        EXPECT_NE(r.error.find("invalid value for type"), std::string::npos);
    };
    ok("byte", "0");
    ok("byte", "255");
    ok("byte", "0xFF");
    bad("byte", "256");
    ok("word", "65535");
    bad("word", "65536");
    ok("dword", "4294967295");
    bad("dword", "4294967296");
    ok("dword", "0x11223344");
    ok("qword", "18446744073709551615");
    bad("qword", "18446744073709551616");
    bad("dword", "xyz");
}

TEST(Parser, ConvertNegativeRejectedByOptionScanner) {
    // "-1" starts with '-', so the option scanner rejects it as an unknown
    // option before value validation (negatives are unsupported either way).
    auto r = parse({"deeptrace_cli", "convert", "dword", "-1"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
    EXPECT_NE(r.error.find("unknown option"), std::string::npos);
}

TEST(Parser, ConvertNoOctalTrap) {
    // "08" must parse as decimal 8, not invalid octal
    auto r = parse({"deeptrace_cli", "convert", "byte", "08"});
    ASSERT_TRUE(r.ok);
    auto r2 = parse({"deeptrace_cli", "convert", "byte", "010"});
    ASSERT_TRUE(r2.ok);  // decimal 10, not octal 8
}

TEST(Parser, ConvertFloatDouble) {
    auto r = parse({"deeptrace_cli", "convert", "float", "3.14"});
    ASSERT_TRUE(r.ok);
    auto r2 = parse({"deeptrace_cli", "convert", "double", "2.71828"});
    ASSERT_TRUE(r2.ok);
    auto bad = parse({"deeptrace_cli", "convert", "float", "0x1p3"});  // hex float
    EXPECT_FALSE(bad.ok);
    auto bad2 = parse({"deeptrace_cli", "convert", "float", "nan"});
    EXPECT_FALSE(bad2.ok);
    auto bad3 = parse({"deeptrace_cli", "convert", "float", "abc"});
    EXPECT_FALSE(bad3.ok);
}

TEST(Parser, ConvertStringAscii) {
    auto r = parse({"deeptrace_cli", "convert", "string", "hello"});
    ASSERT_TRUE(r.ok);
    auto bad = parse({"deeptrace_cli", "convert", "string", ""});
    EXPECT_FALSE(bad.ok);
    // non-printable ASCII is rejected
    std::string s = "a";
    s += static_cast<char>(1);
    auto bad2 = parse({"deeptrace_cli", "convert", "string", s.c_str()});
    EXPECT_FALSE(bad2.ok);
}

TEST(Parser, ConvertHexBytes) {
    auto r = parse({"deeptrace_cli", "convert", "hex", "DEADBEEF"});
    ASSERT_TRUE(r.ok);
    auto r2 = parse({"deeptrace_cli", "convert", "hex", "0x4831C0"});
    ASSERT_TRUE(r2.ok);
    auto bad = parse({"deeptrace_cli", "convert", "hex", "ABC"});  // odd length
    EXPECT_FALSE(bad.ok);
    auto bad2 = parse({"deeptrace_cli", "convert", "hex", "DEADGG"});
    EXPECT_FALSE(bad2.ok);
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

// ---- v2.2.0: asm file / hex2bin / shellcode staged ops ----

TEST(Parser, AsmFileBasic) {
    auto r = parse({"deeptrace_cli", "asm", "file", "code.asm"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.group, "asm");
    EXPECT_EQ(r.req.action, "file");
    EXPECT_EQ(r.req.args[0], "code.asm");
    EXPECT_EQ(r.req.args[1], "");  // --hex unset
    EXPECT_EQ(r.req.args[2], "");  // --c-array unset
    EXPECT_EQ(r.req.args[3], "");  // --out unset
    EXPECT_EQ(r.req.args[4], "");  // --out value unset
}

TEST(Parser, AsmFileWithFlags) {
    auto r = parse({"deeptrace_cli", "asm", "file", "code.asm", "--hex"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[1], "--hex");
    auto r2 = parse({"deeptrace_cli", "asm", "file", "code.asm", "--c-array"});
    ASSERT_TRUE(r2.ok);
    EXPECT_EQ(r2.req.args[2], "--c-array");
}

TEST(Parser, AsmFileOutFlag) {
    auto r = parse({"deeptrace_cli", "asm", "file", "code.asm", "--out", "code.bin"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[3], "--out");
    EXPECT_EQ(r.req.args[4], "code.bin");
    auto missing = parse({"deeptrace_cli", "asm", "file", "code.asm", "--out"});
    EXPECT_FALSE(missing.ok);
    EXPECT_EQ(missing.exit_code, 2);
    EXPECT_NE(missing.error.find("missing argument for option: --out"),
              std::string::npos);
}

TEST(Parser, AsmFileMissingPath) {
    auto r = parse({"deeptrace_cli", "asm", "file"});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
    EXPECT_NE(r.error.find("missing argument: path"), std::string::npos);
}

TEST(Parser, Hex2BinParses) {
    auto r = parse({"deeptrace_cli", "hex2bin", "DEADBEEF", "out.bin"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.group, "hex2bin");
    EXPECT_EQ(r.req.action, "");  // standalone command
    EXPECT_EQ(r.req.args[0], "DEADBEEF");
    EXPECT_EQ(r.req.args[1], "out.bin");
    auto bad = parse({"deeptrace_cli", "hex2bin", "ABC", "out.bin"});  // odd
    EXPECT_FALSE(bad.ok);
    EXPECT_EQ(bad.exit_code, 2);
    auto bad2 = parse({"deeptrace_cli", "hex2bin", "DEADGG", "out.bin"});
    EXPECT_FALSE(bad2.ok);
    auto missing = parse({"deeptrace_cli", "hex2bin", "DEADBEEF"});
    EXPECT_FALSE(missing.ok);
    EXPECT_EQ(missing.exit_code, 2);
}

TEST(Parser, ShellcodeInjectFile) {
    auto r = parse({"deeptrace_cli", "shellcode", "injectfile", "code.bin"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.args[0], "code.bin");
    auto missing = parse({"deeptrace_cli", "shellcode", "injectfile"});
    EXPECT_FALSE(missing.ok);
}

TEST(Parser, ShellcodeAllocRunFreeExec) {
    auto alloc = parse({"deeptrace_cli", "shellcode", "alloc", "4831C0C3"});
    ASSERT_TRUE(alloc.ok);
    EXPECT_EQ(alloc.req.args[0], "4831C0C3");
    auto run = parse({"deeptrace_cli", "shellcode", "run", "0x1000"});
    ASSERT_TRUE(run.ok);
    EXPECT_EQ(run.req.args[0], "0x1000");
    auto free = parse({"deeptrace_cli", "shellcode", "free", "0x1000"});
    ASSERT_TRUE(free.ok);
    auto exec = parse({"deeptrace_cli", "shellcode", "exec", "code.bin"});
    ASSERT_TRUE(exec.ok);
    EXPECT_EQ(exec.req.args[0], "code.bin");
    // address validation: must be a valid address or symbol shape (v2.6.0);
    // a shape with a space is rejected at parse time
    auto bad = parse({"deeptrace_cli", "shellcode", "run", "foo bar"});
    EXPECT_FALSE(bad.ok);
    EXPECT_EQ(bad.exit_code, 2);
}

TEST(Parser, ShellcodeSourceNonEmpty) {
    auto r = parse({"deeptrace_cli", "shellcode", "alloc", ""});
    EXPECT_FALSE(r.ok);
    EXPECT_EQ(r.exit_code, 2);
    auto r2 = parse({"deeptrace_cli", "shellcode", "exec"});
    EXPECT_FALSE(r2.ok);
    EXPECT_EQ(r2.exit_code, 2);
}

TEST(Parser, HelpTextListsNewCommands) {
    std::string help = build_help_text();
    EXPECT_NE(help.find("asm file"), std::string::npos);
    EXPECT_NE(help.find("hex2bin"), std::string::npos);
    EXPECT_NE(help.find("shellcode alloc"), std::string::npos);
    EXPECT_NE(help.find("shellcode run"), std::string::npos);
    EXPECT_NE(help.find("shellcode free"), std::string::npos);
    EXPECT_NE(help.find("shellcode exec"), std::string::npos);
    EXPECT_NE(help.find("shellcode injectfile"), std::string::npos);
    EXPECT_NE(help.find("script check"), std::string::npos);
    EXPECT_NE(help.find("script run"), std::string::npos);
    EXPECT_NE(help.find("script disable"), std::string::npos);
    EXPECT_NE(help.find("script status"), std::string::npos);
    EXPECT_NE(help.find("deeptrace_cli v2.6.0"), std::string::npos);
}

TEST(Parser, ScriptCheckParses) {
    auto r = parse({"deeptrace_cli", "script", "check", "x.aa"});
    ASSERT_TRUE(r.ok);
    EXPECT_EQ(r.req.group, "script");
    EXPECT_EQ(r.req.action, "check");
    EXPECT_EQ(r.req.args.size(), 1u);
    EXPECT_EQ(r.req.args[0], "x.aa");
    auto missing = parse({"deeptrace_cli", "script", "check"});
    EXPECT_FALSE(missing.ok);
    EXPECT_EQ(missing.exit_code, 2);
    EXPECT_NE(missing.error.find("missing argument: file"), std::string::npos);
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

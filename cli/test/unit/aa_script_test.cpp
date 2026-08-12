#include "interface/cmd.h"

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace deeptrace_cli;
using internal::aa::Step;
using internal::aa::StepKind;

namespace {

bool parse_ok(const std::string& text, std::vector<Step>& enable,
              std::vector<Step>& disable) {
    std::string err;
    if (!internal::aa::aa_parse_text(text, enable, disable, err)) {
        ADD_FAILURE() << "parse failed: " << err;
        return false;
    }
    return true;
}

std::string parse_err(const std::string& text) {
    std::vector<Step> enable, disable;
    std::string err;
    if (internal::aa::aa_parse_text(text, enable, disable, err)) return "";
    return err;
}

}  // namespace

// ---- happy path: user-style script (CE AA hook example) ----

TEST(AaScript, HookEnableExample) {
    const char* text =
        "//code from here to '[DISABLE]' will be used to enable the cheat\n"
        "[ENABLE]\n"
        "alloc(newmem,2048,\"GameAssembly.dll\"+7D5778) \n"
        "label(returnhere)\n"
        "label(originalcode)\n"
        "label(exit)\n"
        "alloc(addsunrcx, 8)\n"
        "registersymbol(addsunrcx)\n"
        "\n"
        "newmem: //this is allocated memory, you have read,write,execute access\n"
        "mov [addsunrcx],rcx\n"
        "\n"
        "originalcode:\n"
        "xor r9d,r9d\n"
        "cvttss2si r8d,[rsp+44]\n"
        "\n"
        "exit:\n"
        "jmp returnhere\n"
        "\n"
        "\"GameAssembly.dll\"+7D5778:\n"
        "jmp newmem\n"
        "nop 5\n"
        "returnhere:\n"
        "\n"
        "[DISABLE]\n"
        "dealloc(newmem)\n"
        "unregistersymbol(addsunrcx)\n"
        "dealloc(addsunrcx)\n"
        "\n"
        "\"GameAssembly.dll\"+7D5778:\n"
        "db 45 33 C9 F3 44 0F 2C 44 24 44\n"
        "//xor r9d,r9d\n"
        "//cvttss2si r8d,[rsp+44]\n";
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(text, enable, disable));

    // ENABLE steps: alloc x2, label x3, registersymbol, newmem:/originalcode:/exit:
    // labels, asm x4, hook target, jmp, nop 5, returnhere: label
    ASSERT_EQ(enable.size(), 17u);
    EXPECT_EQ(enable[0].kind, StepKind::Alloc);
    EXPECT_EQ(enable[0].name, "newmem");
    EXPECT_EQ(enable[0].size, 2048u);
    // near expression accepted as placement hint
    EXPECT_EQ(enable[0].text, "\"GameAssembly.dll\"+7D5778");
    EXPECT_EQ(enable[1].kind, StepKind::LabelDecl);
    EXPECT_EQ(enable[1].name, "returnhere");
    EXPECT_EQ(enable[2].kind, StepKind::LabelDecl);
    EXPECT_EQ(enable[3].kind, StepKind::LabelDecl);
    EXPECT_EQ(enable[4].kind, StepKind::Alloc);
    EXPECT_EQ(enable[4].name, "addsunrcx");
    EXPECT_EQ(enable[5].kind, StepKind::RegisterSymbol);
    EXPECT_EQ(enable[6].kind, StepKind::LabelDef);
    EXPECT_EQ(enable[6].name, "newmem");
    EXPECT_EQ(enable[7].kind, StepKind::Asm);
    EXPECT_EQ(enable[7].text, "mov [addsunrcx],rcx");
    EXPECT_EQ(enable[8].kind, StepKind::LabelDef);  // originalcode:
    EXPECT_EQ(enable[9].kind, StepKind::Asm);       // xor r9d,r9d
    EXPECT_EQ(enable[10].kind, StepKind::Asm);      // cvttss2si
    EXPECT_EQ(enable[11].kind, StepKind::LabelDef); // exit:
    EXPECT_EQ(enable[12].kind, StepKind::Asm);      // jmp returnhere
    EXPECT_EQ(enable[13].kind, StepKind::HookTarget);
    EXPECT_EQ(enable[13].module, "GameAssembly.dll");
    EXPECT_EQ(enable[13].offset, 0x7D5778u);
    EXPECT_EQ(enable[14].kind, StepKind::Asm);
    EXPECT_EQ(enable[14].text, "jmp newmem");
    EXPECT_EQ(enable[15].kind, StepKind::NopFill);
    EXPECT_EQ(enable[15].size, 5u);
    EXPECT_EQ(enable[16].kind, StepKind::LabelDef); // returnhere:

    // DISABLE: dealloc, unregistersymbol, dealloc, hook target, db
    ASSERT_EQ(disable.size(), 5u);
    EXPECT_EQ(disable[0].kind, StepKind::Dealloc);
    EXPECT_EQ(disable[0].name, "newmem");
    EXPECT_EQ(disable[1].kind, StepKind::UnregisterSymbol);
    EXPECT_EQ(disable[2].kind, StepKind::Dealloc);
    EXPECT_EQ(disable[2].name, "addsunrcx");
    EXPECT_EQ(disable[3].kind, StepKind::HookTarget);
    EXPECT_EQ(disable[3].module, "GameAssembly.dll");
    EXPECT_EQ(disable[4].kind, StepKind::Db);
}

TEST(AaScript, CreateThreadExample) {
    const char* text =
        "[ENABLE]\n"
        "alloc(newmem,1024)\n"
        "createThread(newmem)\n"
        "\n"
        "newmem:\n"
        "    sub rsp,28\n"
        "    mov rax,addsunrcx\n"
        "    mov rcx,[rax]\n"
        "    mov rdx,0000000000000019\n"
        "    mov r12,\"GameAssembly.dll\"+63B9C0\n"
        "    call r12\n"
        "    add rsp,28\n"
        "    ret\n"
        "\n"
        "[DISABLE]\n"
        "dealloc(newmem)\n";
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(text, enable, disable));
    ASSERT_EQ(enable.size(), 11u);
    EXPECT_EQ(enable[0].kind, StepKind::Alloc);
    EXPECT_EQ(enable[1].kind, StepKind::CreateThread);
    EXPECT_EQ(enable[1].name, "newmem");
    EXPECT_EQ(enable[2].kind, StepKind::LabelDef);
    EXPECT_EQ(enable[2].name, "newmem");
    EXPECT_EQ(enable[3].kind, StepKind::Asm);
    EXPECT_EQ(enable[3].text, "sub rsp,28");
    EXPECT_EQ(enable[10].kind, StepKind::Asm);
    EXPECT_EQ(enable[10].text, "ret");
    ASSERT_EQ(disable.size(), 1u);
    EXPECT_EQ(disable[0].kind, StepKind::Dealloc);
}

// ---- parse errors ----

TEST(AaScript, AllocNearAccepted) {
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok("[ENABLE]\nalloc(a, 2048, 0x140000000)\n", enable, disable));
    ASSERT_EQ(enable.size(), 1u);
    EXPECT_EQ(enable[0].kind, StepKind::Alloc);
    EXPECT_EQ(enable[0].size, 2048u);
    EXPECT_EQ(enable[0].text, "0x140000000");
}

TEST(AaScript, AllocNearModuleForm) {
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok("[ENABLE]\nalloc(a, 64, \"game.dll\"+1000)\n", enable, disable));
    ASSERT_EQ(enable.size(), 1u);
    EXPECT_EQ(enable[0].text, "\"game.dll\"+1000");
}

TEST(AaScript, AllocNearInvalid) {
    std::string err = parse_err("[ENABLE]\nalloc(a, 2048, \"unterminated)\n");
    EXPECT_NE(err.find("invalid alloc near expression"), std::string::npos);
    err = parse_err("[ENABLE]\nalloc(a, 2048, xyz!)\n");
    EXPECT_NE(err.find("invalid alloc near address"), std::string::npos);
}

TEST(AaScript, OutsideBlock) {
    std::string err = parse_err("mov eax, 1\n");
    EXPECT_NE(err.find("statement outside"), std::string::npos);
}

TEST(AaScript, UnknownKeyword) {
    std::string err = parse_err("[ENABLE]\nfrobnicate(a,b)\n");
    // frobnicate( is not a known keyword -> classified as asm line; Keystone
    // errors at execution. Parse-level unknown keyword lines are asm.
    EXPECT_EQ(err, "");
}

TEST(AaScript, MissingClosingParen) {
    std::string err = parse_err("[ENABLE]\nalloc(a,2048\n");
    EXPECT_NE(err.find("expected ')'"), std::string::npos);
}

TEST(AaScript, InvalidAllocSize) {
    std::string err = parse_err("[ENABLE]\nalloc(a,zero)\n");
    EXPECT_NE(err.find("invalid alloc size"), std::string::npos);
}

TEST(AaScript, DeallocOnlyInDisable) {
    std::string err = parse_err("[ENABLE]\ndealloc(a)\n");
    EXPECT_NE(err.find("dealloc is only allowed in [DISABLE]"), std::string::npos);
}

TEST(AaScript, AllocOnlyInEnable) {
    std::string err = parse_err("[DISABLE]\nalloc(a, 8)\n");
    EXPECT_NE(err.find("alloc is only allowed in [ENABLE]"), std::string::npos);
}

TEST(AaScript, UnregisterOnlyInDisable) {
    std::string err = parse_err("[ENABLE]\nunregistersymbol(a)\n");
    EXPECT_NE(err.find("unregistersymbol is only allowed in [DISABLE]"),
              std::string::npos);
}

TEST(AaScript, NoBlock) {
    std::string err = parse_err("// only a comment\n\n");
    EXPECT_NE(err.find("missing [ENABLE]/[DISABLE] block"), std::string::npos);
}

TEST(AaScript, DuplicateBlock) {
    std::string err = parse_err("[ENABLE]\n[ENABLE]\n");
    EXPECT_NE(err.find("duplicate [ENABLE] block"), std::string::npos);
}

TEST(AaScript, UnknownBlock) {
    std::string err = parse_err("[FOO]\n");
    EXPECT_NE(err.find("unknown block"), std::string::npos);
}

TEST(AaScript, MalformedBlockMarker) {
    std::string err = parse_err("[ENABLE] garbage\n");
    EXPECT_NE(err.find("unknown block"), std::string::npos);
}

TEST(AaScript, InvalidDbHex) {
    std::string err = parse_err("[DISABLE]\ndb GG 33\n");
    EXPECT_NE(err.find("invalid db hex bytes"), std::string::npos);
}

TEST(AaScript, ModuleOffsetHexConvention) {
    const char* text =
        "[ENABLE]\n"
        "\"kernel32.dll\"+0x1000:\n"
        "jmp newmem\n";
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(text, enable, disable));
    ASSERT_EQ(enable.size(), 2u);
    EXPECT_EQ(enable[0].kind, StepKind::HookTarget);
    EXPECT_EQ(enable[0].module, "kernel32.dll");
    EXPECT_EQ(enable[0].offset, 0x1000u);
}

TEST(AaScript, LineNumbersTracked) {
    const char* text = "[ENABLE]\nalloc(a,8)\n\nmov eax,1\n";
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(text, enable, disable));
    ASSERT_EQ(enable.size(), 2u);
    EXPECT_EQ(enable[0].line, 2u);
    EXPECT_EQ(enable[1].line, 4u);
}

TEST(AaScript, NopFillParsed) {
    const char* text = "[ENABLE]\nalloc(a,8)\nnop 5\n";
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(text, enable, disable));
    ASSERT_EQ(enable.size(), 2u);
    EXPECT_EQ(enable[1].kind, StepKind::NopFill);
    EXPECT_EQ(enable[1].size, 5u);
}

TEST(AaScript, BareNopIsNop1) {
    // CE hook filler style: a bare "nop" after the hook jmp is nop 1 (the
    // user's example uses "jmp newmem\nnop"). In a normal block it behaves
    // identically to the 1-byte 0x90 it would assemble to anyway.
    const char* text = "[ENABLE]\nalloc(a,8)\nnop\n";
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(text, enable, disable));
    ASSERT_EQ(enable.size(), 2u);
    EXPECT_EQ(enable[1].kind, StepKind::NopFill);
    EXPECT_EQ(enable[1].size, 1u);
}

TEST(AaScript, CommentInsideQuoteKept) {
    const char* text =
        "[ENABLE]\n"
        "mov rax, \"a//b\"\n";  // // inside a string must not be stripped
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(text, enable, disable));
    ASSERT_EQ(enable.size(), 1u);
    EXPECT_EQ(enable[0].text, "mov rax, \"a//b\"");
}

// ---- script check: static validation helpers ----

namespace {

}  // namespace

TEST(AaCheck, CollectSymbols) {
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok("[ENABLE]\nalloc(a,8)\nlabel(x)\ny:\n", enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    EXPECT_EQ(alloc_names.size(), 1u);
    EXPECT_TRUE(alloc_names.count("a") == 1u);
    ASSERT_EQ(symbols.size(), 3u);  // a, x, y
    EXPECT_TRUE(symbols.count("a") == 1u);
    EXPECT_TRUE(symbols.count("x") == 1u);
    EXPECT_TRUE(symbols.count("y") == 1u);
}

TEST(AaCheck, HookStructureOk) {
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(
        "[ENABLE]\nalloc(n,8)\n\"m.dll\"+100:\njmp n\nnop 2\n", enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_TRUE(internal::aa::aa_check_hook_structure(enable, alloc_names, symbols,
                                                      err));
    EXPECT_EQ(err, "");
}

TEST(AaCheck, HookNoJmpFails) {
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok("[ENABLE]\n\"m.dll\"+100:\nnop 2\n", enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_FALSE(internal::aa::aa_check_hook_structure(enable, alloc_names, symbols,
                                                       err));
    EXPECT_NE(err.find("hook target must be followed by 'jmp <label>'"),
              std::string::npos);
}

TEST(AaCheck, HookUndefinedLabelFails) {
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok("[ENABLE]\n\"m.dll\"+100:\njmp nonexist\n", enable,
                         disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_FALSE(internal::aa::aa_check_hook_structure(enable, alloc_names, symbols,
                                                       err));
    EXPECT_NE(err.find("undefined label 'nonexist'"), std::string::npos);
}

TEST(AaCheck, HookSecondAsmFails) {
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok("[ENABLE]\nalloc(n,8)\n\"m.dll\"+100:\njmp n\nmov eax,1\n",
                         enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_FALSE(internal::aa::aa_check_hook_structure(enable, alloc_names, symbols,
                                                       err));
    EXPECT_NE(err.find("only 'jmp <label>' is supported after a hook target"),
              std::string::npos);
}

TEST(AaCheck, PrecheckValidAsm) {
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok("[ENABLE]\nalloc(n,64)\nn:\nmov rax, 1\nadd rax, 2\n",
                         enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_TRUE(internal::aa::aa_precheck_asm(enable, alloc_names, symbols, err));
    EXPECT_EQ(err, "");
}

TEST(AaCheck, PrecheckBadMnemonic) {
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok("[ENABLE]\nalloc(n,64)\nn:\nbogus rax, 1\n", enable,
                         disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_FALSE(internal::aa::aa_precheck_asm(enable, alloc_names, symbols, err));
    EXPECT_NE(err.find("BadFormat"), std::string::npos);
}

TEST(AaCheck, PrecheckBadOperand) {
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok("[ENABLE]\nalloc(n,64)\nn:\nmov rax, bogusreg\n", enable,
                         disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_FALSE(internal::aa::aa_precheck_asm(enable, alloc_names, symbols, err));
    EXPECT_NE(err.find("BadFormat"), std::string::npos);
}

TEST(AaCheck, UserHookScriptPasses) {
    // The user's example hook script shape (hook target + jmp + filler +
    // labels) within the documented capability boundary must pass both static
    // checks. Since v2.5.0 the `mov [addsunrcx],rcx` artificial-pointer line
    // is supported (non-accumulator memory operand -> RIP-relative) and is
    // included exactly as in the user's example.
    const char* text =
        "[ENABLE]\n"
        "alloc(newmem,2048,\"m.dll\"+100)\n"
        "label(returnhere)\n"
        "label(originalcode)\n"
        "label(exit)\n"
        "alloc(addsunrcx, 8)\n"
        "registersymbol(addsunrcx)\n"
        "newmem:\n"
        "mov [addsunrcx],rcx\n"
        "xor r9d,r9d\n"
        "cvttss2si r8d,[rsp+44]\n"
        "exit:\n"
        "jmp returnhere\n"
        "\"m.dll\"+100:\n"
        "jmp newmem\n"
        "nop 5\n"
        "returnhere:\n";
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(text, enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_TRUE(internal::aa::aa_check_hook_structure(enable, alloc_names, symbols,
                                                      err));
    EXPECT_EQ(err, "");
    EXPECT_TRUE(internal::aa::aa_precheck_asm(enable, alloc_names, symbols, err));
    EXPECT_EQ(err, "");
}

// v2.6.0: digit-leading symbol names are rejected so a script symbol can
// never shadow a numeric address in CLI address args (numeric-first).
TEST(AaParse, DigitLeadingSymbolRejected) {
    std::vector<internal::aa::Step> enable, disable;
    std::string err;
    EXPECT_FALSE(internal::aa::aa_parse_text(
        "[ENABLE]\nalloc(100, 8)\n", enable, disable, err));
    EXPECT_NE(err.find("invalid symbol name"), std::string::npos);
    EXPECT_FALSE(internal::aa::aa_parse_text(
        "[ENABLE]\nalloc(1slot, 8)\n", enable, disable, err));
    EXPECT_NE(err.find("invalid symbol name"), std::string::npos);
    // underscore/letter leading names still fine
    EXPECT_TRUE(internal::aa::aa_parse_text(
        "[ENABLE]\nalloc(_slot, 8)\n", enable, disable, err));
    EXPECT_TRUE(internal::aa::aa_parse_text(
        "[ENABLE]\nalloc(sunObjPtr, 8)\n", enable, disable, err));
}

TEST(AaCheck, UserCallScriptPasses) {
    // The user's createThread example script shape (alloc + createThread +
    // stack-aligned stub) within the documented capability boundary must pass
    // both static checks. The original example's `mov r12,"m.dll"+100` line is
    // intentionally simplified: string-immediate operands in asm lines are not
    // supported (module addresses are written via "module"+offset: labels).
    const char* text =
        "[ENABLE]\n"
        "alloc(newmem,1024)\n"
        "createThread(newmem)\n"
        "newmem:\n"
        "sub rsp,28\n"
        "add rsp,28\n"
        "ret\n";
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(text, enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_TRUE(internal::aa::aa_check_hook_structure(enable, alloc_names, symbols,
                                                      err));
    EXPECT_EQ(err, "");
    EXPECT_TRUE(internal::aa::aa_precheck_asm(enable, alloc_names, symbols, err));
    EXPECT_EQ(err, "");
}

TEST(AaCheck, SymbolRefImmediatePasses) {
    // v2.5.0: immediate symbol references are supported (movabs imm64). The
    // precheck uses placeholder (low) addresses, so `mov rax, n` is legal;
    // the 32-bit-operand truncation case only arises at run time with real
    // addresses and is covered by the static-library unit tests.
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(
        "[ENABLE]\nalloc(n,64)\nn:\nmov rax, n\n", enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_TRUE(internal::aa::aa_precheck_asm(enable, alloc_names, symbols, err));
    EXPECT_EQ(err, "");
}

TEST(AaCheck, SymbolRefMemOperandsPass) {
    // v2.5.0: memory-operand symbol references (artificial pointer) pass the
    // precheck: accumulator movs (moffs64) and non-accumulator (RIP-relative).
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(
        "[ENABLE]\nalloc(slot,8)\nalloc(code,64)\ncode:\n"
        "mov [slot],rax\nmov rax,[slot]\nmov [slot],rcx\nlea rdx,[slot]\n",
        enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_TRUE(internal::aa::aa_precheck_asm(enable, alloc_names, symbols, err));
    EXPECT_EQ(err, "");
}

TEST(AaCheck, ComplexMemExprFails) {
    // Complex memory expressions referencing a symbol are rejected explicitly
    // (never silently truncated), both at check time and run time.
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(
        "[ENABLE]\nalloc(slot,8)\nalloc(code,64)\nslot:\ncode:\nmov rax,[slot+4]\n",
        enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_FALSE(internal::aa::aa_precheck_asm(enable, alloc_names, symbols, err));
    EXPECT_NE(err.find("BadFormat"), std::string::npos);
}

TEST(AaCheck, UndefinedSymbolRefFails) {
    // A reference to a symbol that is neither alloc'd nor defined still fails.
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(
        "[ENABLE]\nalloc(code,64)\ncode:\nmov [ghost],rax\n", enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_FALSE(internal::aa::aa_precheck_asm(enable, alloc_names, symbols, err));
    EXPECT_NE(err.find("BadFormat"), std::string::npos);
}

TEST(AaCheck, NopNoWriteTargetFails) {
    // A bare nop (NopFill) before any alloc'd label or hook target is a write
    // with no target: check must reject it (mirroring exec_enable's rule),
    // instead of passing while run writes to address 0.
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok("[ENABLE]\nnop\n", enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_FALSE(internal::aa::aa_precheck_asm(enable, alloc_names, symbols, err));
    EXPECT_NE(err.find("asm line has no write target"), std::string::npos);
}

TEST(AaCheck, AsmNoWriteTargetFails) {
    // Mirror of exec_enable's flush_asm rule: bare asm with no alloc'd-label
    // switch and no hook target must be rejected statically (run errors with
    // "asm line has no write target" at exit 1; check reports it as exit 2).
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok("[ENABLE]\nmov eax, 1\n", enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_FALSE(internal::aa::aa_precheck_asm(enable, alloc_names, symbols, err));
    EXPECT_NE(err.find("asm line has no write target"), std::string::npos);
}

TEST(AaCheck, AsmAfterAllocBeforeLabelFails) {
    // alloc alone does not establish a write target; the label line must
    // follow before asm lines (same rule as exec_enable).
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok("[ENABLE]\nalloc(n,64)\nmov eax, 1\n", enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_FALSE(internal::aa::aa_precheck_asm(enable, alloc_names, symbols, err));
    EXPECT_NE(err.find("asm line has no write target"), std::string::npos);
}

TEST(AaCheck, HookJmpToPlainLabelFails) {
    // Mirror of exec_enable: a hook jmp may only target an alloc'd symbol or
    // a label defined inside a hook block (ext_labels). A label defined in a
    // plain asm section is NOT a valid hook jmp target (run -> BadFormat);
    // check must reject it too instead of accepting the wider symbol set.
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok("[ENABLE]\nplain:\nmov eax,1\n\"m.dll\"+100:\njmp plain\n",
                         enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    // The plain label is followed by asm with no target -> first failure is
    // the write-target error at that group; but the hook jmp label check must
    // also reject 'plain'. Run both validations in the same order as check.
    EXPECT_FALSE(internal::aa::aa_check_hook_structure(enable, alloc_names, symbols,
                                                       err));
    EXPECT_NE(err.find("undefined label 'plain'"), std::string::npos);
}

TEST(AaCheck, HookJmpToAllocOrExtLabelOk) {
    // A hook jmp to an alloc'd symbol, and one to a label defined inside the
    // hook block, are both accepted (run resolves them via ctx.symbols and
    // ext_labels respectively).
    std::vector<Step> enable, disable;
    ASSERT_TRUE(parse_ok(
        "[ENABLE]\nalloc(n,64)\nn:\nmov eax,1\n\"m.dll\"+100:\njmp n\n"
        "returnhere:\n\"m2.dll\"+200:\njmp returnhere\n",
        enable, disable));
    std::set<std::string> alloc_names;
    std::map<std::string, uintptr_t> symbols;
    internal::aa::aa_collect_symbols(enable, alloc_names, symbols);
    std::string err;
    EXPECT_TRUE(internal::aa::aa_check_hook_structure(enable, alloc_names, symbols,
                                                      err));
    EXPECT_EQ(err, "");
}

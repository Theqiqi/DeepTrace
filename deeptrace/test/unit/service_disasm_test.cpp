#include "deeptrace.h"

#include <gtest/gtest.h>

#include <vector>

using namespace deeptrace;

TEST(ServiceDisasmBuffer, DecodesKnownBytesFromZero) {
    // mov rax, [rip+0] ; jmp [rip+0] ; ret  (base address 0 for files)
    const uint8_t b[] = {0x48, 0x8B, 0x05, 0x00, 0x00, 0x00, 0x00,
                         0xFF, 0x25, 0x00, 0x00, 0x00, 0x00, 0xC3};
    std::vector<Instruction> insns;
    EXPECT_EQ(disasm_buffer(b, sizeof(b), 0, 100, insns), Result::Ok);
    ASSERT_EQ(insns.size(), 3u);
    EXPECT_EQ(insns[0].address, 0u);
    EXPECT_EQ(insns[0].bytes.size(), 7u);
    EXPECT_NE(insns[0].text.find("mov rax"), std::string::npos);
    EXPECT_EQ(insns[1].address, 7u);
    EXPECT_NE(insns[1].text.find("jmp"), std::string::npos);
    EXPECT_EQ(insns[2].address, 13u);
    EXPECT_EQ(insns[2].text, "ret");
}

TEST(ServiceDisasmBuffer, CountLimit) {
    const uint8_t b[] = {0x90, 0x90, 0x90, 0x90};  // 4x nop
    std::vector<Instruction> insns;
    EXPECT_EQ(disasm_buffer(b, sizeof(b), 0, 2, insns), Result::Ok);
    ASSERT_EQ(insns.size(), 2u);
}

TEST(ServiceDisasmBuffer, BaseAddrShiftsAddresses) {
    const uint8_t b[] = {0x90, 0xC3};
    std::vector<Instruction> insns;
    EXPECT_EQ(disasm_buffer(b, sizeof(b), 0x1000, 10, insns), Result::Ok);
    ASSERT_EQ(insns.size(), 2u);
    EXPECT_EQ(insns[0].address, 0x1000u);
    EXPECT_EQ(insns[1].address, 0x1001u);
}

TEST(ServiceDisasmBuffer, TruncatesOnUndecodableByte) {
    // 0x0F 0x0B is UD2 (valid); a stray high byte after decodes as the start
    // of a long instruction or stops: decode stops at the first failure but
    // still returns Ok with what was decoded.
    const uint8_t b[] = {0x90, 0x0F, 0x0B, 0xFF, 0xFF, 0xFF};
    std::vector<Instruction> insns;
    EXPECT_EQ(disasm_buffer(b, sizeof(b), 0, 100, insns), Result::Ok);
    EXPECT_FALSE(insns.empty());
}

TEST(ServiceDisasmBuffer, EmptyBufferOk) {
    std::vector<Instruction> insns;
    EXPECT_EQ(disasm_buffer(nullptr, 0, 0, 100, insns), Result::Ok);
    EXPECT_TRUE(insns.empty());
}

TEST(ServiceDisasmBuffer, InvalidArgs) {
    std::vector<Instruction> insns;
    EXPECT_EQ(disasm_buffer(nullptr, 8, 0, 1, insns), Result::InvalidArg);
    EXPECT_EQ(disasm_buffer(nullptr, 0, 0, 0, insns), Result::InvalidArg);
    EXPECT_EQ(disasm_buffer(nullptr, 0, 0, 10001, insns), Result::InvalidArg);
}

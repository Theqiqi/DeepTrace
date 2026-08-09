#include "infrastructure/disassembly/disasm.h"

#include <gtest/gtest.h>

using namespace pmem::internal;

namespace {

DecodedInsn decode(const uint8_t* bytes, size_t len, uint64_t addr = 0x140001000) {
    DecodedInsn out;
    EXPECT_TRUE(disasm_one(bytes, len, addr, out));
    return out;
}

}  // namespace

TEST(Disasm, Nop) {
    const uint8_t b[] = {0x90};
    auto d = decode(b, 1);
    EXPECT_EQ(d.length, 1u);
    EXPECT_EQ(d.text, "nop");
}

TEST(Disasm, Int3) {
    const uint8_t b[] = {0xCC};
    auto d = decode(b, 1);
    EXPECT_EQ(d.text, "int3");
}

TEST(Disasm, Ret) {
    const uint8_t b[] = {0xC3};
    auto d = decode(b, 1);
    EXPECT_EQ(d.text, "ret");
}

TEST(Disasm, PushRax) {
    const uint8_t b[] = {0x50};
    auto d = decode(b, 1);
    EXPECT_EQ(d.text, "push rax");
}

TEST(Disasm, PopRbx) {
    const uint8_t b[] = {0x5B};
    auto d = decode(b, 1);
    EXPECT_EQ(d.text, "pop rbx");
}

TEST(Disasm, MovRaxImm64) {
    // Capstone names the imm64 form "movabs" (Intel convention).
    const uint8_t b[] = {0x48, 0xB8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    auto d = decode(b, sizeof(b));
    EXPECT_EQ(d.length, 10u);
    EXPECT_EQ(d.text, "movabs rax, 0x807060504030201");
}

TEST(Disasm, MovRegReg) {
    const uint8_t b[] = {0x48, 0x89, 0xD8};  // mov rax, rbx
    auto d = decode(b, sizeof(b));
    EXPECT_EQ(d.length, 3u);
    EXPECT_EQ(d.text, "mov rax, rbx");
}

TEST(Disasm, MovRegFromMem) {
    // Capstone Intel syntax includes the operand size and spaces: qword ptr.
    const uint8_t b[] = {0x48, 0x8B, 0x45, 0x08};  // mov rax, qword ptr [rbp+8]
    auto d = decode(b, sizeof(b));
    EXPECT_EQ(d.length, 4u);
    EXPECT_EQ(d.text, "mov rax, qword ptr [rbp + 8]");
}

TEST(Disasm, LeaRipRelative) {
    const uint8_t b[] = {0x48, 0x8D, 0x05, 0x10, 0x00, 0x00, 0x00};  // lea rax, [rip+0x10]
    auto d = decode(b, sizeof(b));
    EXPECT_EQ(d.length, 7u);
    EXPECT_EQ(d.text, "lea rax, [rip + 0x10]");
}

TEST(Disasm, CallRel32) {
    const uint8_t b[] = {0xE8, 0x00, 0x00, 0x00, 0x00};
    auto d = decode(b, sizeof(b), 0x140001000);
    EXPECT_EQ(d.length, 5u);
    EXPECT_EQ(d.text, "call 0x140001005");
}

TEST(Disasm, JmpRel8) {
    const uint8_t b[] = {0xEB, 0xFE};  // jmp $-2 (self loop)
    auto d = decode(b, sizeof(b), 0x140001000);
    EXPECT_EQ(d.length, 2u);
    EXPECT_EQ(d.text, "jmp 0x140001000");
}

TEST(Disasm, JccRel8) {
    const uint8_t b[] = {0x74, 0x0A};  // je +0x0A
    auto d = decode(b, sizeof(b), 0x140001000);
    EXPECT_EQ(d.text, "je 0x14000100c");
}

TEST(Disasm, Syscall) {
    const uint8_t b[] = {0x0F, 0x05};
    auto d = decode(b, sizeof(b));
    EXPECT_EQ(d.text, "syscall");
}

TEST(Disasm, Movzx) {
    const uint8_t b[] = {0x0F, 0xB6, 0xC1};  // movzx eax, cl
    auto d = decode(b, sizeof(b));
    EXPECT_EQ(d.text, "movzx eax, cl");
}

// Capstone coverage beyond the old hand-written decoder: SSE/REP string ops.

TEST(Disasm, SseDecoded) {
    const uint8_t b[] = {0x0F, 0x10, 0x00};  // movups xmm0, xmmword ptr [rax]
    auto d = decode(b, sizeof(b));
    EXPECT_EQ(d.length, 3u);
    EXPECT_EQ(d.text, "movups xmm0, xmmword ptr [rax]");
}

TEST(Disasm, Sse2PxorDecoded) {
    const uint8_t b[] = {0x66, 0x0F, 0xEF, 0xC0};  // pxor xmm0, xmm0
    auto d = decode(b, sizeof(b));
    EXPECT_EQ(d.length, 4u);
    EXPECT_EQ(d.text, "pxor xmm0, xmm0");
}

TEST(Disasm, RepMovsbDecoded) {
    const uint8_t b[] = {0xF3, 0xA4};
    auto d = decode(b, sizeof(b));
    EXPECT_EQ(d.length, 2u);
    EXPECT_EQ(d.text, "rep movsb byte ptr [rdi], byte ptr [rsi]");
}

TEST(Disasm, ShortBuffer) {
    const uint8_t b[] = {0x48, 0xB8};  // truncated movabs
    DecodedInsn out;
    EXPECT_FALSE(disasm_one(b, 2, 0, out));
}

TEST(Disasm, InvalidBytes) {
    // Truly invalid opcode bytes must fail cleanly instead of silently
    // emitting a wrong-length instruction.  0x06 (push es) is invalid in
    // 64-bit mode.
    const uint8_t c[] = {0x06, 0x90, 0x90};
    DecodedInsn out;
    EXPECT_FALSE(disasm_one(c, sizeof(c), 0, out));
}

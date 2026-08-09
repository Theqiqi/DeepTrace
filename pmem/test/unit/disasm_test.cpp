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
    const uint8_t b[] = {0x48, 0xB8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08};
    auto d = decode(b, sizeof(b));
    EXPECT_EQ(d.length, 10u);
    EXPECT_EQ(d.text, "mov rax, 0x807060504030201");
}

TEST(Disasm, MovRegReg) {
    const uint8_t b[] = {0x48, 0x89, 0xD8};  // mov rax, rbx
    auto d = decode(b, sizeof(b));
    EXPECT_EQ(d.length, 3u);
    EXPECT_EQ(d.text, "mov rax, rbx");
}

TEST(Disasm, MovRegFromMem) {
    const uint8_t b[] = {0x48, 0x8B, 0x45, 0x08};  // mov rax, qword ptr [rbp+8]
    auto d = decode(b, sizeof(b));
    EXPECT_EQ(d.length, 4u);
    EXPECT_EQ(d.text, "mov rax, [rbp+0x8]");
}

TEST(Disasm, LeaRipRelative) {
    const uint8_t b[] = {0x48, 0x8D, 0x05, 0x10, 0x00, 0x00, 0x00};  // lea rax, [rip+0x10]
    auto d = decode(b, sizeof(b));
    EXPECT_EQ(d.length, 7u);
    EXPECT_EQ(d.text, "lea rax, [rip+0x10]");
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
    EXPECT_EQ(d.text, "je 0x14000100C");
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

TEST(Disasm, ShortBuffer) {
    const uint8_t b[] = {0x48, 0xB8};  // truncated movabs
    DecodedInsn out;
    EXPECT_FALSE(disasm_one(b, 2, 0, out));
}

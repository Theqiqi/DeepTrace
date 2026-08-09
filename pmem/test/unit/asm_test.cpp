#include "infrastructure/assembly/asmenc.h"

#include <gtest/gtest.h>

#include <vector>

using namespace pmem::internal;

namespace {

std::vector<uint8_t> enc(const std::string& line, bool* ok = nullptr) {
    std::vector<uint8_t> out;
    bool o = asm_one(line, out);
    if (ok) *ok = o;
    return out;
}

}  // namespace

TEST(Asm, Nop) {
    bool ok = false;
    auto b = enc("nop", &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(b.size(), 1u);
    EXPECT_EQ(b[0], 0x90);
}

TEST(Asm, Int3) {
    auto b = enc("int3");
    ASSERT_EQ(b.size(), 1u);
    EXPECT_EQ(b[0], 0xCC);
}

TEST(Asm, Ret) {
    auto b = enc("ret");
    ASSERT_EQ(b.size(), 1u);
    EXPECT_EQ(b[0], 0xC3);
}

TEST(Asm, Syscall) {
    auto b = enc("syscall");
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0x0F);
    EXPECT_EQ(b[1], 0x05);
}

TEST(Asm, PushRax) {
    auto b = enc("push rax");
    ASSERT_EQ(b.size(), 1u);
    EXPECT_EQ(b[0], 0x50);
}

TEST(Asm, PopRbx) {
    auto b = enc("pop rbx");
    ASSERT_EQ(b.size(), 1u);
    EXPECT_EQ(b[0], 0x5B);
}

TEST(Asm, PushImmSmall) {
    auto b = enc("push 5");
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0x6A);
    EXPECT_EQ(b[1], 0x05);
}

TEST(Asm, MovRaxImm64) {
    auto b = enc("mov rax, 0x1234567890");
    ASSERT_EQ(b.size(), 10u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0xB8);
    EXPECT_EQ(b[2], 0x90);   // imm64 little-endian: 90 78 56 34 12 00 00 00
    EXPECT_EQ(b[6], 0x12);
    EXPECT_EQ(b[9], 0x00);
}

TEST(Asm, MovRaxImm32) {
    auto b = enc("mov eax, 0x11223344");
    ASSERT_EQ(b.size(), 5u);
    EXPECT_EQ(b[0], 0xB8);
    EXPECT_EQ(b[4], 0x11);
}

TEST(Asm, MovRbxRax) {
    auto b = enc("mov rbx, rax");
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x89);
    EXPECT_EQ(b[2], 0xC3);
}

TEST(Asm, CallRel32) {
    // Keystone treats the operand as an absolute target (address 0x100),
    // so the rel32 = 0x100 - 5 = 0xFB (keystone's PC-relative semantics).
    auto b = enc("call 0x100");
    ASSERT_EQ(b.size(), 5u);
    EXPECT_EQ(b[0], 0xE8);
    EXPECT_EQ(b[1], 0xFB);
    EXPECT_EQ(b[2], 0x00);
    EXPECT_EQ(b[3], 0x00);
    EXPECT_EQ(b[4], 0x00);
}

TEST(Asm, JmpReg) {
    auto b = enc("jmp rax");
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0xFF);
    EXPECT_EQ(b[1], 0xE0);
}

TEST(Asm, JeRel8) {
    // Keystone: operand is an absolute target; rel8 = 0x0A - 2 = 0x08.
    auto b = enc("je 0x0A");
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0x74);
    EXPECT_EQ(b[1], 0x08);
}

TEST(Asm, UnknownMnemonic) {
    bool ok = true;
    enc("frobnicate rax, 1", &ok);
    EXPECT_FALSE(ok);
}

TEST(Asm, BadRegister) {
    bool ok = true;
    enc("mov zzz, 1", &ok);
    EXPECT_FALSE(ok);
}

// ---- arithmetic / logic group (reg, reg | reg, imm) ----

TEST(Asm, AddRaxImm8) {
    auto b = enc("add rax, 0");
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x83);
    EXPECT_EQ(b[2], 0xC0);
    EXPECT_EQ(b[3], 0x00);
}

TEST(Asm, AddEaxImm32) {
    // Keystone picks the accumulator short form: 05 id (5 bytes).
    auto b = enc("add eax, 0x100");
    ASSERT_EQ(b.size(), 5u);
    EXPECT_EQ(b[0], 0x05);
    EXPECT_EQ(b[1], 0x00);
    EXPECT_EQ(b[2], 0x01);
    EXPECT_EQ(b[3], 0x00);
    EXPECT_EQ(b[4], 0x00);
}

TEST(Asm, AddRaxRbx) {
    auto b = enc("add rax, rbx");
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x01);
    EXPECT_EQ(b[2], 0xD8);
}

TEST(Asm, AddAlImm8) {
    // Keystone picks the accumulator short form: 04 ib (2 bytes).
    auto b = enc("add al, 1");
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0x04);
    EXPECT_EQ(b[1], 0x01);
}

TEST(Asm, SubRaxImm) {
    auto b = enc("sub rax, 0x10");
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x83);
    EXPECT_EQ(b[2], 0xE8);
    EXPECT_EQ(b[3], 0x10);
}

TEST(Asm, XorEaxEax) {
    auto b = enc("xor eax, eax");
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0x31);
    EXPECT_EQ(b[1], 0xC0);
}

TEST(Asm, XorRaxRax) {
    auto b = enc("xor rax, rax");
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x31);
    EXPECT_EQ(b[2], 0xC0);
}

TEST(Asm, CmpRaxRbx) {
    auto b = enc("cmp rax, rbx");
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x39);
    EXPECT_EQ(b[2], 0xD8);
}

TEST(Asm, AndRdiImm8) {
    // 0xFF = 255 does not fit a signed imm8 (max 127), so the imm32 form is
    // the correct encoding (imm32 sign-extended to 64 bits).
    auto b = enc("and rdi, 0xFF");
    ASSERT_EQ(b.size(), 7u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x81);
    EXPECT_EQ(b[2], 0xE7);
    EXPECT_EQ(b[3], 0xFF);
    EXPECT_EQ(b[6], 0x00);
}

TEST(Asm, OrRaxImm8) {
    auto b = enc("or rax, 1");
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x83);
    EXPECT_EQ(b[2], 0xC8);
    EXPECT_EQ(b[3], 0x01);
}

// ---- test / xchg / imul ----

TEST(Asm, TestRaxRax) {
    auto b = enc("test rax, rax");
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x85);
    EXPECT_EQ(b[2], 0xC0);
}

TEST(Asm, TestEaxImm32) {
    // Keystone picks the accumulator short form: A9 id (5 bytes).
    auto b = enc("test eax, 0x80000000");
    ASSERT_EQ(b.size(), 5u);
    EXPECT_EQ(b[0], 0xA9);
    EXPECT_EQ(b[1], 0x00);
    EXPECT_EQ(b[2], 0x00);
    EXPECT_EQ(b[3], 0x00);
    EXPECT_EQ(b[4], 0x80);
}

TEST(Asm, XchgRaxRbx) {
    // Keystone picks the short form: 48 93 (xchg rax, rbx).
    auto b = enc("xchg rax, rbx");
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x93);
}

TEST(Asm, ImulRaxRbx) {
    auto b = enc("imul rax, rbx");
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x0F);
    EXPECT_EQ(b[2], 0xAF);
    EXPECT_EQ(b[3], 0xC3);
}

TEST(Asm, ImulRaxRbxImm8) {
    auto b = enc("imul rax, rbx, 2");
    std::string hex;
    for (uint8_t x : b) {
        char t[8];
        std::snprintf(t, sizeof t, "%02X ", x);
        hex += t;
    }
    EXPECT_EQ(hex, "48 6B C3 02 ") << "actual bytes: " << hex;
}

TEST(Asm, ImulEaxEbxImm32) {
    auto b = enc("imul eax, ebx, 0x100");
    ASSERT_EQ(b.size(), 6u);
    EXPECT_EQ(b[0], 0x69);
    EXPECT_EQ(b[1], 0xC3);
    EXPECT_EQ(b[2], 0x00);
    EXPECT_EQ(b[3], 0x01);
    EXPECT_EQ(b[4], 0x00);
    EXPECT_EQ(b[5], 0x00);
}

// ---- movzx / movsx / movsxd ----

TEST(Asm, MovzxEaxAl) {
    auto b = enc("movzx eax, al");
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x0F);
    EXPECT_EQ(b[1], 0xB6);
    EXPECT_EQ(b[2], 0xC0);
}

TEST(Asm, MovzxRaxAx) {
    auto b = enc("movzx rax, ax");
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x0F);
    EXPECT_EQ(b[2], 0xB7);
    EXPECT_EQ(b[3], 0xC0);
}

TEST(Asm, MovsxRaxAl) {
    auto b = enc("movsx rax, al");
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x0F);
    EXPECT_EQ(b[2], 0xBE);
    EXPECT_EQ(b[3], 0xC0);
}

TEST(Asm, MovsxdRaxEax) {
    auto b = enc("movsxd rax, eax");
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x63);
    EXPECT_EQ(b[2], 0xC0);
}

// ---- high registers (r8-r15) need REX.B in mov reg, imm ----

TEST(Asm, MovR8Imm64) {
    auto b = enc("mov r8, 0x1122334455667788");
    ASSERT_EQ(b.size(), 10u);
    EXPECT_EQ(b[0], 0x49);  // REX.W + REX.B
    EXPECT_EQ(b[1], 0xB8);
    EXPECT_EQ(b[2], 0x88);
}

TEST(Asm, MovR8dImm32) {
    auto b = enc("mov r8d, 0x11223344");
    ASSERT_EQ(b.size(), 6u);
    EXPECT_EQ(b[0], 0x41);  // REX.B
    EXPECT_EQ(b[1], 0xB8);
}

TEST(Asm, MovR8bImm8) {
    auto b = enc("mov r8b, 1");
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x41);  // REX.B
    EXPECT_EQ(b[1], 0xB0);
    EXPECT_EQ(b[2], 0x01);
}

TEST(Asm, MovAhImm8) {
    auto b = enc("mov ah, 1");
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0xB4);  // no REX with high8
    EXPECT_EQ(b[1], 0x01);
}

// high registers in the shared two-op path (REX.R/REX.B)

TEST(Asm, AddR8R9) {
    auto b = enc("add r8, r9");
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x4D);
    EXPECT_EQ(b[1], 0x01);
    EXPECT_EQ(b[2], 0xC8);
}

TEST(Asm, XorR8R8) {
    auto b = enc("xor r8, r8");
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x4D);
    EXPECT_EQ(b[1], 0x31);
    EXPECT_EQ(b[2], 0xC0);
}

// mixed high/low registers: REX.R extends the reg field, REX.B the r/m field

TEST(Asm, ImulR8Rbx) {
    // imul r8, rbx: reg field = r8 (REX.R), r/m = rbx (no REX.B)
    auto b = enc("imul r8, rbx");
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x4C);
    EXPECT_EQ(b[1], 0x0F);
    EXPECT_EQ(b[2], 0xAF);
    EXPECT_EQ(b[3], 0xC3);
}

TEST(Asm, MovzxR8dAl) {
    // movzx r8d, al: reg field = r8d (REX.R), r/m = al (no REX)
    auto b = enc("movzx r8d, al");
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x44);
    EXPECT_EQ(b[1], 0x0F);
    EXPECT_EQ(b[2], 0xB6);
    EXPECT_EQ(b[3], 0xC0);
}

TEST(Asm, MovzxEaxR8b) {
    // movzx eax, r8b: reg field = eax (no REX.R), r/m = r8b (REX.B)
    auto b = enc("movzx eax, r8b");
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x41);
    EXPECT_EQ(b[1], 0x0F);
    EXPECT_EQ(b[2], 0xB6);
    EXPECT_EQ(b[3], 0xC0);
}

// ---- capability boundary (Keystone backend) ----

TEST(Asm, LeaSupported) {
    // Keystone supports memory operands: lea rax, [rbp+8] -> 48 8D 45 08.
    bool ok = false;
    auto b = enc("lea rax, [rbp+8]", &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x8D);
    EXPECT_EQ(b[2], 0x45);
    EXPECT_EQ(b[3], 0x08);
}

TEST(Asm, MemOperandSupported) {
    // Memory operands now assemble: mov [rax], rbx -> 48 89 18.
    bool ok = false;
    auto b = enc("mov [rax], rbx", &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x89);
    EXPECT_EQ(b[2], 0x18);

    // Memory operand with explicit size (required by Keystone):
    // add dword ptr [rax], 1 -> 83 00 01.
    ok = false;
    b = enc("add dword ptr [rax], 1", &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x83);
    EXPECT_EQ(b[1], 0x00);
    EXPECT_EQ(b[2], 0x01);
}

TEST(Asm, WidthMismatchRejected) {
    bool ok = true;
    enc("add rax, ebx", &ok);
    EXPECT_FALSE(ok);
    enc("mov rax, ebx", &ok);
    EXPECT_FALSE(ok);
}

TEST(Asm, Imul8BitRejected) {
    bool ok = true;
    enc("imul al, bl", &ok);
    EXPECT_FALSE(ok);
}

TEST(Asm, MovsxSameWidthSupported) {
    // movsx rax, eax is a valid instruction (sign-extend r32 to r64):
    // 48 63 C0.  The old hand-written encoder rejected it; Keystone accepts.
    bool ok = false;
    auto b = enc("movsx rax, eax", &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x63);
    EXPECT_EQ(b[2], 0xC0);
}

TEST(Asm, AluImm64OutOfRangeRejected) {
    bool ok = true;
    enc("add rax, 0x100000000", &ok);
    EXPECT_FALSE(ok);
}

// ---- Keystone coverage beyond the old hand-written encoder ----

TEST(Asm, SseSupported) {
    // SSE instructions were missing from the old encoder.
    bool ok = false;
    auto b = enc("movups xmm0, xmm1", &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], 0x0F);
    EXPECT_EQ(b[1], 0x10);
    EXPECT_EQ(b[2], 0xC1);

    ok = false;
    b = enc("pxor xmm0, xmm0", &ok);
    EXPECT_TRUE(ok);
    // SSE2 pxor xmm, xmm requires the 66 prefix: 66 0F EF C0.
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x66);
    EXPECT_EQ(b[1], 0x0F);
    EXPECT_EQ(b[2], 0xEF);
    EXPECT_EQ(b[3], 0xC0);
}

TEST(Asm, RepStringSupported) {
    // rep movsb / rep stosb were missing from the old encoder.
    bool ok = false;
    auto b = enc("rep movsb", &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0xF3);
    EXPECT_EQ(b[1], 0xA4);

    ok = false;
    b = enc("rep stosb", &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(b.size(), 2u);
    EXPECT_EQ(b[0], 0xF3);
    EXPECT_EQ(b[1], 0xAA);
}

TEST(Asm, MemOperandWithDisp) {
    // qword ptr memory operand with displacement: 48 8B 45 10 (disp8).
    bool ok = false;
    auto b = enc("mov rax, qword ptr [rbp+0x10]", &ok);
    EXPECT_TRUE(ok);
    ASSERT_EQ(b.size(), 4u);
    EXPECT_EQ(b[0], 0x48);
    EXPECT_EQ(b[1], 0x8B);
    EXPECT_EQ(b[2], 0x45);
    EXPECT_EQ(b[3], 0x10);
}

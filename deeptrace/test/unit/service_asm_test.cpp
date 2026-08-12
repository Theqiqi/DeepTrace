// v2.5.0: symbol references on non-jmp/call instructions (artificial pointer
// pattern): memory operands (RIP-relative / moffs64) and immediates with
// silent-truncation detection. Pure local assembly - no target required.

#include "service/asm.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

using namespace deeptrace;

namespace {

// Read a little-endian u64 from bytes starting at off.
uint64_t le64(const std::vector<uint8_t>& b, size_t off) {
    uint64_t v = 0;
    for (size_t k = 0; k < 8; ++k) v |= static_cast<uint64_t>(b[off + k]) << (8 * k);
    return v;
}

std::map<std::string, uintptr_t> one_symbol(uintptr_t addr) {
    std::map<std::string, uintptr_t> syms;
    syms["slot"] = addr;
    return syms;
}

}  // namespace

TEST(AsmLabels, MovMemSlotStoreAccumulator) {
    // mov [slot],rax -> moffs64 store A3 (REX.W + A3 + addr64), full address
    // regardless of distance.
    std::map<std::string, uintptr_t> syms = one_symbol(0x7FF612345678ull);
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("mov [slot],rax", 0x1000, syms, bytes, nullptr),
              Result::Ok);
    ASSERT_EQ(bytes.size(), 10u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0xA3);
    EXPECT_EQ(le64(bytes, 2), 0x7FF612345678ull);
}

TEST(AsmLabels, MovMemSlotLoadAccumulator) {
    // mov rax,[slot] -> moffs64 load A1.
    std::map<std::string, uintptr_t> syms = one_symbol(0x140000000ull);
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("mov rax,[slot]", 0x1000, syms, bytes, nullptr),
              Result::Ok);
    ASSERT_EQ(bytes.size(), 10u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0xA1);
    EXPECT_EQ(le64(bytes, 2), 0x140000000ull);
}

TEST(AsmLabels, MovMemSlotStoreAccumulator32) {
    // mov [slot],eax -> moffs64 store without REX (9 bytes).
    std::map<std::string, uintptr_t> syms = one_symbol(0x12340000ull);
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("mov [slot],eax", 0x1000, syms, bytes, nullptr),
              Result::Ok);
    ASSERT_EQ(bytes.size(), 9u);
    EXPECT_EQ(bytes[0], 0xA3);
    EXPECT_EQ(le64(bytes, 1), 0x12340000ull);
}

TEST(AsmLabels, MovMemSlotNonAccumulator) {
    // mov [slot],rcx -> RIP-relative [rip+disp32]: 48 89 0D disp32.
    // insn at 0x1000, len 7; disp = 0x2000 - 0x1007 = 0xFF9.
    std::map<std::string, uintptr_t> syms = one_symbol(0x2000);
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("mov [slot],rcx", 0x1000, syms, bytes, nullptr),
              Result::Ok);
    ASSERT_EQ(bytes.size(), 7u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x89);
    EXPECT_EQ(bytes[2], 0x0D);
    EXPECT_EQ(bytes[3], 0xF9);
    EXPECT_EQ(bytes[4], 0x0F);
    EXPECT_EQ(bytes[5], 0x00);
    EXPECT_EQ(bytes[6], 0x00);
}

TEST(AsmLabels, MovMemSlotNegativeDisp) {
    // sym below the instruction: disp = 0x900 - 0x1007 = -0x707 = 0xFFFFF8F9.
    std::map<std::string, uintptr_t> syms = one_symbol(0x900);
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("mov [slot],rcx", 0x1000, syms, bytes, nullptr),
              Result::Ok);
    ASSERT_EQ(bytes.size(), 7u);
    EXPECT_EQ(bytes[3], 0xF9);
    EXPECT_EQ(bytes[4], 0xF8);
    EXPECT_EQ(bytes[5], 0xFF);
    EXPECT_EQ(bytes[6], 0xFF);
}

TEST(AsmLabels, MovMemSlotLayoutShift) {
    // nop at 0x1000, mov at 0x1001 (len 7); disp = 0x2000 - 0x1008 = 0xFF8.
    std::map<std::string, uintptr_t> syms = one_symbol(0x2000);
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("nop\nmov [slot],rcx\nnop", 0x1000, syms,
                                  bytes, nullptr),
              Result::Ok);
    ASSERT_EQ(bytes.size(), 9u);
    EXPECT_EQ(bytes[0], 0x90);
    EXPECT_EQ(bytes[1], 0x48);
    EXPECT_EQ(bytes[2], 0x89);
    EXPECT_EQ(bytes[3], 0x0D);
    EXPECT_EQ(bytes[4], 0xF8);
    EXPECT_EQ(bytes[5], 0x0F);
    EXPECT_EQ(bytes[6], 0x00);
    EXPECT_EQ(bytes[7], 0x00);
    EXPECT_EQ(bytes[8], 0x90);
}

TEST(AsmLabels, LeaMemSlot) {
    // lea rax,[slot] -> 48 8D 05 disp32; disp = 0x2000 - 0x1007 = 0xFF9.
    std::map<std::string, uintptr_t> syms = one_symbol(0x2000);
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("lea rax,[slot]", 0x1000, syms, bytes, nullptr),
              Result::Ok);
    ASSERT_EQ(bytes.size(), 7u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x8D);
    EXPECT_EQ(bytes[2], 0x05);
    EXPECT_EQ(bytes[3], 0xF9);
    EXPECT_EQ(bytes[4], 0x0F);
    EXPECT_EQ(bytes[5], 0x00);
    EXPECT_EQ(bytes[6], 0x00);
}

TEST(AsmLabels, MovzxMemSlot) {
    // movzx eax, byte ptr [slot] -> 0F B6 05 disp32 (7 bytes);
    // disp = 0x2000 - 0x1007 = 0xFF9.
    std::map<std::string, uintptr_t> syms = one_symbol(0x2000);
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("movzx eax, byte ptr [slot]", 0x1000, syms,
                                  bytes, nullptr),
              Result::Ok);
    ASSERT_EQ(bytes.size(), 7u);
    EXPECT_EQ(bytes[0], 0x0F);
    EXPECT_EQ(bytes[1], 0xB6);
    EXPECT_EQ(bytes[2], 0x05);
    EXPECT_EQ(bytes[3], 0xF9);
    EXPECT_EQ(bytes[4], 0x0F);
    EXPECT_EQ(bytes[5], 0x00);
    EXPECT_EQ(bytes[6], 0x00);
}

TEST(AsmLabels, MovImmediate64) {
    // mov rax,slot -> movabs imm64: 48 B8 + addr64.
    std::map<std::string, uintptr_t> syms = one_symbol(0x7FF612345678ull);
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("mov rax,slot", 0x1000, syms, bytes, nullptr),
              Result::Ok);
    ASSERT_EQ(bytes.size(), 10u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0xB8);
    EXPECT_EQ(le64(bytes, 2), 0x7FF612345678ull);
}

TEST(AsmLabels, MovImmediateSmallValue) {
    // value fits imm32: Keystone may pick the short form; verification only
    // requires the encoded value to equal the symbol address.
    std::map<std::string, uintptr_t> syms = one_symbol(0x12345678);
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("mov rax,slot", 0x1000, syms, bytes, nullptr),
              Result::Ok);
    // 48 C7 C0 imm32 sign-extended (7 bytes) or 48 B8 movabs (10 bytes).
    ASSERT_TRUE(bytes.size() == 7u || bytes.size() == 10u);
}

TEST(AsmLabels, MovImmediate32TruncationRejected) {
    // mov ecx,slot with a 64-bit address cannot be encoded (imm32 would
    // truncate); must be BadFormat, never silently wrong.
    std::map<std::string, uintptr_t> syms = one_symbol(0x7FF612345678ull);
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("mov ecx,slot", 0x1000, syms, bytes, nullptr),
              Result::BadFormat);
}

TEST(AsmLabels, PushImmediateTruncationRejected) {
    // push imm32 sign-extends; a high address cannot be represented.
    std::map<std::string, uintptr_t> syms = one_symbol(0x7FF612345678ull);
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("push slot", 0x1000, syms, bytes, nullptr),
              Result::BadFormat);
}

TEST(AsmLabels, ComplexMemExpressionRejected) {
    std::map<std::string, uintptr_t> syms = one_symbol(0x2000);
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("mov [slot+4],rax", 0x1000, syms, bytes,
                                  nullptr),
              Result::BadFormat);
    EXPECT_EQ(asm_assemble_labels("mov rax,[rsp+slot]", 0x1000, syms, bytes,
                                  nullptr),
              Result::BadFormat);
}

TEST(AsmLabels, StreamLabelMemRefResolved) {
    // A memory operand may reference a label defined in the same stream; the
    // layout address is used. "lab:" at 0x1000; mov at 0x1000 (len 7) ->
    // disp = 0x1000 - 0x1007 = -7 = 0xFFFFFFF9.
    std::map<std::string, uintptr_t> syms;
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("lab:\nmov [lab],rcx", 0x1000, syms, bytes,
                                  nullptr),
              Result::Ok);
    ASSERT_EQ(bytes.size(), 7u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x89);
    EXPECT_EQ(bytes[2], 0x0D);
    EXPECT_EQ(bytes[3], 0xF9);
    EXPECT_EQ(bytes[4], 0xFF);
    EXPECT_EQ(bytes[5], 0xFF);
    EXPECT_EQ(bytes[6], 0xFF);
}

TEST(AsmLabels, StreamLabelImmediateRejected) {
    // Immediate references to stream-internal labels depend on layout and
    // cannot be encoded safely.
    std::map<std::string, uintptr_t> syms;
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("lab:\nmov rax,lab", 0x1000, syms, bytes,
                                  nullptr),
              Result::BadFormat);
}

TEST(AsmLabels, JumpCallStillWork) {
    // Existing behavior unchanged: jmp/call to external symbols and stream
    // labels are self-encoded rel32.
    std::map<std::string, uintptr_t> syms = one_symbol(0x2000);
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("jmp slot\ncall slot", 0x1000, syms, bytes,
                                  nullptr),
              Result::Ok);
    ASSERT_EQ(bytes.size(), 10u);
    EXPECT_EQ(bytes[0], 0xE9);
    EXPECT_EQ(bytes[5], 0xE8);
    // jmp rel32 = 0x2000 - 0x1005 = 0xFFB; call at 0x1005, rel32 =
    // 0x2000 - 0x100A = 0xFF6
    EXPECT_EQ(bytes[1], 0xFB);
    EXPECT_EQ(bytes[2], 0x0F);
    EXPECT_EQ(bytes[3], 0x00);
    EXPECT_EQ(bytes[4], 0x00);
    EXPECT_EQ(bytes[6], 0xF6);
    EXPECT_EQ(bytes[7], 0x0F);
    EXPECT_EQ(bytes[8], 0x00);
    EXPECT_EQ(bytes[9], 0x00);
}

TEST(AsmLabels, UndefinedSymbolStillBadFormat) {
    std::map<std::string, uintptr_t> syms;
    std::vector<uint8_t> bytes;
    EXPECT_EQ(asm_assemble_labels("mov [ghost],rax", 0x1000, syms, bytes,
                                  nullptr),
              Result::BadFormat);
}

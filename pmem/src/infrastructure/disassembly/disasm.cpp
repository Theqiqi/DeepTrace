#include "infrastructure/disassembly/disasm.h"

#include <cstdio>
#include <cstring>

namespace pmem::internal {

namespace {

// x64 register names indexed by (rex, reg3).
static const char* kReg64[16] = {"rax", "rcx", "rdx", "rbx", "rsp", "rbp",
                                 "rsi", "rdi", "r8",  "r9",  "r10", "r11",
                                 "r12", "r13", "r14", "r15"};
static const char* kReg32[16] = {"eax", "ecx", "edx", "ebx", "esp", "ebp",
                                 "esi", "edi", "r8d", "r9d", "r10d", "r11d",
                                 "r12d", "r13d", "r14d", "r15d"};
static const char* kReg16[16] = {"ax",  "cx",  "dx",  "bx",  "sp",  "bp",
                                 "si",  "di",  "r8w", "r9w", "r10w", "r11w",
                                 "r12w", "r13w", "r14w", "r15w"};
static const char* kReg8[16] = {"al",  "cl",  "dl",  "bl",  "spl", "bpl",
                                "sil", "dil", "r8b", "r9b", "r10b", "r11b",
                                "r12b", "r13b", "r14b", "r15b"};

struct Dec {
    const uint8_t* p;
    size_t len;
    size_t pos = 0;
    uint64_t addr;
    bool rex = false;
    bool rex_w = false;
    int rex_b = 0;
    int rex_x = 0;
    int rex_r = 0;
    bool opsz16 = false;   // 66 prefix
    bool addrsz32 = false; // 67 prefix
    bool ok = true;

    std::string out;

    uint8_t read() {
        if (pos >= len) { ok = false; return 0; }
        return p[pos++];
    }
    uint8_t peek(size_t off) const {
        if (pos + off >= len) return 0;
        return p[pos + off];
    }
};

const char* reg64(Dec& d, int idx) { return kReg64[((d.rex_r) << 3) | (idx & 7)]; }
const char* reg32(Dec& d, int idx) { return kReg32[((d.rex_r) << 3) | (idx & 7)]; }
const char* reg16(Dec& d, int idx) { return kReg16[((d.rex_r) << 3) | (idx & 7)]; }
const char* reg8(Dec& d, int idx) {
    // REX present -> spl/bpl/sil/dil; otherwise ah/ch/dh/bh
    if (d.rex) return kReg8[((d.rex_r) << 3) | (idx & 7)];
    static const char* kHigh[4] = {"ah", "ch", "dh", "bh"};
    if (idx >= 4) return kHigh[idx - 4];
    return kReg8[idx];
}
const char* rm64(Dec& d, int idx) { return kReg64[((d.rex_b) << 3) | (idx & 7)]; }
const char* rm32(Dec& d, int idx) { return kReg32[((d.rex_b) << 3) | (idx & 7)]; }
const char* rm16(Dec& d, int idx) { return kReg16[((d.rex_b) << 3) | (idx & 7)]; }
const char* rm8(Dec& d, int idx) {
    if (d.rex) return kReg8[((d.rex_b) << 3) | (idx & 7)];
    static const char* kHigh[4] = {"ah", "ch", "dh", "bh"};
    if (idx >= 4) return kHigh[idx - 4];
    return kReg8[idx];
}

// modrm addressing -> "[...]" string (Intel syntax).
std::string mem_expr(Dec& d, int mod, int rm, bool is64) {
    char buf[128];
    if (mod == 3) {
        return std::string(is64 ? rm64(d, rm) : rm32(d, rm));
    }
    // SIB
    if (rm == 4 && mod != 3) {
        uint8_t sib = d.read();
        int scale = 1 << ((sib >> 6) & 3);
        int index = (sib >> 3) & 7;
        int base = sib & 7;
        const char* index_name = ((d.rex_x) << 3 | index) == 4 ? nullptr
                                  : kReg64[((d.rex_x) << 3) | index];
        const char* base_name = nullptr;
        bool base_rip = false;
        int disp32 = 0;
        int64_t disp64 = 0;
        bool have_disp = false;
        if (base == 5 && mod == 0) {
            base_rip = true;  // RIP-relative
            have_disp = true;
            disp32 = static_cast<int32_t>(
                static_cast<uint32_t>(d.read()) |
                (static_cast<uint32_t>(d.read()) << 8) |
                (static_cast<uint32_t>(d.read()) << 16) |
                (static_cast<uint32_t>(d.read()) << 24));
        } else {
            base_name = kReg64[((d.rex_b) << 3) | base];
            if (mod == 1) {
                disp64 = static_cast<int8_t>(d.read());
                have_disp = true;
            } else if (mod == 2) {
                disp64 = static_cast<int32_t>(
                    static_cast<uint32_t>(d.read()) |
                    (static_cast<uint32_t>(d.read()) << 8) |
                    (static_cast<uint32_t>(d.read()) << 16) |
                    (static_cast<uint32_t>(d.read()) << 24));
                have_disp = true;
            }
        }
        std::string expr;
        if (base_rip) {
            expr = "rip";
        } else {
            expr = base_name;
            if (index_name) {
                if (scale == 1) expr += std::string("+") + index_name;
                else {
                    snprintf(buf, sizeof buf, "+%s*%d", index_name, scale);
                    expr += buf;
                }
            }
        }
        if (have_disp) {
            if (disp64 >= 0) {
                snprintf(buf, sizeof buf, "+0x%llx", (unsigned long long)disp64);
            } else {
                snprintf(buf, sizeof buf, "-0x%llx", (unsigned long long)(-disp64));
            }
            expr += buf;
        }
        return "[" + expr + "]";
    }
    // No SIB
    const char* rm_name = nullptr;
    int disp = 0;
    bool have_disp = false;
    if (rm == 5 && mod == 0) {
        disp = static_cast<int32_t>(
            static_cast<uint32_t>(d.read()) |
            (static_cast<uint32_t>(d.read()) << 8) |
            (static_cast<uint32_t>(d.read()) << 16) |
            (static_cast<uint32_t>(d.read()) << 24));
        have_disp = true;
        char tmp[64];
        snprintf(tmp, sizeof tmp, "rip+0x%x", disp);
        return std::string("[") + tmp + "]";
    }
    rm_name = is64 ? rm64(d, rm) : rm32(d, rm);
    if (mod == 1) {
        disp = static_cast<int8_t>(d.read());
        have_disp = true;
    } else if (mod == 2) {
        disp = static_cast<int32_t>(
            static_cast<uint32_t>(d.read()) |
            (static_cast<uint32_t>(d.read()) << 8) |
            (static_cast<uint32_t>(d.read()) << 16) |
            (static_cast<uint32_t>(d.read()) << 24));
        have_disp = true;
    }
    if (!have_disp) return std::string("[") + rm_name + "]";
    if (disp >= 0) {
        snprintf(buf, sizeof buf, "[%s+0x%x]", rm_name, disp);
    } else {
        snprintf(buf, sizeof buf, "[%s-0x%x]", rm_name, -disp);
    }
    return buf;
}

std::string imm_str(uint64_t v, bool show64) {
    char buf[48];
    if (show64) snprintf(buf, sizeof buf, "0x%llx", (unsigned long long)v);
    else snprintf(buf, sizeof buf, "0x%llx", (unsigned long long)(v & 0xFFFFFFFF));
    return buf;
}

const char* kJccRel8[16] = {"jo",  "jno", "jb",  "jae", "je",  "jne", "jbe", "ja",
                            "js",  "jns", "jp",  "jnp", "jl",  "jge", "jle", "jg"};
const char* kSetcc[16] = {"seto",  "setno", "setb",  "setae", "sete",  "setne",
                          "setbe", "seta",  "sets",  "setns", "setp",  "setnp",
                          "setl",  "setge", "setle", "setg"};

}  // namespace

bool disasm_one(const uint8_t* bytes, size_t max_len, uint64_t address,
                DecodedInsn& out) {
    Dec d;
    d.p = bytes;
    d.len = max_len;
    d.addr = address;

    // prefixes
    for (;;) {
        uint8_t b = d.peek(0);
        if (b >= 0x40 && b <= 0x4F) {
            d.read();
            d.rex = true;
            d.rex_w = (b & 8) != 0;
            d.rex_r = (b & 4) != 0;
            d.rex_x = (b & 2) != 0;
            d.rex_b = (b & 1) != 0;
            continue;
        }
        if (b == 0x66) { d.read(); d.opsz16 = true; continue; }
        if (b == 0x67) { d.read(); d.addrsz32 = true; continue; }
        if (b == 0xF2 || b == 0xF3) { d.read(); continue; }
        break;
    }

    auto modrm_expr = [&](bool is64) -> std::string {
        uint8_t m = d.read();
        int mod = (m >> 6) & 3;
        int rm = m & 7;
        if (mod == 3) {
            return std::string(is64 ? rm64(d, rm) : rm32(d, rm));
        }
        return mem_expr(d, mod, rm, is64);
    };

    char buf[160];
    std::string op;

    uint8_t opc = d.read();
    switch (opc) {
        case 0x90: op = "nop"; break;
        case 0xCC: op = "int3"; break;
        case 0xC3: op = "ret"; break;
        case 0xC9: op = "leave"; break;
        case 0x98: op = d.rex_w ? "cdqe" : "cwde"; break;
        case 0x99: op = d.rex_w ? "cqo" : "cdq"; break;
        case 0xF4: op = "hlt"; break;
        case 0xCD: {
            uint8_t n = d.read();
            snprintf(buf, sizeof buf, "int 0x%x", n);
            op = buf;
            break;
        }
        case 0x0F: {
            uint8_t op2 = d.read();
            if (op2 == 0x05) { op = "syscall"; break; }
            if (op2 == 0x31) { op = "rdtsc"; break; }
            if (op2 == 0x1F) {
                // multi-byte nop
                if (modrm_expr(d.rex_w) == "rax") {}
                op = "nop";
                break;
            }
            if (op2 >= 0x80 && op2 <= 0x8F) {
                uint8_t m = d.read();
                int mod = (m >> 6) & 3;
                int rm = m & 7;
                std::string dst = (mod == 3) ? std::string(rm32(d, rm)) : mem_expr(d, mod, rm, false);
                int32_t rel = static_cast<int32_t>(
                    static_cast<uint32_t>(d.read()) |
                    (static_cast<uint32_t>(d.read()) << 8) |
                    (static_cast<uint32_t>(d.read()) << 16) |
                    (static_cast<uint32_t>(d.read()) << 24));
                uint64_t target = d.addr + d.pos + rel;
                snprintf(buf, sizeof buf, "%s 0x%llx", kJccRel8[op2 & 0xF],
                         (unsigned long long)target);
                op = buf;
                break;
            }
            if (op2 >= 0x90 && op2 <= 0x9F) {
                uint8_t m = d.read();
                int mod = (m >> 6) & 3;
                int rm = m & 7;
                std::string dst = (mod == 3) ? std::string(rm8(d, rm)) : mem_expr(d, mod, rm, false);
                snprintf(buf, sizeof buf, "%s %s", kSetcc[op2 & 0xF], dst.c_str());
                op = buf;
                break;
            }
            if (op2 == 0xB6 || op2 == 0xB7 || op2 == 0xBE || op2 == 0xBF) {
                uint8_t m = d.read();
                int mod = (m >> 6) & 3;
                int reg = (m >> 3) & 7;
                int rm = m & 7;
                bool ext = (op2 == 0xBE || op2 == 0xBF);
                bool wide = (op2 == 0xB7 || op2 == 0xBF);
                std::string dst = std::string(d.rex_w ? "movsxd" : (ext ? "movsx" : "movzx"));
                const char* regnm = d.rex_w ? reg64(d, reg) : reg32(d, reg);
                std::string src = (mod == 3) ? std::string(wide ? rm16(d, rm) : rm8(d, rm))
                                             : mem_expr(d, mod, rm, false);
                snprintf(buf, sizeof buf, "%s %s, %s", dst.c_str(), regnm, src.c_str());
                op = buf;
                break;
            }
            if (op2 == 0xAF) {
                uint8_t m = d.read();
                int mod = (m >> 6) & 3;
                int reg = (m >> 3) & 7;
                int rm = m & 7;
                std::string src = (mod == 3) ? std::string(rm64(d, rm)) : mem_expr(d, mod, rm, true);
                snprintf(buf, sizeof buf, "imul %s, %s", reg64(d, reg), src.c_str());
                op = buf;
                break;
            }
            snprintf(buf, sizeof buf, "db 0x0f 0x%02x", op2);
            op = buf;
            break;
        }
        case 0x50: case 0x51: case 0x52: case 0x53:
        case 0x54: case 0x55: case 0x56: case 0x57:
            snprintf(buf, sizeof buf, "push %s", reg64(d, opc & 7));
            op = buf;
            break;
        case 0x58: case 0x59: case 0x5A: case 0x5B:
        case 0x5C: case 0x5D: case 0x5E: case 0x5F:
            snprintf(buf, sizeof buf, "pop %s", reg64(d, opc & 7));
            op = buf;
            break;
        case 0x68: {
            uint32_t v = static_cast<uint32_t>(d.read()) |
                         (static_cast<uint32_t>(d.read()) << 8) |
                         (static_cast<uint32_t>(d.read()) << 16) |
                         (static_cast<uint32_t>(d.read()) << 24);
            snprintf(buf, sizeof buf, "push 0x%x", v);
            op = buf;
            break;
        }
        case 0x6A: {
            int8_t v = static_cast<int8_t>(d.read());
            snprintf(buf, sizeof buf, "push 0x%x", v & 0xFF);
            op = buf;
            break;
        }
        case 0x70: case 0x71: case 0x72: case 0x73:
        case 0x74: case 0x75: case 0x76: case 0x77:
        case 0x78: case 0x79: case 0x7A: case 0x7B:
        case 0x7C: case 0x7D: case 0x7E: case 0x7F: {
            int8_t rel = static_cast<int8_t>(d.read());
            uint64_t target = d.addr + d.pos + rel;
            snprintf(buf, sizeof buf, "%s 0x%llX", kJccRel8[opc & 0xF],
                     (unsigned long long)target);
            op = buf;
            break;
        }
        case 0xEB: {
            int8_t rel = static_cast<int8_t>(d.read());
            snprintf(buf, sizeof buf, "jmp 0x%llx",
                     (unsigned long long)(d.addr + d.pos + rel));
            op = buf;
            break;
        }
        case 0xE9: {
            int32_t rel = static_cast<int32_t>(
                static_cast<uint32_t>(d.read()) |
                (static_cast<uint32_t>(d.read()) << 8) |
                (static_cast<uint32_t>(d.read()) << 16) |
                (static_cast<uint32_t>(d.read()) << 24));
            snprintf(buf, sizeof buf, "jmp 0x%llx",
                     (unsigned long long)(d.addr + d.pos + rel));
            op = buf;
            break;
        }
        case 0xE8: {
            int32_t rel = static_cast<int32_t>(
                static_cast<uint32_t>(d.read()) |
                (static_cast<uint32_t>(d.read()) << 8) |
                (static_cast<uint32_t>(d.read()) << 16) |
                (static_cast<uint32_t>(d.read()) << 24));
            snprintf(buf, sizeof buf, "call 0x%llx",
                     (unsigned long long)(d.addr + d.pos + rel));
            op = buf;
            break;
        }
        case 0xC2: {
            uint16_t n = static_cast<uint16_t>(d.read() | (d.read() << 8));
            snprintf(buf, sizeof buf, "ret 0x%x", n);
            op = buf;
            break;
        }
        case 0x8D: {
            uint8_t m = d.read();
            int mod = (m >> 6) & 3;
            int reg = (m >> 3) & 7;
            int rm = m & 7;
            std::string src = (mod == 3) ? std::string(rm64(d, rm)) : mem_expr(d, mod, rm, true);
            snprintf(buf, sizeof buf, "lea %s, %s", reg64(d, reg), src.c_str());
            op = buf;
            break;
        }
        case 0x8B: case 0x8A: case 0x89: case 0x88: {
            uint8_t m = d.read();
            int mod = (m >> 6) & 3;
            int reg = (m >> 3) & 7;
            int rm = m & 7;
            bool to_reg = (opc == 0x8B || opc == 0x8A);
            bool byte = (opc == 0x8A || opc == 0x88);
            bool wide = d.rex_w;
            std::string dst, src;
            if (to_reg) {
                dst = byte ? std::string(reg8(d, reg))
                           : (wide ? reg64(d, reg) : reg32(d, reg));
                src = (mod == 3) ? std::string(byte ? rm8(d, rm) : (wide ? rm64(d, rm) : rm32(d, rm)))
                                 : mem_expr(d, mod, rm, wide);
            } else {
                src = byte ? std::string(reg8(d, reg))
                           : (wide ? reg64(d, reg) : reg32(d, reg));
                dst = (mod == 3) ? std::string(byte ? rm8(d, rm) : (wide ? rm64(d, rm) : rm32(d, rm)))
                                 : mem_expr(d, mod, rm, wide);
            }
            snprintf(buf, sizeof buf, "mov %s, %s", dst.c_str(), src.c_str());
            op = buf;
            break;
        }
        case 0x63: {
            uint8_t m = d.read();
            int mod = (m >> 6) & 3;
            int reg = (m >> 3) & 7;
            int rm = m & 7;
            std::string src = (mod == 3) ? std::string(rm32(d, rm)) : mem_expr(d, mod, rm, false);
            snprintf(buf, sizeof buf, "movsxd %s, %s", reg64(d, reg), src.c_str());
            op = buf;
            break;
        }
        case 0xC6: {
            uint8_t m = d.read();
            int mod = (m >> 6) & 3;
            int rm = m & 7;
            std::string dst = (mod == 3) ? std::string(rm8(d, rm)) : mem_expr(d, mod, rm, false);
            uint8_t v = d.read();
            snprintf(buf, sizeof buf, "mov %s, 0x%x", dst.c_str(), v);
            op = buf;
            break;
        }
        case 0xC7: {
            uint8_t m = d.read();
            int mod = (m >> 6) & 3;
            int rm = m & 7;
            std::string dst = (mod == 3) ? std::string(rm32(d, rm)) : mem_expr(d, mod, rm, false);
            uint32_t v = static_cast<uint32_t>(d.read()) |
                         (static_cast<uint32_t>(d.read()) << 8) |
                         (static_cast<uint32_t>(d.read()) << 16) |
                         (static_cast<uint32_t>(d.read()) << 24);
            snprintf(buf, sizeof buf, "mov %s, 0x%x", dst.c_str(), v);
            op = buf;
            break;
        }
        case 0xB0: case 0xB1: case 0xB2: case 0xB3:
        case 0xB4: case 0xB5: case 0xB6: case 0xB7:
        case 0xB8: case 0xB9: case 0xBA: case 0xBB:
        case 0xBC: case 0xBD: case 0xBE: case 0xBF: {
            bool byte = opc <= 0xB7;
            int reg = opc & 7;
            if (byte) {
                uint8_t v = d.read();
                snprintf(buf, sizeof buf, "mov %s, 0x%x", reg8(d, reg), v);
            } else if (d.rex_w) {
                uint64_t v = 0;
                for (int i = 0; i < 8; ++i) v |= static_cast<uint64_t>(d.read()) << (8 * i);
                snprintf(buf, sizeof buf, "mov %s, 0x%llx", reg64(d, reg),
                         (unsigned long long)v);
            } else {
                uint32_t v = static_cast<uint32_t>(d.read()) |
                             (static_cast<uint32_t>(d.read()) << 8) |
                             (static_cast<uint32_t>(d.read()) << 16) |
                             (static_cast<uint32_t>(d.read()) << 24);
                snprintf(buf, sizeof buf, "mov %s, 0x%x", reg32(d, reg), v);
            }
            op = buf;
            break;
        }
        default: {
            // arithmetic group: 80-83 (imm), 00-3D variants, FF/FE, F6/F7, C0/C1/D0-D3
            static const char* kArith[8] = {"add", "or", "adc", "sbb", "and", "sub", "xor", "cmp"};
            // Arithmetic register/memory forms: group*8 + {0..3}
            //   +0: op r/m8, r8 ; +1: op r/m32/64, r32/64
            //   +2: op r8, r/m8 ; +3: op r32/64, r/m32/64
            if (opc < 0x40 && (opc & 7) < 4) {
                int group = (opc >> 3) & 7;  // 0=add 1=or 2=adc 3=sbb 4=and 5=sub 6=xor 7=cmp
                bool to_rm = (opc & 3) < 2;  // op r/m, reg
                bool byte = (opc & 1) == 0;  // +0/+2 are byte forms
                bool wide = d.rex_w;
                uint8_t m = d.read();
                int mod = (m >> 6) & 3;
                int reg = (m >> 3) & 7;
                int rm = m & 7;
                std::string a, b;
                if (to_rm) {
                    a = (mod == 3) ? std::string(byte ? rm8(d, rm) : (wide ? rm64(d, rm) : rm32(d, rm)))
                                   : mem_expr(d, mod, rm, wide);
                    b = byte ? std::string(reg8(d, reg))
                             : (wide ? reg64(d, reg) : reg32(d, reg));
                } else {
                    a = byte ? std::string(reg8(d, reg))
                             : (wide ? reg64(d, reg) : reg32(d, reg));
                    b = (mod == 3) ? std::string(byte ? rm8(d, rm) : (wide ? rm64(d, rm) : rm32(d, rm)))
                                   : mem_expr(d, mod, rm, wide);
                }
                snprintf(buf, sizeof buf, "%s %s, %s", kArith[group], a.c_str(), b.c_str());
                op = buf;
                break;
            }
            if (opc >= 0x80 && opc <= 0x83) {
                bool byte = (opc == 0x80);
                bool sign_ext8 = (opc == 0x83);
                int group = -1;
                uint8_t m = d.read();
                int mod = (m >> 6) & 3;
                int reg = (m >> 3) & 7;
                int rm = m & 7;
                group = reg;
                std::string dst = (mod == 3) ? std::string(byte ? rm8(d, rm) : (d.rex_w ? rm64(d, rm) : rm32(d, rm)))
                                             : mem_expr(d, mod, rm, d.rex_w);
                uint64_t v;
                if (byte) v = d.read();
                else if (sign_ext8) v = static_cast<int8_t>(d.read());
                else {
                    v = static_cast<uint32_t>(d.read()) |
                        (static_cast<uint32_t>(d.read()) << 8) |
                        (static_cast<uint32_t>(d.read()) << 16) |
                        (static_cast<uint32_t>(d.read()) << 24);
                }
                snprintf(buf, sizeof buf, "%s %s, 0x%llx", kArith[group], dst.c_str(),
                         (unsigned long long)(v & 0xFF));
                op = buf;
                break;
            }
            if (opc == 0xFE || opc == 0xFF) {
                uint8_t m = d.read();
                int mod = (m >> 6) & 3;
                int reg = (m >> 3) & 7;
                int rm = m & 7;
                std::string dst = (mod == 3) ? std::string(rm64(d, rm)) : mem_expr(d, mod, rm, true);
                switch (reg) {
                    case 0: op = std::string("inc ") + (opc == 0xFE ? dst : dst); break;
                    case 1: op = std::string("dec ") + dst; break;
                    case 2: op = "call " + dst; break;
                    case 4: op = "jmp " + dst; break;
                    case 6: op = "push " + dst; break;
                    default:
                        snprintf(buf, sizeof buf, "db 0x%02x", opc);
                        op = buf;
                }
                break;
            }
            if (opc == 0xF6 || opc == 0xF7) {
                uint8_t m = d.read();
                int mod = (m >> 6) & 3;
                int reg = (m >> 3) & 7;
                int rm = m & 7;
                bool byte = (opc == 0xF6);
                std::string dst = (mod == 3) ? std::string(byte ? rm8(d, rm) : (d.rex_w ? rm64(d, rm) : rm32(d, rm)))
                                             : mem_expr(d, mod, rm, d.rex_w);
                switch (reg) {
                    case 0: {
                        uint64_t v = byte ? d.read() : (static_cast<uint32_t>(d.read()) |
                            (static_cast<uint32_t>(d.read()) << 8) |
                            (static_cast<uint32_t>(d.read()) << 16) |
                            (static_cast<uint32_t>(d.read()) << 24));
                        snprintf(buf, sizeof buf, "test %s, 0x%llx", dst.c_str(),
                                 (unsigned long long)v);
                        op = buf;
                        break;
                    }
                    case 2: op = "not " + dst; break;
                    case 3: op = "neg " + dst; break;
                    case 4: op = "mul " + dst; break;
                    case 5: op = "imul " + dst; break;
                    default:
                        snprintf(buf, sizeof buf, "db 0x%02x", opc);
                        op = buf;
                }
                break;
            }
            if (opc == 0xC0 || opc == 0xC1 || opc == 0xD0 || opc == 0xD1 ||
                opc == 0xD2 || opc == 0xD3) {
                static const char* kShift[8] = {"rol", "ror", "rcl", "rcr",
                                                "shl", "shr", "sal", "sar"};
                uint8_t m = d.read();
                int mod = (m >> 6) & 3;
                int reg = (m >> 3) & 7;
                int rm = m & 7;
                bool byte = (opc == 0xC0 || opc == 0xD0 || opc == 0xD2);
                std::string dst = (mod == 3) ? std::string(byte ? rm8(d, rm) : (d.rex_w ? rm64(d, rm) : rm32(d, rm)))
                                             : mem_expr(d, mod, rm, d.rex_w);
                std::string cnt;
                if (opc == 0xC0 || opc == 0xC1) {
                    uint8_t v = d.read();
                    snprintf(buf, sizeof buf, "%s %s, 0x%x", kShift[reg], dst.c_str(), v);
                    op = buf;
                } else if (opc == 0xD2 || opc == 0xD3) {
                    snprintf(buf, sizeof buf, "%s %s, cl", kShift[reg], dst.c_str());
                    op = buf;
                } else {
                    snprintf(buf, sizeof buf, "%s %s, 1", kShift[reg], dst.c_str());
                    op = buf;
                }
                break;
            }
            if (opc == 0x86 || opc == 0x87) {
                uint8_t m = d.read();
                int mod = (m >> 6) & 3;
                int reg = (m >> 3) & 7;
                int rm = m & 7;
                bool byte = (opc == 0x86);
                std::string a = byte ? std::string(reg8(d, reg))
                                     : (d.rex_w ? reg64(d, reg) : reg32(d, reg));
                std::string b = (mod == 3) ? std::string(byte ? rm8(d, rm) : (d.rex_w ? rm64(d, rm) : rm32(d, rm)))
                                           : mem_expr(d, mod, rm, d.rex_w);
                snprintf(buf, sizeof buf, "xchg %s, %s", a.c_str(), b.c_str());
                op = buf;
                break;
            }
            if (opc == 0x91 || opc == 0x92 || opc == 0x93 || opc == 0x94 ||
                opc == 0x95 || opc == 0x96 || opc == 0x97) {
                snprintf(buf, sizeof buf, "xchg %s, rax", reg64(d, opc & 7));
                op = buf;
                break;
            }
            if (opc == 0x69 || opc == 0x6B) {
                uint8_t m = d.read();
                int mod = (m >> 6) & 3;
                int reg = (m >> 3) & 7;
                int rm = m & 7;
                std::string src = (mod == 3) ? std::string(rm64(d, rm)) : mem_expr(d, mod, rm, true);
                uint64_t v = (opc == 0x6B) ? static_cast<int8_t>(d.read())
                                           : (static_cast<uint32_t>(d.read()) |
                                              (static_cast<uint32_t>(d.read()) << 8) |
                                              (static_cast<uint32_t>(d.read()) << 16) |
                                              (static_cast<uint32_t>(d.read()) << 24));
                snprintf(buf, sizeof buf, "imul %s, %s, 0x%llx", reg64(d, reg), src.c_str(),
                         (unsigned long long)v);
                op = buf;
                break;
            }
            snprintf(buf, sizeof buf, "db 0x%02x", opc);
            op = buf;
        }
    }

    if (!d.ok) return false;
    out.length = static_cast<uint8_t>(d.pos);
    out.text = op;
    return true;
}

}  // namespace pmem::internal

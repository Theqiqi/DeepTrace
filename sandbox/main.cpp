// sandbox: 验证 Capstone 反汇编库的 vcpkg 接入与基础调用
// 验证目标:
//   1) capstone 能否在 Windows+MSVC+vcpkg 环境编译链接并运行
//   2) 基础反汇编调用(cs_open/cs_disasm)输出是否符合预期(断言)
//   3) 自研引擎不支持的指令(SSE/SSE2/REP 字符串)capstone 能否正确解码
#include <capstone/capstone.h>

#include <cstdio>
#include <string>
#include <vector>

struct Case {
    const char* name;
    std::vector<uint8_t> bytes;
    const char* expect;  // 期望的 "mnemonic op_str";nullptr = 仅打印不断言
};

int main() {
    std::printf("capstone header version: %u.%u.%u\n", CS_VERSION_MAJOR, CS_VERSION_MINOR, CS_VERSION_EXTRA);
    int major = 0, minor = 0;
    unsigned rv = cs_version(&major, &minor);
    std::printf("capstone runtime version: %d.%d (raw 0x%x)\n", major, minor, rv);

    // 诊断:各架构 cs_open 是否成功(排查 x86 是否被编译禁用)
    struct { cs_arch a; cs_mode m; const char* n; } try_arch[] = {
        {CS_ARCH_X86, CS_MODE_64, "x86/64"},
        {CS_ARCH_ARM, CS_MODE_ARM, "arm"},
        {CS_ARCH_ARM64, CS_MODE_ARM, "arm64"},
        {CS_ARCH_RISCV, CS_MODE_RISCV64, "riscv64"},
    };
    for (auto& t : try_arch) {
        csh h2;
        cs_err e = cs_open(t.a, t.m, &h2);
        std::printf("cs_open(%-8s) = %d (%s)\n", t.n, e, cs_strerror(e));
        if (e == CS_ERR_OK) cs_close(&h2);
    }

    csh handle;
    cs_err err = cs_open(CS_ARCH_X86, CS_MODE_64, &handle);
    if (err != CS_ERR_OK) {
        std::printf("FAIL: cs_open err=%d (%s)\n", err, cs_strerror(err));
        return 1;
    }
    cs_option(handle, CS_OPT_SYNTAX, CS_OPT_SYNTAX_INTEL);

    const std::vector<Case> cases = {
        // ---- 自研引擎支持的基础指令(对照 disasm_test.cpp) ----
        {"mov rax, rbx", {0x48, 0x89, 0xD8}, "mov rax, rbx"},
        {"mov rax, imm64",
         {0x48, 0xB8, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08},
         "movabs rax, 0x807060504030201"},
        {"call rel32", {0xE8, 0x00, 0x00, 0x00, 0x00}, nullptr},
        {"lea rax, [rbp+8]", {0x48, 0x8D, 0x45, 0x08},
         "lea rax, [rbp + 8]"},
        {"add rax, 5", {0x48, 0x83, 0xC0, 0x05}, "add rax, 5"},
        {"jmp $-2 (loop)", {0xEB, 0xFE}, nullptr},
        {"syscall", {0x0F, 0x05}, "syscall"},
        // ---- 自研引擎不支持的指令(能力缺口) ----
        {"movups xmm0,[rax] (SSE)", {0x0F, 0x10, 0x00},
         "movups xmm0, xmmword ptr [rax]"},
        {"pxor xmm0,xmm0 (SSE2)", {0x66, 0x0F, 0xEF, 0xC0},
         "pxor xmm0, xmm0"},
        {"cvtsi2sd xmm0,rax (SSE2)", {0xF2, 0x48, 0x0F, 0x2A, 0xC0},
         "cvtsi2sd xmm0, rax"},
        {"rep movsb (string)", {0xF3, 0xA4},
         "rep movsb byte ptr [rdi], byte ptr [rsi]"},
        {"movdqu xmm0,[rax] (SSE2)", {0xF3, 0x0F, 0x6F, 0x00},
         "movdqu xmm0, xmmword ptr [rax]"},
    };

    int pass = 0, fail = 0;
    for (const auto& c : cases) {
        cs_insn* insn = nullptr;
        size_t n = cs_disasm(handle, c.bytes.data(), c.bytes.size(),
                             0x140001000, 1, &insn);
        if (n == 0) {
            std::printf("[%s] FAIL: 无法解码  bytes=", c.name);
            for (uint8_t b : c.bytes) std::printf("%02X ", b);
            std::printf("\n");
            ++fail;
            continue;
        }
        std::string got = std::string(insn->mnemonic) + " " + insn->op_str;
        while (!got.empty() && got.back() == ' ') got.pop_back();  // 去尾随空格
        std::printf("[%s] %s  (len=%u)\n", c.name, got.c_str(), insn->size);
        if (c.expect) {
            if (got == c.expect) {
                ++pass;
            } else {
                std::printf("    期望: %s\n", c.expect);
                ++fail;
            }
        }
        cs_free(insn, n);
    }
    cs_close(&handle);
    std::printf("\n结果: 断言通过 %d, 失败 %d\n", pass, fail);
    return fail == 0 ? 0 : 1;
}

#include "deeptrace.h"
#include "target_util.h"

#include <gtest/gtest.h>

#include <chrono>
#include <cstring>
#include <thread>

namespace {

testutil::TargetInfo g_target;

// Global environment: spawns the target once before all tests and kills it
// afterwards. Registered during static init, so gtest's own main() picks it up.
class TargetEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        if (!testutil::spawn_target(testutil::target_exe_path(), g_target)) {
            fprintf(stderr, "FATAL: could not spawn deeptrace_target.exe\n");
            _exit(2);
        }
    }
    void TearDown() override { testutil::kill_target(g_target.pid); }
};

::testing::Environment* const g_env =
    ::testing::AddGlobalTestEnvironment(new TargetEnvironment());

class DeeptraceIntegration : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_NE(g_target.pid, 0u) << "target not spawned";
        ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    }
    void TearDown() override { deeptrace::detach(); }
};

}  // namespace

TEST(DeeptraceProcess, Enumerate) {
    std::vector<deeptrace::ProcessInfo> procs;
    EXPECT_EQ(deeptrace::enumerate_processes(procs), deeptrace::Result::Ok);
    bool found = false;
    for (const auto& p : procs) {
        if (p.pid == g_target.pid) {
            found = true;
            EXPECT_EQ(p.name, L"deeptrace_target.exe");
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(DeeptraceProcess, Info) {
    deeptrace::ProcessInfo info;
    EXPECT_EQ(deeptrace::process_info(g_target.pid, info), deeptrace::Result::Ok);
    EXPECT_EQ(info.pid, g_target.pid);
}

TEST(DeeptraceProcess, InfoInvalidPid) {
    deeptrace::ProcessInfo info;
    EXPECT_EQ(deeptrace::process_info(0xFFFFFFFF, info), deeptrace::Result::NoSuchProcess);
}

TEST_F(DeeptraceIntegration, SessionPid) {
    uint32_t pid = 0;
    EXPECT_EQ(deeptrace::session_pid(&pid), deeptrace::Result::Ok);
    EXPECT_EQ(pid, g_target.pid);
}

TEST_F(DeeptraceIntegration, ReadKnownInt) {
    uint32_t v = 0;
    size_t n = 0;
    EXPECT_EQ(deeptrace::memory_read(g_target.g_int, &v, 4, &n), deeptrace::Result::Ok);
    EXPECT_EQ(n, 4u);
    EXPECT_EQ(v, 0x11223344u);
}

TEST_F(DeeptraceIntegration, ReadKnownInt64) {
    uint64_t v = 0;
    size_t n = 0;
    EXPECT_EQ(deeptrace::memory_read(g_target.g_int64, &v, 8, &n), deeptrace::Result::Ok);
    EXPECT_EQ(v, 0x1122334455667788ULL);
}

TEST_F(DeeptraceIntegration, ReadFault) {
    uint8_t b = 0;
    size_t n = 0;
    EXPECT_EQ(deeptrace::memory_read(0x1, &b, 1, &n), deeptrace::Result::ReadFault);
}

TEST_F(DeeptraceIntegration, WriteKnownInt) {
    uint32_t v = 0xCAFEBABE;
    size_t n = 0;
    EXPECT_EQ(deeptrace::memory_write(g_target.g_int, &v, 4, &n), deeptrace::Result::Ok);
    EXPECT_EQ(n, 4u);
    uint32_t back = 0;
    size_t rn = 0;
    EXPECT_EQ(deeptrace::memory_read(g_target.g_int, &back, 4, &rn), deeptrace::Result::Ok);
    EXPECT_EQ(back, 0xCAFEBABEu);
    // restore
    v = 0x11223344;
    deeptrace::memory_write(g_target.g_int, &v, 4, &n);
}

TEST_F(DeeptraceIntegration, Dump) {
    std::vector<uint8_t> out;
    EXPECT_EQ(deeptrace::memory_dump(g_target.g_bytes, 4, out), deeptrace::Result::Ok);
    ASSERT_EQ(out.size(), 4u);
    EXPECT_EQ(out[0], 0xDE);
    EXPECT_EQ(out[1], 0xAD);
    EXPECT_EQ(out[2], 0xBE);
    EXPECT_EQ(out[3], 0xEF);
}

TEST_F(DeeptraceIntegration, Regions) {
    std::vector<deeptrace::MemoryRegion> regions;
    EXPECT_EQ(deeptrace::memory_regions(regions), deeptrace::Result::Ok);
    EXPECT_FALSE(regions.empty());
    // the module image should be listed with our base
    bool found = false;
    for (const auto& r : regions) {
        if (r.base <= g_target.g_int && g_target.g_int < r.base + r.size) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(DeeptraceIntegration, Readval) {
    std::string text;
    EXPECT_EQ(deeptrace::memory_readval(g_target.g_int, deeptrace::ValueType::Dword, text),
              deeptrace::Result::Ok);
    EXPECT_EQ(text, "0x11223344");
    EXPECT_EQ(deeptrace::memory_readval(g_target.g_double, deeptrace::ValueType::Double, text),
              deeptrace::Result::Ok);
    EXPECT_EQ(text, "2.71828");
}

TEST_F(DeeptraceIntegration, Modules) {
    std::vector<deeptrace::ModuleInfo> mods;
    EXPECT_EQ(deeptrace::module_list(mods), deeptrace::Result::Ok);
    bool found = false;
    for (const auto& m : mods) {
        if (m.name == L"deeptrace_target.exe") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(DeeptraceIntegration, ModuleBase) {
    uintptr_t base = 0;
    EXPECT_EQ(deeptrace::module_base("deeptrace_target.exe", &base), deeptrace::Result::Ok);
    EXPECT_NE(base, 0u);
}

TEST_F(DeeptraceIntegration, ModuleExports) {
    // deeptrace_target exports none, but kernel32 does
    std::vector<deeptrace::ExportInfo> exps;
    EXPECT_EQ(deeptrace::module_exports("kernel32.dll", exps), deeptrace::Result::Ok);
    EXPECT_FALSE(exps.empty());
}

TEST_F(DeeptraceIntegration, Threads) {
    std::vector<deeptrace::ThreadInfo> threads;
    EXPECT_EQ(deeptrace::thread_list(threads), deeptrace::Result::Ok);
    EXPECT_GE(threads.size(), 1u);
    bool found_worker = false;
    for (const auto& t : threads) {
        if (t.tid == g_target.worker_tid) found_worker = true;
    }
    EXPECT_TRUE(found_worker);
}

TEST_F(DeeptraceIntegration, PatternScan) {
    std::vector<uintptr_t> hits;
    EXPECT_EQ(deeptrace::pattern_scan("DE AD BE EF", hits), deeptrace::Result::Ok);
    bool found = false;
    for (auto h : hits) {
        if (h == g_target.g_bytes) found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(DeeptraceIntegration, PatternScanBadFormat) {
    std::vector<uintptr_t> hits;
    EXPECT_EQ(deeptrace::pattern_scan("DE AD ZZ", hits), deeptrace::Result::BadFormat);
}

TEST_F(DeeptraceIntegration, Registers) {
    std::vector<deeptrace::RegisterInfo> regs;
    EXPECT_EQ(deeptrace::registers_get(regs, g_target.worker_tid), deeptrace::Result::Ok);
    bool has_rip = false, has_rax = false;
    for (const auto& r : regs) {
        if (r.name == "rip") has_rip = true;
        if (r.name == "rax") has_rax = true;
    }
    EXPECT_TRUE(has_rip);
    EXPECT_TRUE(has_rax);
}

TEST_F(DeeptraceIntegration, RegisterGet) {
    uint64_t v = 0;
    EXPECT_EQ(deeptrace::register_get("rip", &v, g_target.worker_tid), deeptrace::Result::Ok);
    EXPECT_NE(v, 0u);
    EXPECT_EQ(deeptrace::register_get("zzz", &v, g_target.worker_tid),
              deeptrace::Result::NotFound);
}

TEST_F(DeeptraceIntegration, DisasmAt) {
    std::vector<deeptrace::Instruction> insns;
    EXPECT_EQ(deeptrace::disasm_at(g_target.g_bytes, 4, insns), deeptrace::Result::Ok);
    ASSERT_GE(insns.size(), 1u);
    EXPECT_EQ(insns[0].address, g_target.g_bytes);
}

TEST_F(DeeptraceIntegration, DisasmRange) {
    std::vector<deeptrace::Instruction> insns;
    EXPECT_EQ(deeptrace::disasm_range(g_target.g_bytes, g_target.g_bytes + 8, insns),
              deeptrace::Result::Ok);
    EXPECT_FALSE(insns.empty());
}

TEST_F(DeeptraceIntegration, BreakpointRoundTrip) {
    deeptrace::BreakpointInfo bp;
    EXPECT_EQ(deeptrace::breakpoint_set(g_target.g_int, bp), deeptrace::Result::Ok);
    EXPECT_EQ(bp.address, g_target.g_int);
    uint8_t cur = 0;
    size_t n = 0;
    deeptrace::memory_read(g_target.g_int, &cur, 1, &n);
    EXPECT_EQ(cur, 0xCC);
    EXPECT_EQ(deeptrace::breakpoint_clear(g_target.g_int), deeptrace::Result::Ok);
    deeptrace::memory_read(g_target.g_int, &cur, 1, &n);
    EXPECT_EQ(cur, 0x44);  // original low byte of 0x11223344
}

TEST_F(DeeptraceIntegration, BreakpointDuplicate) {
    deeptrace::BreakpointInfo bp;
    EXPECT_EQ(deeptrace::breakpoint_set(g_target.g_int, bp), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::breakpoint_set(g_target.g_int, bp), deeptrace::Result::AlreadyExists);
    EXPECT_EQ(deeptrace::breakpoint_clear(g_target.g_int), deeptrace::Result::Ok);
}

TEST_F(DeeptraceIntegration, DebugStatus) {
    deeptrace::DebugStatus st;
    EXPECT_EQ(deeptrace::debug_status(st), deeptrace::Result::Ok);
    EXPECT_TRUE(st.attached);
}

TEST_F(DeeptraceIntegration, WatchRoundTrip) {
    EXPECT_EQ(deeptrace::watch_clear(), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::watch_add("test_int", g_target.g_int, deeptrace::ValueType::Dword),
              deeptrace::Result::Ok);
    std::vector<deeptrace::WatchEntry> entries;
    EXPECT_EQ(deeptrace::watch_refresh(entries), deeptrace::Result::Ok);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].description, "test_int");
    EXPECT_EQ(entries[0].value, "0x11223344");
    EXPECT_EQ(deeptrace::watch_remove(0), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::watch_list(entries), deeptrace::Result::Ok);
    EXPECT_TRUE(entries.empty());
}

// watch list must show live values (previously it printed an empty/misleading
// VALID column because it never read the target memory).
TEST_F(DeeptraceIntegration, WatchListReadsValues) {
    EXPECT_EQ(deeptrace::watch_clear(), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::watch_add("live_int", g_target.g_int, deeptrace::ValueType::Dword),
              deeptrace::Result::Ok);
    std::vector<deeptrace::WatchEntry> entries;
    EXPECT_EQ(deeptrace::watch_list(entries), deeptrace::Result::Ok);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_TRUE(entries[0].valid);
    EXPECT_EQ(entries[0].value, "0x11223344");
    EXPECT_EQ(deeptrace::watch_remove(0), deeptrace::Result::Ok);
}

// debug_continue: set a software breakpoint on the busy worker loop (worker_fn
// executes constantly) and run to it. The hit is consumed (INT3 restored and
// re-armed) and reported in ContinueInfo.
TEST_F(DeeptraceIntegration, DebugContinueBreakpointHit) {
    EXPECT_EQ(deeptrace::debug_attach(), deeptrace::Result::Ok);
    // worker_tick runs on every worker iteration, so the INT3 fires quickly.
    deeptrace::BreakpointInfo bp;
    EXPECT_EQ(deeptrace::breakpoint_set(g_target.worker_tick, bp),
              deeptrace::Result::Ok);

    deeptrace::ContinueInfo info;
    EXPECT_EQ(deeptrace::debug_continue(5000, info), deeptrace::Result::Ok);
    EXPECT_TRUE(info.hit);
    EXPECT_EQ(info.exception, 0x80000003u);  // EXCEPTION_BREAKPOINT
    EXPECT_EQ(info.address, g_target.worker_tick);
    EXPECT_NE(info.tid, 0u);
    EXPECT_NE(info.rip, 0u);

    EXPECT_EQ(deeptrace::breakpoint_clear(g_target.worker_tick),
              deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::debug_detach(), deeptrace::Result::Ok);
}

// debug_continue without an attached debugger must fail cleanly.
TEST_F(DeeptraceIntegration, DebugContinueNotAttached) {
    deeptrace::ContinueInfo info;
    EXPECT_EQ(deeptrace::debug_continue(100, info), deeptrace::Result::NotAttached);
}

// Debugger attach must not kill the target: detach() pairs
// DebugActiveProcessStop before closing the handle (the CLI auto-detaches
// after every command, so a missing pairing crashed the target).
TEST_F(DeeptraceIntegration, DebugAttachTargetSurvives) {
    EXPECT_EQ(deeptrace::debug_attach(), deeptrace::Result::Ok);
    deeptrace::ProcessInfo info;
    // still running while the debugger is attached
    EXPECT_EQ(deeptrace::process_info(g_target.pid, info), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::debug_detach(), deeptrace::Result::Ok);
    // and still running after detach (debug_mode already cleared)
    EXPECT_EQ(deeptrace::process_info(g_target.pid, info), deeptrace::Result::Ok);
}

// Regression: "add/sub/... reg, imm" used to fail with BadFormat although the
// mnemonic was declared supported (only mov was actually implemented).
TEST_F(DeeptraceIntegration, AsmAluAssemble) {
    std::vector<uint8_t> bytes;
    EXPECT_EQ(deeptrace::asm_assemble("add rax, 0", bytes, nullptr), deeptrace::Result::Ok);
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x83);
    EXPECT_EQ(bytes[2], 0xC0);
    EXPECT_EQ(bytes[3], 0x00);

    EXPECT_EQ(deeptrace::asm_assemble("xor eax, eax; cmp rax, rbx; ret", bytes, nullptr),
              deeptrace::Result::Ok);
    ASSERT_EQ(bytes.size(), 6u);
    EXPECT_EQ(bytes[0], 0x31);
    EXPECT_EQ(bytes[1], 0xC0);
    EXPECT_EQ(bytes[2], 0x48);
    EXPECT_EQ(bytes[3], 0x39);
    EXPECT_EQ(bytes[4], 0xD8);
    EXPECT_EQ(bytes[5], 0xC3);
}

// Companion-resource test: testdll.dll is built next to the test exe and
// must survive a full inject -> list -> eject round trip in the target.
TEST_F(DeeptraceIntegration, DllInjectRoundTrip) {
    std::string dll = testutil::test_dll_path();
    ASSERT_TRUE(testutil::file_exists(dll)) << "testdll.dll not built next to test";

    deeptrace::InjectInfo info;
    EXPECT_EQ(deeptrace::dll_inject(dll, info), deeptrace::Result::Ok);
    EXPECT_EQ(info.kind, "dll");
    EXPECT_NE(info.remote_base, 0u);

    std::vector<deeptrace::InjectInfo> list;
    EXPECT_EQ(deeptrace::dll_list(list), deeptrace::Result::Ok);
    bool found = false;
    for (const auto& i : list) {
        if (i.remote_base == info.remote_base) found = true;
    }
    EXPECT_TRUE(found);

    EXPECT_EQ(deeptrace::dll_eject(dll), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::dll_list(list), deeptrace::Result::Ok);
    bool still = false;
    for (const auto& i : list) {
        if (i.remote_base == info.remote_base) still = true;
    }
    EXPECT_FALSE(still);
}

// Shellcode needs no companion file: { 0xC3 } (ret) runs to completion and
// the record must be visible via status.
TEST_F(DeeptraceIntegration, ShellcodeRoundTrip) {
    std::vector<uint8_t> code = {0xC3};
    deeptrace::InjectInfo info;
    EXPECT_EQ(deeptrace::shellcode_inject(code, info), deeptrace::Result::Ok);
    EXPECT_EQ(info.kind, "shellcode");
    EXPECT_NE(info.remote_base, 0u);

    std::vector<deeptrace::InjectInfo> list;
    EXPECT_EQ(deeptrace::shellcode_status(list), deeptrace::Result::Ok);
    bool found = false;
    for (const auto& i : list) {
        if (i.remote_base == info.remote_base) found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(DeeptraceIntegration, AsmAssemble) {
    std::vector<uint8_t> bytes;
    std::string text;
    EXPECT_EQ(deeptrace::asm_assemble("nop; nop; ret", bytes, &text), deeptrace::Result::Ok);
    ASSERT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 0x90);
    EXPECT_EQ(bytes[1], 0x90);
    EXPECT_EQ(bytes[2], 0xC3);
    EXPECT_EQ(text, "9090C3");
}

TEST_F(DeeptraceIntegration, AsmAssembleBad) {
    std::vector<uint8_t> bytes;
    EXPECT_EQ(deeptrace::asm_assemble("bogus rax, 1", bytes, nullptr),
              deeptrace::Result::BadFormat);
}

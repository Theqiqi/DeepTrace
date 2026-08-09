#include "pmem.h"
#include "target_util.h"

#include <gtest/gtest.h>

#include <cstring>

namespace {

testutil::TargetInfo g_target;

// Global environment: spawns the target once before all tests and kills it
// afterwards. Registered during static init, so gtest's own main() picks it up.
class TargetEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        if (!testutil::spawn_target(testutil::target_exe_path(), g_target)) {
            fprintf(stderr, "FATAL: could not spawn pmem_target.exe\n");
            _exit(2);
        }
    }
    void TearDown() override { testutil::kill_target(g_target.pid); }
};

::testing::Environment* const g_env =
    ::testing::AddGlobalTestEnvironment(new TargetEnvironment());

class PmemIntegration : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_NE(g_target.pid, 0u) << "target not spawned";
        ASSERT_EQ(pmem::attach(g_target.pid), pmem::Result::Ok);
    }
    void TearDown() override { pmem::detach(); }
};

}  // namespace

TEST(PmemProcess, Enumerate) {
    std::vector<pmem::ProcessInfo> procs;
    EXPECT_EQ(pmem::enumerate_processes(procs), pmem::Result::Ok);
    bool found = false;
    for (const auto& p : procs) {
        if (p.pid == g_target.pid) {
            found = true;
            EXPECT_EQ(p.name, L"pmem_target.exe");
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(PmemProcess, Info) {
    pmem::ProcessInfo info;
    EXPECT_EQ(pmem::process_info(g_target.pid, info), pmem::Result::Ok);
    EXPECT_EQ(info.pid, g_target.pid);
}

TEST(PmemProcess, InfoInvalidPid) {
    pmem::ProcessInfo info;
    EXPECT_EQ(pmem::process_info(0xFFFFFFFF, info), pmem::Result::NoSuchProcess);
}

TEST_F(PmemIntegration, SessionPid) {
    uint32_t pid = 0;
    EXPECT_EQ(pmem::session_pid(&pid), pmem::Result::Ok);
    EXPECT_EQ(pid, g_target.pid);
}

TEST_F(PmemIntegration, ReadKnownInt) {
    uint32_t v = 0;
    size_t n = 0;
    EXPECT_EQ(pmem::memory_read(g_target.g_int, &v, 4, &n), pmem::Result::Ok);
    EXPECT_EQ(n, 4u);
    EXPECT_EQ(v, 0x11223344u);
}

TEST_F(PmemIntegration, ReadKnownInt64) {
    uint64_t v = 0;
    size_t n = 0;
    EXPECT_EQ(pmem::memory_read(g_target.g_int64, &v, 8, &n), pmem::Result::Ok);
    EXPECT_EQ(v, 0x1122334455667788ULL);
}

TEST_F(PmemIntegration, ReadFault) {
    uint8_t b = 0;
    size_t n = 0;
    EXPECT_EQ(pmem::memory_read(0x1, &b, 1, &n), pmem::Result::ReadFault);
}

TEST_F(PmemIntegration, WriteKnownInt) {
    uint32_t v = 0xCAFEBABE;
    size_t n = 0;
    EXPECT_EQ(pmem::memory_write(g_target.g_int, &v, 4, &n), pmem::Result::Ok);
    EXPECT_EQ(n, 4u);
    uint32_t back = 0;
    size_t rn = 0;
    EXPECT_EQ(pmem::memory_read(g_target.g_int, &back, 4, &rn), pmem::Result::Ok);
    EXPECT_EQ(back, 0xCAFEBABEu);
    // restore
    v = 0x11223344;
    pmem::memory_write(g_target.g_int, &v, 4, &n);
}

TEST_F(PmemIntegration, Dump) {
    std::vector<uint8_t> out;
    EXPECT_EQ(pmem::memory_dump(g_target.g_bytes, 4, out), pmem::Result::Ok);
    ASSERT_EQ(out.size(), 4u);
    EXPECT_EQ(out[0], 0xDE);
    EXPECT_EQ(out[1], 0xAD);
    EXPECT_EQ(out[2], 0xBE);
    EXPECT_EQ(out[3], 0xEF);
}

TEST_F(PmemIntegration, Regions) {
    std::vector<pmem::MemoryRegion> regions;
    EXPECT_EQ(pmem::memory_regions(regions), pmem::Result::Ok);
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

TEST_F(PmemIntegration, Readval) {
    std::string text;
    EXPECT_EQ(pmem::memory_readval(g_target.g_int, pmem::ValueType::Dword, text),
              pmem::Result::Ok);
    EXPECT_EQ(text, "0x11223344");
    EXPECT_EQ(pmem::memory_readval(g_target.g_double, pmem::ValueType::Double, text),
              pmem::Result::Ok);
    EXPECT_EQ(text, "2.71828");
}

TEST_F(PmemIntegration, Modules) {
    std::vector<pmem::ModuleInfo> mods;
    EXPECT_EQ(pmem::module_list(mods), pmem::Result::Ok);
    bool found = false;
    for (const auto& m : mods) {
        if (m.name == L"pmem_target.exe") {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST_F(PmemIntegration, ModuleBase) {
    uintptr_t base = 0;
    EXPECT_EQ(pmem::module_base("pmem_target.exe", &base), pmem::Result::Ok);
    EXPECT_NE(base, 0u);
}

TEST_F(PmemIntegration, ModuleExports) {
    // pmem_target exports none, but kernel32 does
    std::vector<pmem::ExportInfo> exps;
    EXPECT_EQ(pmem::module_exports("kernel32.dll", exps), pmem::Result::Ok);
    EXPECT_FALSE(exps.empty());
}

TEST_F(PmemIntegration, Threads) {
    std::vector<pmem::ThreadInfo> threads;
    EXPECT_EQ(pmem::thread_list(threads), pmem::Result::Ok);
    EXPECT_GE(threads.size(), 1u);
    bool found_worker = false;
    for (const auto& t : threads) {
        if (t.tid == g_target.worker_tid) found_worker = true;
    }
    EXPECT_TRUE(found_worker);
}

TEST_F(PmemIntegration, PatternScan) {
    std::vector<uintptr_t> hits;
    EXPECT_EQ(pmem::pattern_scan("DE AD BE EF", hits), pmem::Result::Ok);
    bool found = false;
    for (auto h : hits) {
        if (h == g_target.g_bytes) found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(PmemIntegration, PatternScanBadFormat) {
    std::vector<uintptr_t> hits;
    EXPECT_EQ(pmem::pattern_scan("DE AD ZZ", hits), pmem::Result::BadFormat);
}

TEST_F(PmemIntegration, Registers) {
    std::vector<pmem::RegisterInfo> regs;
    EXPECT_EQ(pmem::registers_get(regs, g_target.worker_tid), pmem::Result::Ok);
    bool has_rip = false, has_rax = false;
    for (const auto& r : regs) {
        if (r.name == "rip") has_rip = true;
        if (r.name == "rax") has_rax = true;
    }
    EXPECT_TRUE(has_rip);
    EXPECT_TRUE(has_rax);
}

TEST_F(PmemIntegration, RegisterGet) {
    uint64_t v = 0;
    EXPECT_EQ(pmem::register_get("rip", &v, g_target.worker_tid), pmem::Result::Ok);
    EXPECT_NE(v, 0u);
    EXPECT_EQ(pmem::register_get("zzz", &v, g_target.worker_tid),
              pmem::Result::NotFound);
}

TEST_F(PmemIntegration, DisasmAt) {
    std::vector<pmem::Instruction> insns;
    EXPECT_EQ(pmem::disasm_at(g_target.g_bytes, 4, insns), pmem::Result::Ok);
    ASSERT_GE(insns.size(), 1u);
    EXPECT_EQ(insns[0].address, g_target.g_bytes);
}

TEST_F(PmemIntegration, DisasmRange) {
    std::vector<pmem::Instruction> insns;
    EXPECT_EQ(pmem::disasm_range(g_target.g_bytes, g_target.g_bytes + 8, insns),
              pmem::Result::Ok);
    EXPECT_FALSE(insns.empty());
}

TEST_F(PmemIntegration, BreakpointRoundTrip) {
    pmem::BreakpointInfo bp;
    EXPECT_EQ(pmem::breakpoint_set(g_target.g_int, bp), pmem::Result::Ok);
    EXPECT_EQ(bp.address, g_target.g_int);
    uint8_t cur = 0;
    size_t n = 0;
    pmem::memory_read(g_target.g_int, &cur, 1, &n);
    EXPECT_EQ(cur, 0xCC);
    EXPECT_EQ(pmem::breakpoint_clear(g_target.g_int), pmem::Result::Ok);
    pmem::memory_read(g_target.g_int, &cur, 1, &n);
    EXPECT_EQ(cur, 0x44);  // original low byte of 0x11223344
}

TEST_F(PmemIntegration, BreakpointDuplicate) {
    pmem::BreakpointInfo bp;
    EXPECT_EQ(pmem::breakpoint_set(g_target.g_int, bp), pmem::Result::Ok);
    EXPECT_EQ(pmem::breakpoint_set(g_target.g_int, bp), pmem::Result::AlreadyExists);
    EXPECT_EQ(pmem::breakpoint_clear(g_target.g_int), pmem::Result::Ok);
}

TEST_F(PmemIntegration, DebugStatus) {
    pmem::DebugStatus st;
    EXPECT_EQ(pmem::debug_status(st), pmem::Result::Ok);
    EXPECT_TRUE(st.attached);
}

TEST_F(PmemIntegration, WatchRoundTrip) {
    EXPECT_EQ(pmem::watch_clear(), pmem::Result::Ok);
    EXPECT_EQ(pmem::watch_add("test_int", g_target.g_int, pmem::ValueType::Dword),
              pmem::Result::Ok);
    std::vector<pmem::WatchEntry> entries;
    EXPECT_EQ(pmem::watch_refresh(entries), pmem::Result::Ok);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_EQ(entries[0].description, "test_int");
    EXPECT_EQ(entries[0].value, "0x11223344");
    EXPECT_EQ(pmem::watch_remove(0), pmem::Result::Ok);
    EXPECT_EQ(pmem::watch_list(entries), pmem::Result::Ok);
    EXPECT_TRUE(entries.empty());
}

// watch list must show live values (previously it printed an empty/misleading
// VALID column because it never read the target memory).
TEST_F(PmemIntegration, WatchListReadsValues) {
    EXPECT_EQ(pmem::watch_clear(), pmem::Result::Ok);
    EXPECT_EQ(pmem::watch_add("live_int", g_target.g_int, pmem::ValueType::Dword),
              pmem::Result::Ok);
    std::vector<pmem::WatchEntry> entries;
    EXPECT_EQ(pmem::watch_list(entries), pmem::Result::Ok);
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_TRUE(entries[0].valid);
    EXPECT_EQ(entries[0].value, "0x11223344");
    EXPECT_EQ(pmem::watch_remove(0), pmem::Result::Ok);
}

// Debugger attach must not kill the target: detach() pairs
// DebugActiveProcessStop before closing the handle (the CLI auto-detaches
// after every command, so a missing pairing crashed the target).
TEST_F(PmemIntegration, DebugAttachTargetSurvives) {
    EXPECT_EQ(pmem::debug_attach(), pmem::Result::Ok);
    pmem::ProcessInfo info;
    // still running while the debugger is attached
    EXPECT_EQ(pmem::process_info(g_target.pid, info), pmem::Result::Ok);
    EXPECT_EQ(pmem::debug_detach(), pmem::Result::Ok);
    // and still running after detach (debug_mode already cleared)
    EXPECT_EQ(pmem::process_info(g_target.pid, info), pmem::Result::Ok);
}

// Regression: "add/sub/... reg, imm" used to fail with BadFormat although the
// mnemonic was declared supported (only mov was actually implemented).
TEST_F(PmemIntegration, AsmAluAssemble) {
    std::vector<uint8_t> bytes;
    EXPECT_EQ(pmem::asm_assemble("add rax, 0", bytes, nullptr), pmem::Result::Ok);
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], 0x48);
    EXPECT_EQ(bytes[1], 0x83);
    EXPECT_EQ(bytes[2], 0xC0);
    EXPECT_EQ(bytes[3], 0x00);

    EXPECT_EQ(pmem::asm_assemble("xor eax, eax; cmp rax, rbx; ret", bytes, nullptr),
              pmem::Result::Ok);
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
TEST_F(PmemIntegration, DllInjectRoundTrip) {
    std::string dll = testutil::test_dll_path();
    ASSERT_TRUE(testutil::file_exists(dll)) << "testdll.dll not built next to test";

    pmem::InjectInfo info;
    EXPECT_EQ(pmem::dll_inject(dll, info), pmem::Result::Ok);
    EXPECT_EQ(info.kind, "dll");
    EXPECT_NE(info.remote_base, 0u);

    std::vector<pmem::InjectInfo> list;
    EXPECT_EQ(pmem::dll_list(list), pmem::Result::Ok);
    bool found = false;
    for (const auto& i : list) {
        if (i.remote_base == info.remote_base) found = true;
    }
    EXPECT_TRUE(found);

    EXPECT_EQ(pmem::dll_eject(dll), pmem::Result::Ok);
    EXPECT_EQ(pmem::dll_list(list), pmem::Result::Ok);
    bool still = false;
    for (const auto& i : list) {
        if (i.remote_base == info.remote_base) still = true;
    }
    EXPECT_FALSE(still);
}

// Shellcode needs no companion file: { 0xC3 } (ret) runs to completion and
// the record must be visible via status.
TEST_F(PmemIntegration, ShellcodeRoundTrip) {
    std::vector<uint8_t> code = {0xC3};
    pmem::InjectInfo info;
    EXPECT_EQ(pmem::shellcode_inject(code, info), pmem::Result::Ok);
    EXPECT_EQ(info.kind, "shellcode");
    EXPECT_NE(info.remote_base, 0u);

    std::vector<pmem::InjectInfo> list;
    EXPECT_EQ(pmem::shellcode_status(list), pmem::Result::Ok);
    bool found = false;
    for (const auto& i : list) {
        if (i.remote_base == info.remote_base) found = true;
    }
    EXPECT_TRUE(found);
}

TEST_F(PmemIntegration, AsmAssemble) {
    std::vector<uint8_t> bytes;
    std::string text;
    EXPECT_EQ(pmem::asm_assemble("nop; nop; ret", bytes, &text), pmem::Result::Ok);
    ASSERT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 0x90);
    EXPECT_EQ(bytes[1], 0x90);
    EXPECT_EQ(bytes[2], 0xC3);
    EXPECT_EQ(text, "9090C3");
}

TEST_F(PmemIntegration, AsmAssembleBad) {
    std::vector<uint8_t> bytes;
    EXPECT_EQ(pmem::asm_assemble("bogus rax, 1", bytes, nullptr),
              pmem::Result::BadFormat);
}

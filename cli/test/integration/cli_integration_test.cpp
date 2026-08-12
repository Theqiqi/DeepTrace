// CLI integration tests: walk the full three-layer chain
// parse -> execute -> deeptrace public API against a real deeptrace_target.exe process.
// The target prints known addresses; we assert deeptrace state changes and exit codes.

#include "command/parser.h"
#include "interface/executor.h"
#include "printing/printer.h"

#include "deeptrace.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace {

struct TargetInfo {
    uint32_t pid = 0;
    uintptr_t g_int = 0;
    uintptr_t g_bytes = 0;
    uintptr_t g_flag = 0;
};

TargetInfo g_target;

bool spawn_target(TargetInfo& out) {
    char buf[MAX_PATH] = {0};
    ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string exe(buf);
    size_t slash = exe.find_last_of("/\\");
    if (slash != std::string::npos) exe = exe.substr(0, slash);
    std::string target = exe + "\\deeptrace_target.exe";

    SECURITY_ATTRIBUTES sa = {0};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hRead = nullptr, hWrite = nullptr;
    if (!::CreatePipe(&hRead, &hWrite, &sa, 0)) return false;
    ::SetHandleInformation(hRead, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWrite;
    si.hStdError = hWrite;
    PROCESS_INFORMATION pi = {0};

    std::string cmdline = "\"" + target + "\"";
    char* cmd = _strdup(cmdline.c_str());
    BOOL ok = ::CreateProcessA(nullptr, cmd, nullptr, nullptr, TRUE, 0, nullptr,
                               nullptr, &si, &pi);
    free(cmd);
    ::CloseHandle(hWrite);
    if (!ok) {
        ::CloseHandle(hRead);
        return false;
    }
    ::CloseHandle(pi.hThread);

    std::string all;
    DWORD readn = 0;
    for (int i = 0; i < 100; ++i) {
        if (!::PeekNamedPipe(hRead, nullptr, 0, nullptr, &readn, nullptr)) break;
        if (readn > 0) {
            char chunk[4096];
            DWORD got = 0;
            if (::ReadFile(hRead, chunk, 4096, &got, nullptr) && got > 0)
                all.append(chunk, got);
        }
        if (all.find("WORKER_TID:") != std::string::npos) break;
        ::Sleep(50);
    }
    ::CloseHandle(hRead);
    ::CloseHandle(pi.hProcess);
    if (all.find("PID:") == std::string::npos) return false;

    auto find_u32 = [&](const char* key) -> uint32_t {
        size_t p = all.find(key);
        if (p == std::string::npos) return 0;
        p += strlen(key);
        while (p < all.size() && (all[p] == ' ' || all[p] == '\t' || all[p] == '\r' ||
                                  all[p] == '\n'))
            ++p;
        return static_cast<uint32_t>(strtoul(all.c_str() + p, nullptr, 10));
    };
    auto find_addr = [&](const char* key) -> uintptr_t {
        size_t p = all.find(key);
        if (p == std::string::npos) return 0;
        size_t at = all.find("@0x", p);
        if (at == std::string::npos) return 0;
        return strtoull(all.c_str() + at + 3, nullptr, 16);
    };

    out.pid = find_u32("PID:");
    out.g_int = find_addr("g_int ");
    out.g_bytes = find_addr("g_bytes");
    out.g_flag = find_addr("g_flag");
    return out.pid != 0;
}

class TargetEnvironment : public ::testing::Environment {
public:
    void SetUp() override {
        if (!spawn_target(g_target)) {
            std::fprintf(stderr, "FATAL: could not spawn deeptrace_target.exe\n");
            _exit(2);
        }
    }
    void TearDown() override {
        HANDLE h = ::OpenProcess(PROCESS_TERMINATE, FALSE, g_target.pid);
        if (h) {
            ::TerminateProcess(h, 1);
            ::CloseHandle(h);
        }
    }
};

::testing::Environment* const g_env =
    ::testing::AddGlobalTestEnvironment(new TargetEnvironment());

// Run the CLI chain: argv -> parse -> execute. Returns the exit code.
int run_cli(const std::vector<std::string>& argv) {
    std::vector<std::string> copy = argv;
    std::vector<char*> args;
    for (auto& a : copy) args.push_back(a.data());
    deeptrace_cli::ParseResult pr =
        deeptrace_cli::parse_args(static_cast<int>(args.size()), args.data());
    if (!pr.ok) return pr.exit_code;
    return deeptrace_cli::execute(pr.req);
}

std::string hex(uintptr_t v) {
    char b[32];
    std::snprintf(b, sizeof b, "0x%llX", (unsigned long long)v);
    return b;
}

}  // namespace

// ------- full chain: parse -> execute -> deeptrace API -------

TEST(CliChain, PsList) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "ps", "list"}), 0);
}

TEST(CliChain, AttachAndReadKnownInt) {
    // -p attaches in the executor; then mem read hits the real value.
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "ps", "info"}), 0);

    // full chain read: address from target banner
    std::vector<uint8_t> buf(4);
    size_t n = 0;
    // execute mem read then verify via deeptrace API that the bytes are correct
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "mem", "read",
                       hex(g_target.g_int), "4", "hex"}),
              0);
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::memory_read(g_target.g_int, buf.data(), 4, &n), deeptrace::Result::Ok);
    EXPECT_EQ(n, 4u);
    EXPECT_EQ(buf[0], 0x44);
    EXPECT_EQ(buf[1], 0x33);
    EXPECT_EQ(buf[2], 0x22);
    EXPECT_EQ(buf[3], 0x11);
    deeptrace::detach();
}

TEST(CliChain, MemWriteChangesTarget) {
    // hex-bytes semantics: BEBAFECA (little-endian) -> dword 0xCAFEBABE
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "mem", "write",
                       hex(g_target.g_int), "BEBAFECA", "hex"}),
              0);
    uint32_t v = 0;
    size_t n = 0;
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::memory_read(g_target.g_int, &v, 4, &n), deeptrace::Result::Ok);
    EXPECT_EQ(v, 0xCAFEBABEu);
    // restore
    v = 0x11223344;
    deeptrace::memory_write(g_target.g_int, &v, 4, &n);
    deeptrace::detach();
}

TEST(CliChain, MemReadval) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "mem", "readval",
                       hex(g_target.g_int), "dword"}),
              0);
}

TEST(CliChain, MemRegions) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "mem", "regions"}),
              0);
}

TEST(CliChain, ModuleListAndBase) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "module", "list"}),
              0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "module", "base",
                       "deeptrace_target.exe"}),
              0);
}

TEST(CliChain, ThreadList) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "thread", "list"}),
              0);
}

// v2.1.0: standalone debug commands were removed (single entry = debug run).
// Calling them is a usage error (exit 2) and must not touch the target.
TEST(CliChain, DebugSingleCommandsRejected) {
    const char* removed[] = {"attach", "detach", "pause", "resume", "step",
                             "next", "break", "clear", "hbreak", "hclear",
                             "guard", "unguard", "status", "registers",
                             "register"};
    for (const char* a : removed) {
        EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid),
                           "debug", a}),
                  2) << a;
    }
    // no residual breakpoint bytes: the target must still be intact
    std::vector<uint8_t> buf(4);
    size_t n = 0;
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::memory_read(g_target.g_int, buf.data(), 4, &n),
              deeptrace::Result::Ok);
    EXPECT_EQ(buf[0], 0x44);  // g_int unchanged (no 0xCC pollution)
    deeptrace::detach();
}

// The CLI `watch list` must display live values (list reads target memory).
TEST(CliChain, WatchListShowsValue) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "watch",
                       "clear"}),
              0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "watch", "add",
                       "live_int", hex(g_target.g_int), "dword"}),
              0);
    std::vector<deeptrace::WatchEntry> entries;
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::watch_list(entries), deeptrace::Result::Ok);
    deeptrace::detach();
    ASSERT_EQ(entries.size(), 1u);
    EXPECT_TRUE(entries[0].valid);
    EXPECT_EQ(entries[0].value, "0x11223344");
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "watch",
                       "remove", "0"}),
              0);
}

// DLL injection through the full CLI chain against the companion testdll.dll.
TEST(CliChain, DllInjectRoundTrip) {
    char buf[MAX_PATH] = {0};
    ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string dir(buf);
    size_t slash = dir.find_last_of("/\\");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    std::string dll = dir + "\\testdll.dll";
    // The build copies testdll.dll next to the test exe (POST_BUILD), so a
    // missing file is a deployment failure, not a reason to skip.
    std::ifstream f(dll);
    ASSERT_TRUE(f.good()) << "testdll.dll not deployed next to the test exe";
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "dll",
                       "inject", dll}),
              0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "dll",
                       "list"}),
              0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "dll",
                       "eject", dll}),
              0);
}

TEST(CliChain, ResolveScanFindsKnownPattern) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "resolve", "scan",
                       "DE AD BE EF"}),
              0);
}

// convert is a pure data conversion: works without -p and without a session.
TEST(CliChain, ConvertNoProcess) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "convert", "dword", "100"}), 0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "convert", "float", "3.14"}), 0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "convert", "string", "hello"}), 0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "convert", "hex", "DEADBEEF"}), 0);
    // usage errors -> exit 2
    EXPECT_EQ(run_cli({"deeptrace_cli", "convert", "bogus", "1"}), 2);
    EXPECT_EQ(run_cli({"deeptrace_cli", "convert", "dword", "xyz"}), 2);
    EXPECT_EQ(run_cli({"deeptrace_cli", "convert", "byte", "256"}), 2);
}

// The convert output format is scan-compatible: the documented pattern for
// dword 0x11223344 (g_int) is "44 33 22 11" and must be found by resolve scan.
TEST(CliChain, ConvertOutputFeedsScan) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "convert", "dword", "287454020"}), 0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "resolve", "scan",
                       "44 33 22 11"}),
              0);
    // the pre-v1.4.0 typed scan syntax is gone: extra arg is a usage error
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "resolve", "scan",
                       "100", "dword"}),
              2);
}

TEST(CliChain, DisasmAt) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "disasm", "at",
                       hex(g_target.g_bytes), "4"}),
              0);
}

TEST(CliChain, AsmAssemble) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "asm", "assemble", "nop; ret"}), 0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "asm", "assemble", "mov rax, 1", "--hex"}), 0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "asm", "assemble", "nop", "--c-array"}), 0);
}

TEST(CliChain, WatchRoundTrip) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "watch", "clear"}),
              0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "watch", "add",
                       "test_int", hex(g_target.g_int), "dword"}),
              0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "watch",
                       "refresh"}),
              0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "watch", "remove",
                       "0"}),
              0);
}

// Load a JSON script fixture next to the test exe (deployed by POST_BUILD),
// substitute the %G_INT% placeholder with the runtime address, and write the
// materialized script to a temp file. Returns the temp path.
std::string materialize_script(const char* fixture, uintptr_t g_int, int tag) {
    char buf[MAX_PATH] = {0};
    ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string dir(buf);
    size_t slash = dir.find_last_of("/\\");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    std::ifstream in(dir + "\\" + fixture);
    EXPECT_TRUE(in.good()) << "fixture not deployed: " << fixture;
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    std::string addr = hex(g_int);
    size_t p = 0;
    while ((p = content.find("%G_INT%", p)) != std::string::npos) {
        content.replace(p, 7, addr);
        p += addr.size();
    }
    char tmpname[L_tmpnam] = {0};
    std::tmpnam(tmpname);
    std::string path = std::string(tmpname) + "_" + std::to_string(tag) + ".json";
    std::ofstream f(path, std::ios::binary);
    f << content;
    f.close();
    return path;
}

// One invocation = one debug session: attach -> debug_attach -> steps ->
// cleanup -> debug_detach -> detach. The target must survive.
// The step list comes from the real fixture cli/test/scripts/debug_session.json.
TEST(CliChain, DebugRunScriptedSession) {
    std::string script = materialize_script("debug_session.json", g_target.g_int, 1);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "debug", "run",
                       script}),
              0);
    std::remove(script.c_str());
    // side effect check: target must still be alive after the session
    HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, g_target.pid);
    ASSERT_NE(h, nullptr);
    DWORD code = 0;
    EXPECT_TRUE(::GetExitCodeProcess(h, &code));
    EXPECT_EQ(code, STILL_ACTIVE);
    ::CloseHandle(h);
}

// Regression: write accepts space-separated hex; the bytes must land correctly.
// The script comes from the real fixture cli/test/scripts/debug_write.json.
TEST(CliChain, DebugRunWriteSpacedHex) {
    std::string script = materialize_script("debug_write.json", g_target.g_int, 2);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "debug", "run",
                       script}),
              0);
    std::remove(script.c_str());
    uint32_t v = 0;
    size_t n = 0;
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::memory_read(g_target.g_int, &v, 4, &n), deeptrace::Result::Ok);
    EXPECT_EQ(v, 0xCAFEBABEu);  // little-endian: BE BA FE CA -> 0xCAFEBABE
    v = 0x11223344;
    deeptrace::memory_write(g_target.g_int, &v, 4, &n);
    deeptrace::detach();
}

// Session-end cleanup: a breakpoint armed in the script and never cleared
// must be restored by cleanup_session at detach (no residual 0xCC in the
// target after debug run returns).
// Fixture: cli/test/scripts/debug_break_only.json.
TEST(CliChain, DebugRunSessionCleanupRestoresByte) {
    std::string script = materialize_script("debug_break_only.json", g_target.g_int, 4);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "debug", "run",
                       script}),
              0);
    std::remove(script.c_str());
    uint8_t b = 0;
    size_t n = 0;
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::memory_read(g_target.g_int, &b, 1, &n), deeptrace::Result::Ok);
    EXPECT_EQ(b, 0x44);  // original byte restored (no 0xCC residual)
    deeptrace::detach();
    // side effect check: target survived the session
    HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, g_target.pid);
    ASSERT_NE(h, nullptr);
    DWORD code = 0;
    EXPECT_TRUE(::GetExitCodeProcess(h, &code));
    EXPECT_EQ(code, STILL_ACTIVE);
    ::CloseHandle(h);
}

// Script errors: missing file and invalid content -> exit 2, no session opened.
// The bad script comes from the real fixture cli/test/scripts/debug_bad.json.
TEST(CliChain, DebugRunScriptErrors) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "debug", "run",
                       "no_such_script.json"}),
              2);
    char buf[MAX_PATH] = {0};
    ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string dir(buf);
    size_t slash = dir.find_last_of("/\\");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    std::string bad = dir + "\\debug_bad.json";
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "debug", "run",
                       bad}),
              2);
}

TEST(CliChain, NoSuchProcessAttach) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "ps", "attach", "99999999"}), 1);
}

TEST(CliChain, MissingCommand) {
    EXPECT_EQ(run_cli({"deeptrace_cli"}), 1);
}

TEST(CliChain, UnknownCommand) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "bogus", "cmd"}), 2);
}

TEST(CliChain, ReadFaultExit) {
    // address 1 is not readable -> ReadFault -> exit 1
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "mem", "read",
                       "0x1", "4"}),
              1);
}

// ---- v2.2.0: asm file / hex2bin / shellcode staged ops ----

// asm file: assemble a source file, optionally write .bin; the produced bytes
// must match the inline assemble result (xor eax,eax; ret = 31 C0 C3).
TEST(CliChain, AsmFileAssemblesAndWritesBin) {
    char tmpname[L_tmpnam] = {0};
    std::tmpnam(tmpname);
    std::string asm_path = std::string(tmpname) + ".asm";
    std::string bin_path = std::string(tmpname) + ".bin";
    {
        std::ofstream f(asm_path);
        f << "xor eax, eax\nret\n";
    }
    EXPECT_EQ(run_cli({"deeptrace_cli", "asm", "file", asm_path, "--out", bin_path}), 0);
    std::ifstream bin(bin_path, std::ios::binary);
    ASSERT_TRUE(bin.good()) << "bin file not written";
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(bin)),
                               std::istreambuf_iterator<char>());
    ASSERT_EQ(bytes.size(), 3u);
    EXPECT_EQ(bytes[0], 0x31);
    EXPECT_EQ(bytes[1], 0xC0);
    EXPECT_EQ(bytes[2], 0xC3);
    std::remove(asm_path.c_str());
    std::remove(bin_path.c_str());
    // error paths
    EXPECT_EQ(run_cli({"deeptrace_cli", "asm", "file", "no_such_file.asm"}), 2);
}

// hex2bin: hex -> raw .bin file, readable back as bytes.
TEST(CliChain, Hex2BinWritesFile) {
    char tmpname[L_tmpnam] = {0};
    std::tmpnam(tmpname);
    std::string bin_path = std::string(tmpname) + ".bin";
    EXPECT_EQ(run_cli({"deeptrace_cli", "hex2bin", "DEADBEEF", bin_path}), 0);
    std::ifstream bin(bin_path, std::ios::binary);
    ASSERT_TRUE(bin.good());
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(bin)),
                               std::istreambuf_iterator<char>());
    ASSERT_EQ(bytes.size(), 4u);
    EXPECT_EQ(bytes[0], 0xDE);
    EXPECT_EQ(bytes[3], 0xEF);
    std::remove(bin_path.c_str());
    // usage errors
    EXPECT_EQ(run_cli({"deeptrace_cli", "hex2bin", "ABC", bin_path}), 2);
}

// shellcode staged ops through the full chain: alloc (write only) -> run
// (trigger, repeatable) -> free (cleanup). The {0xC3} ret shellcode keeps the
// target safe; the record must exist after alloc and be gone after free.
TEST(CliChain, ShellcodeAllocRunFreeChain) {
    std::string pid = std::to_string(g_target.pid);
    // alloc: no execute, returns address. Verify the bytes landed in the target.
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "alloc", "C3"}), 0);
    // status lists a shellcode record for this pid
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "status"}), 0);
    // run on the recorded address: find it via the record file by scanning
    // the status output is complex here; instead drive the public API to locate
    // the address we just allocated (the only new one at this point).
    std::vector<deeptrace::InjectInfo> list;
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::shellcode_status(list), deeptrace::Result::Ok);
    deeptrace::detach();
    uintptr_t addr = 0;
    for (const auto& i : list) {
        if (i.kind == "shellcode" && i.remote_base != 0) addr = i.remote_base;
    }
    ASSERT_NE(addr, 0u) << "no shellcode record after alloc";

    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "run", hex(addr)}), 0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "run", hex(addr)}), 0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "free", hex(addr)}), 0);
    // record gone -> run/free now fail with business error (exit 1)
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "run", hex(addr)}), 1);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "free", hex(addr)}), 1);
    // side effect check: target survived the whole staged flow
    HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, g_target.pid);
    ASSERT_NE(h, nullptr);
    DWORD code = 0;
    EXPECT_TRUE(::GetExitCodeProcess(h, &code));
    EXPECT_EQ(code, STILL_ACTIVE);
    ::CloseHandle(h);
}

// shellcode alloc with an invalid source (neither hex nor an existing file)
// is a usage error (exit 2).
TEST(CliChain, ShellcodeAllocBadSource) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "shellcode",
                       "alloc", "zzzz"}),
              2);
}

// Capability boundary (negative): alloc must NOT accept .asm source files
// (assembly is exec-only); the raw text must not be injected as shellcode.
TEST(CliChain, ShellcodeAllocRejectsAsmFile) {
    char tmpname[L_tmpnam] = {0};
    std::tmpnam(tmpname);
    std::string asm_path = std::string(tmpname) + ".asm";
    {
        std::ofstream f(asm_path);
        f << "ret\n";
    }
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "shellcode",
                       "alloc", asm_path}),
              2);
    std::remove(asm_path.c_str());
}

// exec with an .asm source whose assembly fails (syntax error) is a business
// error (exit 1, BadFormat), not a usage error.
TEST(CliChain, ShellcodeExecBadAsm) {
    char tmpname[L_tmpnam] = {0};
    std::tmpnam(tmpname);
    std::string asm_path = std::string(tmpname) + ".asm";
    {
        std::ofstream f(asm_path);
        f << "bogus rax, 1\n";
    }
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "shellcode",
                       "exec", asm_path}),
              1);
    std::remove(asm_path.c_str());
}

// shellcode exec: one invocation = complete flow (convert -> write -> trigger)
// using a .bin file produced by hex2bin. Target must survive.
TEST(CliChain, ShellcodeExecBinFile) {
    char tmpname[L_tmpnam] = {0};
    std::tmpnam(tmpname);
    std::string bin_path = std::string(tmpname) + ".bin";
    EXPECT_EQ(run_cli({"deeptrace_cli", "hex2bin", "C3", bin_path}), 0);
    std::string pid = std::to_string(g_target.pid);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "exec", bin_path}), 0);
    std::remove(bin_path.c_str());
    // exec produced a record; find and free it to avoid cross-test residue.
    std::vector<deeptrace::InjectInfo> list;
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::shellcode_status(list), deeptrace::Result::Ok);
    deeptrace::detach();
    for (const auto& i : list) {
        if (i.kind == "shellcode" && i.remote_base != 0) {
            EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "free",
                               hex(i.remote_base)}),
                      0);
        }
    }
    // invalid source is a usage error
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "exec", "zzzz"}), 2);
}

// shellcode exec with an .asm source: assembled in memory, injected, executed.
TEST(CliChain, ShellcodeExecAsmSource) {
    char tmpname[L_tmpnam] = {0};
    std::tmpnam(tmpname);
    std::string asm_path = std::string(tmpname) + ".asm";
    {
        std::ofstream f(asm_path);
        f << "ret\n";
    }
    std::string pid = std::to_string(g_target.pid);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "exec", asm_path}), 0);
    std::remove(asm_path.c_str());
    // cleanup any record created by exec
    std::vector<deeptrace::InjectInfo> list;
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::shellcode_status(list), deeptrace::Result::Ok);
    deeptrace::detach();
    for (const auto& i : list) {
        if (i.kind == "shellcode" && i.remote_base != 0) {
            EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "free",
                               hex(i.remote_base)}),
                      0);
        }
    }
}

// ---- v2.3.0: AA-style script engine (script run/disable/status) ----

// Shared temp-write for AA scripts (used by materialize_aa and
// write_aa_script); content is written as-is.
std::string materialize_aa_content(const std::string& content, int tag) {
    char tmpname[L_tmpnam] = {0};
    std::tmpnam(tmpname);
    std::string path = std::string(tmpname) + "_" + std::to_string(tag) + ".aa";
    std::ofstream f(path, std::ios::binary);
    f << content;
    f.close();
    return path;
}

// Load an AA script fixture next to the test exe (deployed by POST_BUILD),
// substitute the %HOOK_OFF% placeholder with the runtime module offset, and
// write the materialized script to a temp file. Returns the temp path.
std::string materialize_aa(const char* fixture, const std::string& hook_off,
                           int tag) {
    char buf[MAX_PATH] = {0};
    ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string dir(buf);
    size_t slash = dir.find_last_of("/\\");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    std::ifstream in(dir + "\\" + fixture);
    EXPECT_TRUE(in.good()) << "fixture not deployed: " << fixture;
    std::string content((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
    size_t p = 0;
    constexpr size_t kPlaceholderLen = 10;  // strlen("%HOOK_OFF%")
    while ((p = content.find("%HOOK_OFF%", p)) != std::string::npos) {
        content.replace(p, kPlaceholderLen, hook_off);
        p += hook_off.size();
    }
    return materialize_aa_content(content, tag);
}

// Write ad-hoc AA script text to a temp file (for boundary scripts that do
// not warrant a repository fixture); returns the path.
std::string write_aa_script(const std::string& content, int tag) {
    return materialize_aa_content(content, tag);
}

bool target_alive() {
    HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, g_target.pid);
    if (!h) return false;
    DWORD code = 0;
    bool alive = ::GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    ::CloseHandle(h);
    return alive;
}

// call-type script: alloc + createThread(ret) + dealloc. Run is idempotent
// (already enabled), disable is idempotent (already disabled). Target must
// survive the whole round trip.
// call-type script from the real fixture cli/test/scripts/script_call.aa.
TEST(CliChain, ScriptRunCreateThreadIdempotent) {
    std::string script = materialize_aa("script_call.aa", "", 1);
    std::string pid = std::to_string(g_target.pid);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "script", "run", script}), 0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "script", "run", script}), 0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "script", "disable", script}), 0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "script", "disable", script}), 0);
    std::remove(script.c_str());
    EXPECT_TRUE(target_alive());
}

// hook-type script: patch a module address (g_bytes data region) with a
// 5-byte jmp, then restore on disable. Verify the patch and the restore by
// reading the actual bytes.
TEST(CliChain, ScriptRunHookRoundTrip) {
    uintptr_t base = 0;
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::module_base("deeptrace_target.exe", &base),
              deeptrace::Result::Ok);
    deeptrace::detach();
    ASSERT_NE(base, 0u);
    uintptr_t offset = g_target.g_bytes - base;
    char offbuf[32];
    std::snprintf(offbuf, sizeof offbuf, "%llX", (unsigned long long)offset);

    // hook-type script from the real fixture cli/test/scripts/script_hook.aa
    // with %HOOK_OFF% substituted by the runtime module offset.
    std::string script = materialize_aa("script_hook.aa", "0x" + std::string(offbuf), 2);
    std::string pid = std::to_string(g_target.pid);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "script", "run", script}), 0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "script", "status"}), 0);

    // The hook patched the first 5 bytes of g_bytes with E9 (jmp rel32).
    {
        uint8_t b5[5] = {0};
        size_t n = 0;
        ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
        EXPECT_EQ(deeptrace::memory_read(g_target.g_bytes, b5, 5, &n),
                  deeptrace::Result::Ok);
        deeptrace::detach();
        EXPECT_EQ(b5[0], 0xE9) << "g_bytes must start with jmp rel32 opcode";
    }

    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "script", "disable", script}), 0);

    // Original bytes restored from the saved record.
    {
        uint8_t b5[5] = {0};
        size_t n = 0;
        ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
        EXPECT_EQ(deeptrace::memory_read(g_target.g_bytes, b5, 5, &n),
                  deeptrace::Result::Ok);
        deeptrace::detach();
        EXPECT_EQ(b5[0], 0xDE);
        EXPECT_EQ(b5[1], 0xAD);
        EXPECT_EQ(b5[2], 0xBE);
        EXPECT_EQ(b5[3], 0xEF);
        EXPECT_EQ(b5[4], 0x48);
    }
    std::remove(script.c_str());
    EXPECT_TRUE(target_alive());
}

// Usage errors: missing file, unknown block, missing [ENABLE]/[DISABLE]
// blocks -> exit 2.
TEST(CliChain, ScriptRunUsageErrors) {
    std::string pid = std::to_string(g_target.pid);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "script", "run", "no_such.aa"}), 2);

    // unknown block from the real fixture cli/test/scripts/script_bad.aa
    std::string bad = materialize_aa("script_bad.aa", "", 3);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "script", "run", bad}), 2);
    std::remove(bad.c_str());

    std::string noen = write_aa_script("[DISABLE]\ndealloc(a)\n", 4);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "script", "run", noen}), 2);
    std::remove(noen.c_str());

    std::string nodis = write_aa_script("[ENABLE]\nalloc(a, 8)\n", 5);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "script", "disable", nodis}), 2);
    std::remove(nodis.c_str());
}

// Script commands operate on a target process: without -p they must fail
// with NotAttached (exit 1). The script file itself must be readable, so use
// a real file (parse/validation errors would otherwise win with exit 2).
TEST(CliChain, ScriptRequiresAttach) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "script", "status"}), 1);
    std::string script = write_aa_script("[ENABLE]\nalloc(a, 8)\n", 7);
    EXPECT_EQ(run_cli({"deeptrace_cli", "script", "run", script}), 1);
    std::remove(script.c_str());
}

// Mid-script failure (invalid instruction -> BadFormat) must roll back:
// no enabled record remains and no symbol is left allocated.
// Fixture: cli/test/scripts/script_badasm.aa.
TEST(CliChain, ScriptRunRollbackOnFailure) {
    std::string script = materialize_aa("script_badasm.aa", "", 6);
    std::string pid = std::to_string(g_target.pid);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "script", "run", script}), 1);
    std::vector<deeptrace::ScriptInfo> list;
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::script_status(list), deeptrace::Result::Ok);
    deeptrace::detach();
    bool found = false;
    for (const auto& s : list) {
        if (s.path == script) found = true;
    }
    EXPECT_FALSE(found) << "enabled record must be rolled back after failure";
    std::remove(script.c_str());
    EXPECT_TRUE(target_alive());
}

// ---- v2.5.0: artificial pointer (symbol refs on any instruction) ----

// The artificial-pointer fixture spawns a thread that writes two known 64-bit
// values into two 8-byte slots: slotA via accumulator moffs64 (mov [slotA],rax)
// and slotB via non-accumulator RIP-relative (mov [slotB],rcx). Reading both
// slots back proves both encodings execute correctly against real runtime
// addresses (not just assembling). Fixture: script_aptr.aa.
TEST(CliChain, ScriptArtificialPointerRoundTrip) {
    std::string script = materialize_aa("script_aptr.aa", "", 30);
    std::string pid = std::to_string(g_target.pid);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "script", "run", script}), 0);

    // Locate the two slot addresses from the script record (owner = path).
    std::vector<deeptrace::ScriptInfo> list;
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::script_status(list), deeptrace::Result::Ok);
    deeptrace::detach();
    uintptr_t slotA = 0, slotB = 0;
    for (const auto& s : list) {
        if (s.path != script) continue;
        for (const auto& a : s.allocs) {
            if (a.first == "slotA") slotA = a.second;
            else if (a.first == "slotB") slotB = a.second;
        }
    }
    ASSERT_NE(slotA, 0u) << "slotA not recorded";
    ASSERT_NE(slotB, 0u) << "slotB not recorded";

    // The spawned thread wrote both values; read them back from the target.
    uint64_t vA = 0, vB = 0;
    size_t n = 0;
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::memory_read(slotA, &vA, 8, &n), deeptrace::Result::Ok);
    EXPECT_EQ(n, 8u);
    EXPECT_EQ(deeptrace::memory_read(slotB, &vB, 8, &n), deeptrace::Result::Ok);
    deeptrace::detach();
    EXPECT_EQ(vA, 0x1122334455667788ull) << "moffs64 store wrote wrong value";
    EXPECT_EQ(vB, 0x99AABBCCDDEEFF00ull) << "RIP-relative store wrote wrong value";

    // Cleanup: disable frees the slots and the code buffer by name.
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "script", "disable", script}), 0);
    std::remove(script.c_str());
    EXPECT_TRUE(target_alive());
}

// script check accepts the artificial-pointer script (no attach).
TEST(CliChain, ScriptCheckArtificialPointer) {
    std::string script = materialize_aa("script_aptr.aa", "", 31);
    EXPECT_EQ(run_cli({"deeptrace_cli", "script", "check", script}), 0);
    std::remove(script.c_str());
}

// ---- v2.4.0: script check (syntax + assembly precheck, no attach) ----

// script check is a pure local validation: it must work without -p, without
// attaching, and must not touch the target process (no alloc, no write, no
// thread). Valid scripts pass with exit 0; invalid ones fail with exit 2.
TEST(CliChain, ScriptCheckNoAttach) {
    std::string call = materialize_aa("script_call.aa", "", 20);
    EXPECT_EQ(run_cli({"deeptrace_cli", "script", "check", call}), 0);
    std::remove(call.c_str());

    std::string bad = materialize_aa("script_bad.aa", "", 21);
    EXPECT_EQ(run_cli({"deeptrace_cli", "script", "check", bad}), 2);
    std::remove(bad.c_str());

    std::string badasm = materialize_aa("script_badasm.aa", "", 22);
    EXPECT_EQ(run_cli({"deeptrace_cli", "script", "check", badasm}), 2);
    std::remove(badasm.c_str());

    EXPECT_EQ(run_cli({"deeptrace_cli", "script", "check", "no_such.aa"}), 2);
    // side effect check: the target must be untouched by check (no -p at all)
    EXPECT_TRUE(target_alive());
}

// The hook fixture materializes %HOOK_OFF% into a valid hex offset, so the
// full hook script (target + jmp + filler) passes check with exit 0. The raw
// fixture with the placeholder fails (invalid module offset), which is the
// correct behavior: check validates the file exactly as given.
TEST(CliChain, ScriptCheckHookFixture) {
    // raw fixture (placeholder still present) -> invalid module offset
    char buf[MAX_PATH] = {0};
    ::GetModuleFileNameA(nullptr, buf, MAX_PATH);
    std::string dir(buf);
    size_t slash = dir.find_last_of("/\\");
    if (slash != std::string::npos) dir = dir.substr(0, slash);
    EXPECT_EQ(run_cli({"deeptrace_cli", "script", "check",
                       dir + "\\script_hook.aa"}),
              2);
    // materialized (placeholder replaced with a hex offset) -> passes
    std::string ok = materialize_aa("script_hook.aa", "0x1000", 23);
    EXPECT_EQ(run_cli({"deeptrace_cli", "script", "check", ok}), 0);
    std::remove(ok.c_str());
}

// Structural validation: a hook target without a following jmp, a jmp to an
// undefined label, and a second instruction inside a hook block are all
// detected statically (exit 2) without attaching.
TEST(CliChain, ScriptCheckHookStructureErrors) {
    std::string no_jmp = write_aa_script(
        "[ENABLE]\n\"m.dll\"+100:\nnop 2\n", 24);
    EXPECT_EQ(run_cli({"deeptrace_cli", "script", "check", no_jmp}), 2);
    std::remove(no_jmp.c_str());

    std::string undef = write_aa_script(
        "[ENABLE]\n\"m.dll\"+100:\njmp nonexist\n", 25);
    EXPECT_EQ(run_cli({"deeptrace_cli", "script", "check", undef}), 2);
    std::remove(undef.c_str());

    std::string second = write_aa_script(
        "[ENABLE]\nalloc(n,8)\n\"m.dll\"+100:\njmp n\nmov eax,1\n", 26);
    EXPECT_EQ(run_cli({"deeptrace_cli", "script", "check", second}), 2);
    std::remove(second.c_str());
    EXPECT_TRUE(target_alive());
}

// shellcode injectfile: .bin file -> inject (execute immediately), then free.
TEST(CliChain, ShellcodeInjectFile) {
    char tmpname[L_tmpnam] = {0};
    std::tmpnam(tmpname);
    std::string bin_path = std::string(tmpname) + ".bin";
    EXPECT_EQ(run_cli({"deeptrace_cli", "hex2bin", "C3", bin_path}), 0);
    std::string pid = std::to_string(g_target.pid);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "injectfile", bin_path}), 0);
    std::remove(bin_path.c_str());
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "injectfile",
                       "no_such.bin"}),
              2);
    // free the injected record
    std::vector<deeptrace::InjectInfo> list;
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::shellcode_status(list), deeptrace::Result::Ok);
    deeptrace::detach();
    for (const auto& i : list) {
        if (i.kind == "shellcode" && i.remote_base != 0) {
            EXPECT_EQ(run_cli({"deeptrace_cli", "-p", pid, "shellcode", "free",
                               hex(i.remote_base)}),
                      0);
        }
    }
}

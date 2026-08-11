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

TEST(CliChain, Registers) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "debug",
                       "registers"}),
              0);
}

TEST(CliChain, DebugStatus) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "debug", "status"}),
              0);
}

// Regression: `debug attach` used to kill the target because the executor
// auto-detached without DebugActiveProcessStop (debugger exit terminates the
// debuggee on Windows). The target must still be running afterwards.
TEST(CliChain, DebugAttachTargetSurvives) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "debug",
                       "attach"}),
              0);
    HANDLE h = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                             g_target.pid);
    ASSERT_NE(h, nullptr);
    DWORD code = 0;
    EXPECT_TRUE(::GetExitCodeProcess(h, &code));
    EXPECT_EQ(code, STILL_ACTIVE);
    ::CloseHandle(h);
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

TEST(CliChain, DebugBreakRoundTrip) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "debug", "break",
                       hex(g_target.g_int)}),
              0);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "debug", "clear",
                       hex(g_target.g_int)}),
              0);
}

TEST(CliChain, BreakpointAffectsDeeptraceState) {
    // break through CLI, then verify the byte is 0xCC via deeptrace directly
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "debug", "break",
                       hex(g_target.g_int)}),
              0);
    uint8_t b = 0;
    size_t n = 0;
    ASSERT_EQ(deeptrace::attach(g_target.pid), deeptrace::Result::Ok);
    EXPECT_EQ(deeptrace::memory_read(g_target.g_int, &b, 1, &n), deeptrace::Result::Ok);
    EXPECT_EQ(b, 0xCC);
    deeptrace::detach();
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "debug", "clear",
                       hex(g_target.g_int)}),
              0);
}

// Helper: write a script file and return its path (unique per call).
std::string write_script(const std::string& content, int tag) {
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
TEST(CliChain, DebugRunScriptedSession) {
    std::string script = write_script(
        "[\n"
        "  {\"op\": \"status\"},\n"
        "  {\"op\": \"registers\"},\n"
        "  {\"op\": \"read\", \"addr\": \"" + hex(g_target.g_int) +
            "\", \"size\": \"4\"},\n"
        "  {\"op\": \"break\", \"addr\": \"" + hex(g_target.g_int) + "\"},\n"
        "  {\"op\": \"clear\", \"addr\": \"" + hex(g_target.g_int) + "\"},\n"
        "  {\"op\": \"watch_add\", \"desc\": \"sc_g\", \"addr\": \"" +
            hex(g_target.g_int) + "\", \"type\": \"dword\"},\n"
        "  {\"op\": \"watch_list\"},\n"
        "  {\"op\": \"watch_clear\"}\n"
        "]\n",
        1);
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
TEST(CliChain, DebugRunWriteSpacedHex) {
    std::string script = write_script(
        "[{\"op\": \"write\", \"addr\": \"" + hex(g_target.g_int) +
            "\", \"bytes\": \"BE BA FE CA\"}]\n",
        2);
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

// Script errors: missing file and invalid content -> exit 2, no session opened.
TEST(CliChain, DebugRunScriptErrors) {
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "debug", "run",
                       "no_such_script.json"}),
              2);
    std::string bad = write_script("[{\"op\": \"frobnicate\"}]\n", 3);
    EXPECT_EQ(run_cli({"deeptrace_cli", "-p", std::to_string(g_target.pid), "debug", "run",
                       bad}),
              2);
    std::remove(bad.c_str());
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

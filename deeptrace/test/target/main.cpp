// deeptrace_target.exe - test target program.
// Provides known values at known addresses (ASLR disabled) and prints:
//   a banner with symbol/address/value table,
//   a fixed PID line: "PID: <number>",
//   a WORKER_TID line for the busy worker thread.
// Loops until g_flag is set to 0xCAFE (used by memory-write tests).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cinttypes>
#include <cstdint>
#include <cstdio>

namespace target {

volatile int32_t g_int = 0x11223344;
volatile int64_t g_int64 = 0x1122334455667788LL;
volatile float g_float = 3.14159f;
volatile double g_double = 2.71828;
volatile uint8_t g_bytes[16] = {0xDE, 0xAD, 0xBE, 0xEF, 0x48, 0x8B, 0x45,
                                0x08, 0x90, 0x90, 0x90, 0x90, 0xCC, 0xC3,
                                0x90, 0x90};
volatile uint32_t g_flag = 0;     // set to 0xCAFE to exit
volatile uint64_t g_counter = 0;  // incremented by worker thread
volatile uint8_t g_work_loop[16] = {0x48, 0xFF, 0xC0, 0x48, 0x8B, 0x05, 0x00,
                                    0x00, 0x00, 0x00, 0xEB, 0xF4, 0x90, 0x90,
                                    0x90, 0x90};  // bytes resembling a loop

DWORD WINAPI WorkerProc(LPVOID) {
    // Busy loop: keeps RIP inside user code so single-step / breakpoint
    // tests have a stable target.
    for (;;) {
        ++g_counter;
        if (g_flag == 0xCAFE) break;
    }
    return 0;
}

void PrintBanner() {
    printf("=============================================\n");
    printf(" deeptrace_target v1.0 - test target\n");
    printf(" ASLR disabled, fixed image base\n");
    printf("---------------------------------------------\n");
    printf(" PID: %lu\n", GetCurrentProcessId());
    printf(" g_int       = 0x%08X  @0x%llX\n", (uint32_t)g_int,
           (unsigned long long)(uintptr_t)&g_int);
    printf(" g_int64     = 0x%016llX @0x%llX\n", (unsigned long long)g_int64,
           (unsigned long long)(uintptr_t)&g_int64);
    printf(" g_float     = %f  @0x%llX\n", (double)g_float,
           (unsigned long long)(uintptr_t)&g_float);
    printf(" g_double    = %f  @0x%llX\n", g_double,
           (unsigned long long)(uintptr_t)&g_double);
    printf(" g_bytes[0]  = 0xDE @0x%llX\n",
           (unsigned long long)(uintptr_t)&g_bytes[0]);
    printf(" g_flag      = 0x%08X @0x%llX\n", (uint32_t)g_flag,
           (unsigned long long)(uintptr_t)&g_flag);
    printf(" g_counter   = 0x%016llX @0x%llX\n", (unsigned long long)g_counter,
           (unsigned long long)(uintptr_t)&g_counter);
    printf(" worker_fn   = 0x%llX\n",
           (unsigned long long)(uintptr_t)&WorkerProc);
    printf("---------------------------------------------\n");
    fflush(stdout);
}

}  // namespace target

int main() {
    using namespace target;
    PrintBanner();

    HANDLE hThread = CreateThread(nullptr, 0, WorkerProc, nullptr, 0, nullptr);
    printf(" WORKER_TID: %lu\n", hThread ? GetThreadId(hThread) : 0UL);
    fflush(stdout);

    // Main loop: sleep to keep process alive; exits when g_flag set.
    while (g_flag != 0xCAFE) {
        Sleep(10);
    }
    if (hThread) {
        WaitForSingleObject(hThread, 2000);
        CloseHandle(hThread);
    }
    printf("deeptrace_target exiting (g_flag set)\n");
    return 0;
}

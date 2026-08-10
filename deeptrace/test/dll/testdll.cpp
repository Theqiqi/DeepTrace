// testdll.dll - companion module for dll-injection tests.
// Provides a small exported API so an injection test can sanity-check that
// the module was loaded into the target (base address != 0, module listed).
// Does not link deeptrace and has no dependencies besides kernel32.

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

extern "C" {
__declspec(dllexport) int testdll_value(void) { return 0x12345678; }
__declspec(dllexport) volatile int testdll_global = 42;
}

BOOL APIENTRY DllMain(HMODULE, DWORD reason, LPVOID) {
    (void)reason;
    return TRUE;
}

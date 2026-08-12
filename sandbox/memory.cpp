#include "memory.h"

#include "winapi.h"

bool MemRead(HANDLE proc, uintptr_t addr, void* buf, size_t size) {
    SIZE_T rd = 0;
    return g_winapi.ReadProcessMemory(proc, reinterpret_cast<LPCVOID>(addr), buf, size, &rd) &&
           rd == size;
}

bool MemWrite(HANDLE proc, uintptr_t addr, const void* buf, size_t size) {
    SIZE_T wr = 0;
    return g_winapi.WriteProcessMemory(proc, reinterpret_cast<LPVOID>(addr), buf, size, &wr) &&
           wr == size;
}

uintptr_t MemAlloc(HANDLE proc, size_t size) {
    return reinterpret_cast<uintptr_t>(g_winapi.VirtualAllocEx(
        proc, nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
}

bool MemFree(HANDLE proc, uintptr_t addr) {
    return g_winapi.VirtualFreeEx(proc, reinterpret_cast<LPVOID>(addr), 0, MEM_RELEASE) == TRUE;
}

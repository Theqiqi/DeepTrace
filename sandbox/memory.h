#pragma once
#include <cstddef>
#include <cstdint>
#include <windows.h>

// ---- memory 原子化接口 ----
// 读/写/分配/释放四个原子操作,内部经 g_winapi 函数指针表调用。
// 各基础设施层共享同一份表(winapi.h 的全局唯一实例),无需各自实例化、无需传表。

bool MemRead(HANDLE proc, uintptr_t addr, void* buf, size_t size);
bool MemWrite(HANDLE proc, uintptr_t addr, const void* buf, size_t size);
uintptr_t MemAlloc(HANDLE proc, size_t size);
bool MemFree(HANDLE proc, uintptr_t addr);

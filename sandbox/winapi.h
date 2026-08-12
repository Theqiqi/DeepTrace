#pragma once
#include <windows.h>

// ---- 函数指针类型 + 函数指针表(Windows 内存相关 API) ----
// 初始化用"直接赋值绑定"(链接器正常解析导入,不经 GetProcAddress)。
//
// 实例化策略:全进程只存在一份——extern 全局 `g_winapi`(定义在 winapi.cpp,
// 静态初始化一次)。main 与各基础设施层直接取用,无需函数、无需实例化、无需传表;
// 测试注入即 `g_winapi.ReadProcessMemory = fake`。

using OpenProcessFn        = HANDLE(WINAPI*)(DWORD, BOOL, DWORD);
using CloseHandleFn        = BOOL(WINAPI*)(HANDLE);
using ReadProcessMemoryFn  = BOOL(WINAPI*)(HANDLE, LPCVOID, LPVOID, SIZE_T, SIZE_T*);
using WriteProcessMemoryFn = BOOL(WINAPI*)(HANDLE, LPVOID, LPCVOID, SIZE_T, SIZE_T*);
using VirtualAllocExFn     = LPVOID(WINAPI*)(HANDLE, LPVOID, SIZE_T, DWORD, DWORD);
using VirtualFreeExFn      = BOOL(WINAPI*)(HANDLE, LPVOID, SIZE_T, DWORD);
using VirtualQueryExFn     = SIZE_T(WINAPI*)(HANDLE, LPCVOID, PMEMORY_BASIC_INFORMATION, SIZE_T);
using VirtualProtectExFn   = BOOL(WINAPI*)(HANDLE, LPVOID, SIZE_T, DWORD, PDWORD);

// C 风格 typedef:类型短名 WinApi + 指针别名 pWinApi(指针别名当前未用,保留以便将来按指针持有)
typedef struct WindowsMemoryApi {
    ReadProcessMemoryFn  ReadProcessMemory;
    WriteProcessMemoryFn WriteProcessMemory;
    VirtualAllocExFn     VirtualAllocEx;
    VirtualFreeExFn      VirtualFreeEx;
    VirtualQueryExFn     VirtualQueryEx;
    VirtualProtectExFn   VirtualProtectEx;
    OpenProcessFn        OpenProcess;
    CloseHandleFn        CloseHandle;
} WinApi, *pWinApi;

// 全局唯一实例:声明在此,定义在 winapi.cpp(直接赋值绑定,静态初始化一次)
extern WinApi g_winapi;

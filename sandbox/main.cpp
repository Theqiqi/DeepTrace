// main:只做 2~3 个原子调用,验证 memory 层经函数指针表工作。
// 表是全局唯一实例 g_winapi(winapi.cpp 定义一次),main 无需实例化、无需传表。
// 操作自身进程(pid = GetCurrentProcessId()),自包含、无需外部目标。

#include <cstdint>
#include <cstdio>

#include "memory.h"
#include "winapi.h"

int main() {
    HANDLE proc = g_winapi.OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetCurrentProcessId());
    if (!proc) { printf("FAIL: OpenProcess\n"); return 1; }

    const uint64_t value = 0x1122334455667788ull;
    uint64_t back = 0;

    uintptr_t addr = MemAlloc(proc, 0x100);                      // ① 分配
    if (!addr) { printf("FAIL: alloc\n"); return 1; }
    MemWrite(proc, addr, &value, sizeof(value));                 // ② 写
    MemRead(proc, addr, &back, sizeof(back));                    // ③ 读回

    printf("alloc = %p\n", reinterpret_cast<void*>(addr));
    printf("readback = 0x%016llX  match = %s\n",
           static_cast<unsigned long long>(back), back == value ? "yes" : "no");

    bool ok = addr != 0 && back == value;
    ok = MemFree(proc, addr) && ok;                              // 收尾:释放 + 关闭
    ok = g_winapi.CloseHandle(proc) && ok;

    printf(ok ? "ALL PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}

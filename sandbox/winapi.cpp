#include "winapi.h"

// 全局唯一实例:直接赋值绑定真实函数,链接器正常解析导入,不经 GetProcAddress。
// 初始化顺序与结构体成员声明顺序一致。
WinApi g_winapi = {
    ReadProcessMemory,
    WriteProcessMemory,
    VirtualAllocEx,
    VirtualFreeEx,
    VirtualQueryEx,
    VirtualProtectEx,
    OpenProcess,
    CloseHandle,
};

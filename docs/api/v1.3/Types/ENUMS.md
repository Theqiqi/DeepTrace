# 枚举:ValueType / BreakpointType

定义于 `deeptrace.h`(经 `domain/types.h`)。

## ValueType —— 值类型

用于类型化内存读取(`memory_readval`)与监视项(`watch_add`)指定的数据类型。

```cpp
enum class ValueType { Byte, Word, Dword, Qword, Float, Double };
```

| 枚举值 | 大小 | 含义 |
|--------|------|------|
| `Byte` | 1 字节 | 无符号 8 位整数,格式化输出 `0x00`~`0xFF` |
| `Word` | 2 字节 | 无符号 16 位整数(小端) |
| `Dword` | 4 字节 | 无符号 32 位整数(小端) |
| `Qword` | 8 字节 | 无符号 64 位整数(小端) |
| `Float` | 4 字节 | IEEE 754 单精度浮点(小端) |
| `Double` | 8 字节 | IEEE 754 双精度浮点(小端) |

使用示例:

```cpp
std::string text;
deeptrace::Result r =
    deeptrace::memory_readval(0x140001000, deeptrace::ValueType::Dword, text);
// text 形如 "0x11223344"
```

## BreakpointType —— 断点类型

`BreakpointInfo.type` 字段的值,标识断点的实现机制。

```cpp
enum class BreakpointType { Software, Hardware, PageGuard };
```

| 枚举值 | 含义 |
|--------|------|
| `Software` | 软件断点:将目标地址首字节改写为 `0xCC`(INT3),由 `breakpoint_set` 创建;`BreakpointInfo.original_byte` 保存被覆盖的原字节。 |
| `Hardware` | 硬件断点:利用 x64 调试寄存器 DR0-DR3 实现,不修改目标代码,由 `hw_breakpoint_set` 创建;`BreakpointInfo.hw_index` 记录占用的 DR 槽位(0-3)。 |
| `PageGuard` | 页守卫:通过 `VirtualProtectEx` 对内存页设置 `PAGE_GUARD` 属性,由 `guard_set` 创建。 |

## 参见

- [memory_readval](../Modules/MEMORY.md#deeptracememory_readval)
- [breakpoint_set](../Modules/DEBUG.md#deeptracebreakpoint_set)
- [hw_breakpoint_set](../Modules/DEBUG.md#deeptracehw_breakpoint_set)
- [guard_set](../Modules/DEBUG.md#deeptraceguard_set)

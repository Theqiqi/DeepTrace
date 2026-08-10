# Enums: ValueType / BreakpointType

Defined in `deeptrace.h` (via `domain/types.h`).

## ValueType — Value Type

Specifies the data type for typed memory reads (`memory_readval`) and watch entries (`watch_add`).

```cpp
enum class ValueType { Byte, Word, Dword, Qword, Float, Double };
```

| Enum value | Size | Meaning |
|------------|------|---------|
| `Byte` | 1 byte | unsigned 8-bit integer; formatted output `0x00`~`0xFF` |
| `Word` | 2 bytes | unsigned 16-bit integer (little-endian) |
| `Dword` | 4 bytes | unsigned 32-bit integer (little-endian) |
| `Qword` | 8 bytes | unsigned 64-bit integer (little-endian) |
| `Float` | 4 bytes | IEEE 754 single-precision float (little-endian) |
| `Double` | 8 bytes | IEEE 754 double-precision float (little-endian) |

Usage example:

```cpp
std::string text;
deeptrace::Result r =
    deeptrace::memory_readval(0x140001000, deeptrace::ValueType::Dword, text);
// text looks like "0x11223344"
```

## BreakpointType — Breakpoint Type

The value of the `BreakpointInfo.type` field; identifies the breakpoint implementation mechanism.

```cpp
enum class BreakpointType { Software, Hardware, PageGuard };
```

| Enum value | Meaning |
|------------|---------|
| `Software` | Software breakpoint: rewrites the first byte at the target address to `0xCC` (INT3), created by `breakpoint_set`; `BreakpointInfo.original_byte` stores the overwritten original byte. |
| `Hardware` | Hardware breakpoint: uses the x64 debug registers DR0-DR3, does not modify target code, created by `hw_breakpoint_set`; `BreakpointInfo.hw_index` records the occupied DR slot (0-3). |
| `PageGuard` | Page guard: sets the `PAGE_GUARD` attribute on a memory page via `VirtualProtectEx`, created by `guard_set`. |

## See Also

- [memory_readval](../Modules/MEMORY.md#deeptracememory_readval)
- [breakpoint_set](../Modules/DEBUG.md#deeptracebreakpoint_set)
- [hw_breakpoint_set](../Modules/DEBUG.md#deeptracehw_breakpoint_set)
- [guard_set](../Modules/DEBUG.md#deeptraceguard_set)

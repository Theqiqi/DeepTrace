# 模块:反汇编

对目标进程内存进行 x64 反汇编。要求已 `attach` 目标进程。

## deeptrace::disasm_at

### 语法

```cpp
Result disasm_at(uintptr_t addr, uint32_t count, std::vector<Instruction>& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `addr` | `uintptr_t` | 目标进程内起始地址 |
| `count` | `uint32_t` | 期望反汇编的指令条数,范围 1 ~ 10000 |
| `out` | `std::vector<Instruction>&` | 输出参数,指令列表(地址/机器码/文本) |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 反汇编成功(可能少于 `count` 条,以 `out.size()` 为准) |
| `Result::InvalidArg` | `count == 0` 或 `count > 10000` |
| `Result::NotAttached` | 未附加会话 |
| `Result::ReadFault` | 起始内存不可读 |

### 说明

从 `addr` 起反汇编最多 `count` 条指令。内部按 `count × 15` 字节(x64 最长指令)预读,
逐条解码;遇无法解码的字节或内存边界提前停止,返回已解码部分。每条 `Instruction`
含指令地址、机器码字节与纯 ASCII 反汇编文本。典型用途:单步后查看 `*out_rip` 处的
后续指令、HOOK 点定位、代码分析。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
std::vector<deeptrace::Instruction> insns;
if (deeptrace::disasm_at(0x140001000, 10, insns) == deeptrace::Result::Ok) {
    for (const auto& i : insns) {
        std::cout << i.text << "\n";
    }
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::disasm_range](#deeptracedisasm_range)

---

## deeptrace::disasm_range

### 语法

```cpp
Result disasm_range(uintptr_t start, uintptr_t end, std::vector<Instruction>& out);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `start` | `uintptr_t` | 反汇编起始地址 |
| `end` | `uintptr_t` | 反汇编结束地址(不含),`end >= start`,区间上限 64 MiB |
| `out` | `std::vector<Instruction>&` | 输出参数,指令列表 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 反汇编成功 |
| `Result::InvalidArg` | `end < start` 或区间超过 64 MiB |
| `Result::NotAttached` | 未附加会话 |
| `Result::ReadFault` | 区间起始内存不可读 |

### 说明

反汇编 `[start, end)` 地址区间内的全部可解码指令。相比 `disasm_at`,适合已知边界
的整段代码分析(如整个函数体)。区间过大(>64 MiB)返回 `InvalidArg` 防止失控。

前置条件:已 `attach(pid)`。后置条件:无。

### 示例

```cpp
std::vector<deeptrace::Instruction> insns;
deeptrace::disasm_range(func_addr, func_addr + 0x200, insns);
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::disasm_at](#deeptracedisasm_at)

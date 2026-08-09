# 示例:调试会话与断点

演示调试会话的完整生命周期:**附加 → 读寄存器 → 进入调试 → 设断点 → 单步 →
清断点 → 退出调试 → 分离**。源码:
[src/debug_breakpoints.cpp](src/debug_breakpoints.cpp)。

## API 调用顺序

| 步骤 | API | 说明 |
|------|-----|------|
| 1 | `attach(pid)` | 建立进程会话 |
| 2 | `registers_get` | 读取寄存器(无需调试模式) |
| 3 | `debug_attach()` | 进入调试模式(需管理员) |
| 4 | `breakpoint_set` | 设置软件断点(改写为 0xCC) |
| 5 | `debug_step` | 单步执行 |
| 6 | `breakpoint_clear` | 恢复原字节 |
| 7 | `debug_detach()` | 退出调试模式(目标继续运行) |
| 8 | `detach()` | 关闭进程会话 |

## 代码

```cpp
// 完整代码见 src/debug_breakpoints.cpp,关键片段:
deeptrace::attach(pid);                                        // 1. 附加

std::vector<deeptrace::RegisterInfo> regs;
deeptrace::registers_get(regs, 0);                             // 2. 寄存器

if (deeptrace::debug_attach() != deeptrace::Result::Ok) {      // 3. 调试附加
    deeptrace::detach();
    return 1;   // 需要管理员权限
}

deeptrace::BreakpointInfo bp;
deeptrace::breakpoint_set(addr, bp);                           // 4. 设断点

uintptr_t rip = 0;
deeptrace::debug_step(0, &rip);                                // 5. 单步

deeptrace::breakpoint_clear(addr);                             // 6. 清断点
deeptrace::debug_detach();                                     // 7. 退出调试
deeptrace::detach();                                           // 8. 分离
```

## 构建与运行

```bat
build_examples.bat
debug_breakpoints.exe 1234 0x140001000
```

## 提示

- **必须**在退出前清除断点并调用 `debug_detach`/`detach`,否则目标进程可能因遗留
  INT3 或调试器未分离而被终止。
- `debug_step` 在未进入调试模式时也会自动执行一次性的附加→单步→分离流程。
- 硬件断点(`hw_breakpoint_set`)不修改目标代码,适合只读代码段。

## 相关 API

- [debug_attach](../Modules/DEBUG.md#deeptracedebug_attach)
- [breakpoint_set](../Modules/DEBUG.md#deeptracebreakpoint_set)
- [debug_step](../Modules/DEBUG.md#deeptracedebug_step)
- [registers_get](../Modules/DEBUG.md#deeptraceregisters_get)

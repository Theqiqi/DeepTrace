# deeptrace_cli - 版本变更记录

## v2.5.0 脚本符号引用(人造指针,相对 v2.4.0)

> 输入:用户问「脚本有没有分配人造指针的功能,有没有必要添加?」,给出 CE 经典人造
> 指针脚本(alloc(sunObjPtr,8) + registersymbol + `mov [sunObjPtr],rax`),并指出
> 「这好像就是 C 语言代码中的变量」。经调查与 ask_user 确认:**完整实现**符号引用支持。
> 本版本为修改/添加功能型流程,版本号功能位 +1(v2.4.0 → v2.5.0)。

### 1. 改动点清单(初稿,待 04_需求分析 确认)

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | 静态库 `asm_assemble_labels`:符号引用不再限于 jmp/call,支持非 jmp/call 引用(内存操作数/立即数),静默截断改为显式报错 | 修改 | DeepTrace/src/service/asm.cpp、asm.h |
| 2 | 内存操作数 `[sym]` 重写为 RIP 相对 `[rip+disp32]`(两遍定长+算位移),避免 Keystone 静默截断 | 新增 | service/asm.cpp |
| 3 | 立即数 `sym` 替换为地址字面量后编码,用 Capstone 校验实际编码地址 == 符号地址(防截断) | 新增 | service/asm.cpp、infrastructure/disassembly |
| 4 | CLI exec_enable / script check 同步(符号宇宙不变,仅编码层扩展) | 修改 | cli/src/interface/cmd_script.cpp |
| 5 | 版本号 2.5.0(help/version/CMake/测试断言) | 修改 | 全局 |

### 2. 已确认设计决策(ask_user 结果)

- **完整实现**符号引用:脚本汇编行可引用 alloc'd 符号/标签于任意指令(`mov [sym],reg`、
  `mov reg,[sym]`、`mov reg,sym`、`lea reg,[sym]` 等),不再局限于 jmp/call。
- 编码策略(基于 Keystone 实测,见 01_想法收集):
  - 内存操作数 `[sym]`:重写为 `[rip+disp32]`,disp = sym - (指令地址+长度),两遍汇编;
    位移超 int32 → BadFormat(显式报错,不静默错编)。
  - 立即数 `sym`:替换为地址字面量;rax 等 64 位目标 Keystone 用 movabs imm64 完整编码;
    随后用 Capstone 校验编码中实际引用的地址 == 符号地址,发现静默截断(如 32 位操作数
    装不下)→ BadFormat。
  - 复杂内存表达式 `[reg+sym]`/`[sym+reg]` 等 → 明确报错「不支持」。
- 释放机制沿用既有 per-PID 落盘记录(scripts.dat):`[DISABLE] dealloc(name)` 按名字
  查找地址并 VirtualFreeEx,与 CE「名字为线索」语义一致;无性能问题。
- 不实现 alloc near 真实就近分配(本次范围外);RIP 相对 ±2GB 距离限制作为能力边界记录。

### 3. 能力边界(新增)

- 符号引用支持的指令形态:独立内存操作数 `[sym]`、独立立即数 `sym`。
- RIP 相对位移必须落在 int32 范围内,否则 BadFormat。
- 32 位操作数目标引用 64 位地址(如 `mov ecx,sym` 且地址 > 0xFFFFFFFF)→ BadFormat。
- 复杂内存表达式引用符号 → 明确报错。

### 4. 实现期决策(代码审查后补充)

(待补充)

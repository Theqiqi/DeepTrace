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

- **编码方案落地**:内存操作数 `[sym]` 重写 `[rip+disp32]` 两遍汇编(占位定长 →
  算位移 → 回填),长度不一致防御性报错;`mov <acc>,[sym]`(rax/eax/ax)自编码
  moffs64(A1/A3,不受 ±2GB 限制);立即数替换地址字面量后 Keystone 编码 + Capstone
  重解码校验(防静默截断)。
- **审查修复(3 项)**:
  1. imm 长度守卫:值 > 0xFFFFFFFF 时必须编码为 ≥9 字节(64 位立即数),否则
     BadFormat——堵住 imm32 sign-extension 复现内核地址的假阳性。
  2. classify 精确括号匹配:仅 `[name]` 走内存路径;`[ name ]`/`[rsp+name]`/
     `[name+4]` 落到立即数路径(替换+校验,安全编码或显式 BadFormat)。
  3. nop/db 写入目标守卫:`[ENABLE]\nnop\n`(裸 nop = NopFill 1,CE hook 填充)
     无目标时 check 报错(退出 2)与 run 报错(退出 1)一致,不再往地址 0 写。
- **顺带修复预存 bug**:`save_enabled_scripts` 重写 scripts.dat 时丢弃符号 owner
  字段,导致 `script_enable` 后所有符号在 status 中归入 (unowned);已保留 owner,
  由集成测试 ScriptArtificialPointerRoundTrip 锁定。
- **能力边界补充**:`jmp [sym]` 仍按直接跳转(rel32 到符号地址)自编码(既有行为,
  非间接跳转),不在本次范围。

### 5. 验证

- 静态库:单元 115(含 AsmLabels 19 个编码/截断用例)+ 集成 47;CLI:单元 150 +
  集成 43 + e2e 174;Debug/Release 全绿。
- 真实进程验证:script_aptr.aa 线程写入双槽位,moffs64 与 RIP 相对两种编码的
  值读回一致;disable 按名释放;目标存活。
- 用户示例脚本(人造指针 + hook + 裸 nop)`script check` 通过。
- git:`feat: artificial pointer` + `fix: review findings` 提交齐全,tag `v2.5.0`。

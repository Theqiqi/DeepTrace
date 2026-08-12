# deeptrace_cli - 版本变更记录

## v2.8.0 mem write 符号寻址确认与测试补强(相对 v2.7.0)

> 输入:用户问「让脚本外也能直接写人造指针的值:mem write <symbol> <value>,
> 用于动态改指针目标」。
> 经调查确认:**能力在 v2.6.0 已完整实现**——`mem write` 的 address 参数
> 已接入 `resolve_addr`(数字优先,符号名经 script_symbol 解析),真实进程
> 手动验证 hex/dec 写入读回一致、未知符号 NotFound。v2.6.0 集成/e2e 只测了
> mem read/readval/disasm/watch add,唯独漏了 **mem write <symbol>** 用例,
> 且无文档明确该能力。本版本为「能力确认 + 测试覆盖补强 + 文档声明」,
> 无行为变更。版本号功能位 +1(v2.7.0 → v2.8.0)。

### 1. 改动点清单(初稿,待 04_需求分析 确认)

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | 确认 `mem write <symbol> <value> [fmt]` 已由 resolve_addr 接通(无代码改动) | 确认 | cli/src/interface/cmd_memory.cpp(v2.6.0 已接) |
| 2 | 单测:mem write 的 address 参数接受符号形状 | 新增 | cli/test/unit/parser_test.cpp |
| 3 | 集成测试:script run 后 mem write <symbol> hex/dec 写入、读回验证、未知符号 | 新增 | cli/test/integration/cli_integration_test.cpp |
| 4 | e2e:mem write <符号> 动态改指针目标读回 | 新增 | cli/test/e2e/test_cli_e2e.py、cases.md |
| 5 | 版本号 2.8.0(help/version/测试断言) | 修改 | 全局 |

### 2. 已确认设计决策

- **能力已存在(v2.6.0)**:mem write 第一个参数经 `resolve_addr` 解析——
  数字地址照旧;符号名(ASCII 标识符)经 `script_symbol` 解析为记录地址后
  写入。值格式沿用既有 mem write(hex 字节 / dec 8 字节小端)。
- **动态改指针目标语义**:人造指针槽位(如 `sunObjPtr`,8 字节)内存放目标
  对象指针;`mem write sunObjPtr <0x...>` 直接改写槽位值 = 动态改指针目标,
  脚本内代码(`mov rax,[sunObjPtr]`)读到的即新目标。
- **无新命令/无新参数**:完全复用 mem write 既有语法与错误语义。
- **错误语义不变**:符号未注册 → NotFound 退出 1;值格式非法 → InvalidArg
  退出 2;无 session → NotAttached 退出 1。

### 3. 能力边界(声明支持 / 不支持)

- 支持:`mem write <symbol> <value> [hex|dec]` 写任意脚本符号指向的内存
  (人造指针槽位 / 代码缓冲区均可)。
- 不支持:符号名恰好为数字形状(数字优先,记录在案);跨进程符号;
  dec 格式固定 8 字节(小端),写小于 8 字节的槽位会越界写(与既有 mem write
  dec 语义一致,调用方自行负责长度)。
- 符号生命周期:disable/dealloc 后符号失效 → NotFound。

### 4. 实现期决策(代码审查后补充)

- (待补充)

### 5. 验证

- 手动(真实进程):alloc slotA → mem write slotA hex/dec → mem read 读回一致;
  未知符号 NotFound;disable 清理。
- 单测 + 集成(真实进程 mem write <symbol> 读回) + e2e。
- git:流程每步提交齐全,tag `v2.8.0`。

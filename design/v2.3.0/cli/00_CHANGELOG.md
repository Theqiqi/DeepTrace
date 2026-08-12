# deeptrace_cli - 版本变更记录

## v2.3.0 脚本关键字引擎(相对 v2.2.0)

> 输入:想法「把 shellcode 的 alloc/run/free/createThread 等写成脚本关键字,shellcode 配合 asm/bin/hex 字节即可执行」,并附 Cheat Engine AA 脚本语法示例。
> 本版本为修改/添加功能型流程,版本号功能位 +1(v2.2.0 → v2.3.0)。
> 改动点清单为初稿(0.5 步建立),随分析与设计步骤细化,最终以各设计文档为准。

### 1. 改动点清单(初稿,待 04_需求分析 确认)

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | 新命令组 `script`:`script run <file>` 执行脚本文件 | 新增 | command/commands.cpp、interface/cmd_script.cpp |
| 2 | 脚本语法(参考 CE AA):关键字 alloc/label/registersymbol/createThread/dealloc/unregistersymbol/db + `[ENABLE]`/`[DISABLE]` 块 + 汇编行 | 新增 | 静态库(脚本执行能力) + CLI 脚本解析/执行 |
| 3 | 脚本中 shellcode 数据源:.asm 汇编 / .bin 文件 / 16 进制字节 | 新增 | 复用 v2.2.0 的 asm file / hex2bin / shellcode exec 能力 |
| 4 | 代码改写(hook)能力:目标地址改写为 jmp + 原始字节保存/恢复 | 待确认 | 静态库(较大能力,需确认范围) |
| 5 | 既有命令全部保留(兼容) | 不变 | 全局 |
| 6 | 版本号 2.3.0(help/version/CMake/测试断言) | 修改 | 全局 |

### 2. 已确认设计决策(ask_user 结果)

- 语法范围:**完整版 AA**(精简版基础上加 label/registersymbol/模块偏移地址表达式/hook 改写与恢复)。
- 数据源:**三态全支持** — .asm/.s 汇编文件、.bin 文件、16 进制字节;保留 asm 与 bin 与字节互转的既有 CLI 命令(asm file / hex2bin / shellcode exec)。
- 执行语义(用户提出幂等方案):**enable/disable 幂等** — 不管执行开启命令多少次、关闭多少次,只要各执行一次必然符合「开」或「关」状态,从而天然无状态;call 型脚本一次调用跑完 enable+disable 完整流程,hook 型脚本 enable 后持续存在、disable 单独触发。
- 脚本文件含 [ENABLE]/[DISABLE] 双块;hook 状态需跨调用可判定(enable 重复执行跳过、disable 重复执行跳过),hook 记录持久化方案在需求分析中确定。

### 3. 实现期决策补充(与 15_异常设计 同步)

- `alloc` 的 near 第三参数(如 `alloc(newmem,2048,"game.dll"+7D5778)`)接受为
  **放置提示**:解析并校验表达式(模块必须已加载,否则 NotFound 退出 1),但
  分配位置由 OS(VirtualAllocEx)决定,不保证就近放置。
- hook 目标后仅支持 `jmp <label>` 与 CE 填充行(`nop N`/`db`/标签),其余语句
  报脚本错误退出 1;hook 覆盖固定 5 字节(jmp rel32)。
- 已知边界:汇编块内未定义符号引用可能被 Keystone 静默编码(以 0 地址),
  hook 的 `jmp <label>` 例外——label 必须存在于脚本符号表,否则 BadFormat。

### 4. 变更范围(初稿)

| 位置 | 变更 |
|------|------|
| design/v2.3.0/cli/ | 受影响文档复制并标注改动(01/02/04/05/07/10/12/13/14/15/16/17/18) |
| cli/src/command/commands.cpp | 命令表新增 script 组 |
| cli/src/interface/cmd_script.cpp | 新命令处理器(脚本解析+执行) |
| DeepTrace/include/deeptrace.h | 新增带 label 汇编 / hook / createThread / 脚本记录 API(走 .flow/cpp_static_flow.md) |
| cli/test/ | 单元/集成/e2e 补用例 + 回归 |
| DeepTrace/test/ | 静态库补用例(如有新 API) |

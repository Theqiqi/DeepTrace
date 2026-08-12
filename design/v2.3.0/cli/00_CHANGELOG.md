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

### 2. 待确认设计决策

- 脚本语法范围:完全照搬 CE AA(含 jmp hook / registersymbol)还是精简(仅 alloc/run/free/createThread + 数据源)?
- 命令入口:`script run <file>` 独立命令组?
- 执行语义:一次调用 = 执行一个脚本(ENABLE 块),DISABLE 块如何触发(独立参数/独立调用)?

### 3. 变更范围(初稿)

| 位置 | 变更 |
|------|------|
| design/v2.3.0/cli/ | 受影响文档复制并标注改动(01/02/04/05/07/10/12/13/14/15/16/17/18) |
| cli/src/command/commands.cpp | 命令表新增 script 组 |
| cli/src/interface/cmd_script.cpp | 新命令处理器(脚本解析+执行) |
| DeepTrace/include/deeptrace.h | 视确认结果新增脚本/hook 公共 API(走 .flow/cpp_static_flow.md) |
| cli/test/ | 单元/集成/e2e 补用例 + 回归 |
| DeepTrace/test/ | 静态库补用例(如有新 API) |

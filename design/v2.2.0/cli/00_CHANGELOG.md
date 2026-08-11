# deeptrace_cli - 版本变更记录

## v2.2.0 汇编代码注入并执行(相对 v2.1.0)

> 输入:想法「传入汇编代码后在目标进程中执行」。
> 本版本为修改/添加功能型流程,版本号功能位 +1(v2.1.0 → v2.2.0)。
> 改动点清单为初稿(0.5 步建立),随分析与设计步骤细化,最终以各设计文档为准。

### 1. 改动点清单(已确认,以 04_需求分析 为准)

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | `asm file <path.asm> [--hex] [--c-array] [--out <bin>]`:汇编文件 → 字节,可选写 .bin | 新增 | command/commands.cpp、interface/cmd_asm.cpp |
| 2 | `hex2bin <hex> <output.bin>`:16 进制 → .bin 文件 | 新增 | command/commands.cpp、interface/cmd_hex2bin.cpp |
| 3 | `shellcode injectfile <path.bin>`:读 .bin 注入(立即执行) | 新增 | interface/cmd_shellcode.cpp |
| 4 | `shellcode alloc <source>`:只分配+写入,输出首地址 | 新增 | 静态库(shellcode_alloc) + interface/cmd_shellcode.cpp |
| 5 | `shellcode run <addr>`:对已记录地址触发一次(可重复) | 新增 | 静态库(shellcode_run) + interface/cmd_shellcode.cpp |
| 6 | `shellcode free <addr>`:释放+清记录 | 新增 | 静态库(shellcode_free) + interface/cmd_shellcode.cpp |
| 7 | `shellcode exec <source>`:流水线单入口(转换→写入→触发) | 新增 | 静态库 + interface/cmd_shellcode.cpp |
| 8 | 既有命令全部保留(兼容) | 不变 | 全局 |
| 9 | 版本号 2.2.0(help/version/CMake/测试断言) | 修改 | 全局 |

### 2. 已确认设计决策

- 命令形态:**独立子命令组合**(工具分开、使用时组合)。
- `shellcode inject` **保留兼容**;新增 alloc(只写入不执行)/run(触发)/free(清理)。
- .asm 文件处理:`asm file <path> [--out <bin>]`;`shellcode exec` 亦可直接吃 .asm 内存直转。
- hex → .bin:独立 `hex2bin` 命令。
- 核心原语两条:写入(alloc)/触发(run),free 清痕迹,exec 为流水线单入口(一次调用完整流程)。

### 3. 变更范围(初稿)

| 位置 | 变更 |
|------|------|
| design/v2.2.0/cli/ | 受影响文档复制并标注改动(01/02/03/04/05/07/10/12/13/14/15/16/17/18) |
| cli/src/command/commands.cpp | 命令表新增/修改 |
| cli/src/interface/cmd_asm.cpp、cmd_shellcode.cpp | 新增动作 |
| DeepTrace/include/deeptrace.h | 新增 shellcode 公共 API(走 .flow/cpp_static_flow.md) |
| cli/test/ | 单元/集成/e2e 补用例 + 回归 |
| DeepTrace/test/ | 静态库补用例 |

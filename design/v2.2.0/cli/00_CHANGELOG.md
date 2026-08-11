# deeptrace_cli - 版本变更记录

## v2.2.0 汇编代码注入并执行(相对 v2.1.0)

> 输入:想法「传入汇编代码后在目标进程中执行」。
> 本版本为修改/添加功能型流程,版本号功能位 +1(v2.1.0 → v2.2.0)。
> 改动点清单为初稿(0.5 步建立),随分析与设计步骤细化,最终以各设计文档为准。

### 1. 改动点清单(初稿,待细化)

| # | 改动点(初稿) | 新增/修改/删除 | 影响 |
|---|--------------|----------------|------|
| 1 | asm 组支持 .asm/.s 文件输入(汇编文件 → 字节),可选写出 .bin 文件 | 新增 | command/commands.cpp、interface/cmd_asm.cpp |
| 2 | shellcode 组支持 .bin 文件输入(读取文件字节 → 注入) | 新增 | interface/cmd_shellcode.cpp |
| 3 | shellcode 组新增「只分配+写入,不执行」动作,输出分配首地址 | 新增 | 静态库 + interface/cmd_shellcode.cpp |
| 4 | shellcode 组新增「对已分配地址创建远程线程触发一次」动作 | 新增 | 静态库 + interface/cmd_shellcode.cpp |
| 5 | shellcode 组新增「释放已分配地址」动作(含记录清理) | 新增 | 静态库 + interface/cmd_shellcode.cpp |
| 6 | 16 进制字节 → .bin 文件写出能力 | 新增(待定) | convert/asm 命令 |
| 7 | 版本号 2.2.0(help/version/CMake/测试断言) | 修改 | 全局 |

### 2. 待用户决策(进入需求分析前确定)

- 命令形态:独立子命令组合 / 打包单条流水线命令 / 参数化阶段控制。
- 现有 `shellcode inject`(分配+写入+立即执行)是否保持兼容。
- .asm 文件处理方式:先转 .bin 再注入 / 内存直转一条命令完成。

### 3. 变更范围(初稿)

| 位置 | 变更 |
|------|------|
| design/v2.2.0/cli/ | 受影响文档复制并标注改动(01/02/03/04/05/07/10/12/13/14/15/16/17/18) |
| cli/src/command/commands.cpp | 命令表新增/修改 |
| cli/src/interface/cmd_asm.cpp、cmd_shellcode.cpp | 新增动作 |
| DeepTrace/include/deeptrace.h | 新增 shellcode 公共 API(走 .flow/cpp_static_flow.md) |
| cli/test/ | 单元/集成/e2e 补用例 + 回归 |
| DeepTrace/test/ | 静态库补用例 |

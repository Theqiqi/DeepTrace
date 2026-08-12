# deeptrace_cli - 版本变更记录

## v2.4.0 脚本语法检查命令(相对 v2.3.0)

> 输入:用户问「此脚本功能有没有添加对语法的检查?」,确认现有解析期+执行期两层检查后,
> 提出需要一个**只检查不执行**的独立命令(可用于 CI/编辑器校验);经 ask_user 确认新增 `script check`。
> 本版本为修改/添加功能型流程,版本号功能位 +1(v2.3.0 → v2.4.0)。

### 1. 改动点清单(初稿,待 04_需求分析 确认)

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | 新命令 `script check <file>`:只检查脚本语法与汇编可编性,不 attach、不执行、无目标副作用 | 新增 | command/commands.cpp、interface/cmd_script.cpp |
| 2 | 语法检查范围:复用现有解析期检查(aa_parse_text)+ 新增执行前静态校验(hook 块结构)+ 汇编预检(占位地址汇编) | 新增 | interface/cmd_script.cpp |
| 3 | 退出码:检查失败统一退出 2(输入/脚本问题),成功输出 `OK` + 统计 | 新增 | interface/cmd_script.cpp |
| 4 | 既有命令全部保留(兼容);script run/disable/status 行为不变 | 不变 | 全局 |
| 5 | 版本号 2.4.0(help/version/CMake/测试断言) | 修改 | 全局 |

### 2. 已确认设计决策(ask_user 结果)

- 新增独立命令 **`script check <file>`**:只检查不执行,可用于 CI/编辑器校验。
- 检查内容三层:①文件读取(失败 → 退出 2) ②AA 语法解析(复用 aa_parse_text,
  失败 → 退出 2) ③执行前静态校验(ENABLE 块 hook 结构:目标后必须 jmp、label
  必须已定义)+ 汇编预检(占位地址调用静态库 asm_assemble_labels,捕获未知指令/非法
  操作数 → 退出 2)。
- **无需 attach**:check 为纯本地操作,不依赖 -p;与 script run/disable 的
  attach 路径完全隔离。
- 输出:通过 → `OK` + 统计(步骤数/hook 数/汇编指令数);失败 → `Error: script
  check failed at line N: <msg>`,退出码 2。
- 既有 script 命令行为不变;解析错误统一沿用 `script parse error at line N` 格式。

### 2.5 实现期决策补充(与 15_异常设计 同步)

- check 的静态校验严格镜像 run 的执行语义(代码审查修复):
  - asm 行无写入目标(无 alloc'd 标签切换 / 无 hook 目标在前)→ check 报错退出 2
    (与 exec_enable 的 flush_asm 同规则);
  - hook jmp 标签宇宙 = alloc 符号 ∪ hook 块内标签(与 resolve_enable 的
    ext_labels 一致),普通汇编块标签不可作为 hook jmp 目标;
  - `script check -p <pid>` 不触发 attach(executor 排除),保持纯本地。
- 已知边界:用户示例中的 `mov [addsunrcx],rcx`(非 jmp/call 符号引用)与
  `mov r12,"m.dll"+100`(asm 行内字符串立即数)超出 v2.3.0 已声明能力边界,
  check 与 run 同样报 BadFormat(能力边界一致,文档不暗示支持)。

### 3. 变更范围(初稿)

| 位置 | 变更 |
|------|------|
| design/v2.4.0/cli/ | 受影响文档复制并标注改动(00/01/04/07/10/13/14/15) |
| cli/src/command/commands.cpp | 命令表 script 组新增 check 条目 + 版本号 2.4.0 |
| cli/src/interface/cmd_script.cpp | 新增 script_check_cmd(读文件 → 解析 → 结构校验 → 汇编预检) |
| cli/src/printing/printer.cpp | 版本号 2.4.0(无新输出格式,复用 print_message) |
| cli/test/ | 单元(check 解析/结构/预检)+ 集成 + e2e 补用例 + 回归 |

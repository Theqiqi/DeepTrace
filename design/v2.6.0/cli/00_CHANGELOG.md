# deeptrace_cli - 版本变更记录

## v2.6.0 符号寻址(相对 v2.5.0)

> 输入:用户问「给 mem read / watch add 等命令加符号寻址支持(如 mem read sunObjPtr),
> 配合人造指针外部读值更方便」。即脚本内 alloc 的人造指针符号(槽位)可以在脚本外
> 的命令行地址参数里直接按名引用,由 CLI 解析为真实地址。经调查确认:**完整实现**。
> 本版本为修改/添加功能型流程,版本号功能位 +1(v2.5.0 → v2.6.0)。

### 1. 改动点清单(初稿,待 04_需求分析 确认)

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | 静态库新增公共 API `script_symbol(name, &addr)`(按名查 per-PID 脚本符号记录) | 新增 | DeepTrace/include/deeptrace.h、service/script.h/cpp |
| 2 | CLI 解析器:`address` 参数类型扩展为「数字地址 或 符号名形状」(ASCII 标识符) | 修改 | cli/src/command/parser.cpp、commands.cpp |
| 3 | CLI 接口层:`resolve_addr(s, &addr)` 先数字后符号(向后兼容),失败报 NotFound | 新增 | cli/src/interface/executor.cpp、cmd.h |
| 4 | 各地址命令接入:mem read/write/dump/readval、disasm at/range、watch add、shellcode injectat/run/free | 修改 | cli/src/interface/cmd_memory.cpp、cmd_disasm.cpp、cmd_watch.cpp、cmd_shellcode.cpp |
| 5 | 版本号 2.6.0(help/version/CMake/测试断言) | 修改 | 全局 |

### 2. 已确认设计决策

- **完整实现**:脚本 alloc 符号(人造指针槽位,如 `sunObjPtr`)可在任意取地址参数中
  按名引用(`mem read sunObjPtr 8` / `watch add ptr sunObjPtr qword` /
  `disasm at code` 等),CLI 解析为符号记录中的真实地址后照常执行。
- **解析顺序(向后兼容)**:先按数字地址解析(纯数字 / 0x 前缀十六进制),解析不出
  再按符号名查静态库 `script_symbol`。既有数字用法(如 `mem read 0x1234`)行为不变;
  符号名形状为 ASCII 标识符(`[A-Za-z_][A-Za-z0-9_]*`),与脚本符号命名一致。
- **无附加交互**:符号解析发生在 attach 之后(地址命令均需 -p),记录来源为按 PID
  持久化的 scripts.dat;`script check` 等无 attach 命令不涉及符号寻址。
- **错误语义**:符号不存在 → `NotFound`(业务错误,退出 1);名字形状非法 → 用法
  错误(退出 2),与既有 address 参数校验一致。

### 3. 能力边界(新增)

- 符号寻址仅作用于「地址型参数」(address 类型),不作用于字符串/模块名等参数。
- 解析顺序固定:数字优先。纯数字/0x 前缀十六进制先按地址解析;数字解析不出
  的标识符形状才查符号。
- **符号名形状统一收紧(审查修复)**:符号名首字符必须为字母或下划线。数字开头
  的符号名(如 `alloc(100,8)`)在脚本解析层(CLI aa 解析器)与静态库
  `script_symbol/script_alloc` 均拒绝——彻底杜绝「符号名恰好是数字 → CLI 静默
  当地址解析」的影子歧义。
- 符号必须已由脚本 `alloc` 注册(per-PID 记录存在);未注册 → NotFound。
- 目标进程需已 attach(地址命令本就要求 -p);无 session → NotAttached。

### 4. 实现期决策(代码审查后补充)

- **resolve_addr 数字判定**:以首字符分派——数字开头走数字路径(显式进制解析,
  避免 strtoull base-0 八进制陷阱如 `010`→8),字母/下划线开头走
  `deeptrace::script_symbol`。与解析器 valid_address/valid_symbol_shape 完全对齐。
- **审查修复(2 项)**:
  1. 数字开头符号名影子歧义:CLI aa 解析器与静态库 valid_symbol_name 均要求
     首字符字母/下划线,`alloc(100,8)` 类脚本 check/run 报「invalid symbol
     name」(静态库 script_alloc 直接 InvalidArg),不会在 mem read 时静默错编。
  2. 补 `disasm at <符号>` 集成正例(aptr 脚本的 code 符号可反汇编)。
- **顺带修复预存 flaky 测试**:AsmAssembleLabels/ExternalSymbol 用大写 hex 与
  Capstone 反汇编文本比对,地址含 A-F 时偶发失败;改为大小写不敏感匹配。

### 5. 验证

- 静态库:单元 115 + 集成 48(含 script_symbol 查找/NotFound/InvalidArg、
  数字开头符号名拒绝)。
- CLI:单元 154(parser 符号形状通过 + resolve_addr 数字/符号分派 + aa 数字开头
  符号名拒绝)、集成 44(script run 后 mem read <符号> 读回真实值 + disasm at
  <符号>)、e2e 186(真实进程符号寻址全链路:mem read/readval/watch add 按符号、
  未知符号 NotFound、disable 后失效)。Debug/Release 全绿。
- git:feat + fix(review findings)提交齐全,tag `v2.6.0`。

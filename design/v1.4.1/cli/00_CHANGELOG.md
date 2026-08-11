# deeptrace_cli - 版本变更记录

## v1.4.1 修正 v1.4.0 设计:独立 convert 命令,resolve scan 恢复原样(相对 v1.4.0)

> 修改型流程:本版本为修改/修复(修复位 +1)。v1.4.0 把「不同类型数据转十六进制字节」
> 错误地做进了 `resolve scan`(给 scan 增加可选 type 参数)。本版本撤销该改动:
> `resolve scan` 恢复 v1.3.0 纯 AOB pattern 行为;「数据 → 十六进制字节」实现为
> **独立命令 `convert`**,其输出格式与 `resolve scan` 的 pattern 输入完全兼容,
> 可直接作为扫描模式使用(即"匹配 scan 的搜索")。
> 静态库(deeptrace)零改动,扫描能力仍复用既有 `pattern_scan` 公共 API。

### 1. 改动点清单

| # | 改动点 | 新增/修改/删除 | 影响命令组/功能 |
|---|--------|----------------|-----------------|
| 1 | `resolve scan` 移除 v1.4.0 新增的可选 type 参数与类型值转换,恢复 `resolve scan <pattern>` 纯 AOB 行为 | 删除(v1.4.0 改动回滚) | resolve/scan |
| 2 | 新增独立命令 `convert <type> <value>`,按类型把值转换为十六进制字节并输出(整数/浮点小端、字符串 ASCII、hex 连续字节) | 新增 | convert |
| 3 | 解析器支持无子动作的独立命令(命令表 action 为空,位置参数从第 1 个开始) | 新增 | command/parser |
| 4 | 参数校验:新增 `convert-type`(类型枚举)与按类型校验的 `convert-value` 规则(跨字段校验) | 新增 | command/parser |
| 5 | 接口调用层新增 `internal::typed_bytes` 转换辅助(纯标准库),新增 `cmd_convert` 处理器与分派 | 新增 | interface |
| 6 | 版本号统一升至 v1.4.1(help/version 输出、CMake 工程版本、测试断言) | 修改 | 全局 |

### 2. 能力边界(声明支持 / 不支持)

- convert 支持:
  - 整数类型 byte(0..255)/word(0..65535)/dword(0..4294967295)/qword(0..18446744073709551615),
    十进制或 0x 前缀十六进制,多字节小端序;
  - float/double:IEEE754 小端序,仅十进制字面量(拒绝十六进制浮点如 0x1p3),必须有限值;
  - string:纯 ASCII 可打印字符(0x20..0x7E),逐字节输出;
  - hex:连续十六进制字节串(可选 0x 前缀、偶数长度),逐字节透传。
- convert 不支持:负数整数(整数解析为无符号,返回用法错误)、非 ASCII 字符串、
  大小端切换、AOB 通配符(??)。以上不支持项均返回明确 `Error:` 与退出码 2。
- 兼容性:`resolve scan` 行为与 v1.3.0 完全一致;v1.4.0 的 `resolve scan <value> [type]`
  用法不再支持(`resolve scan 100 dword` 报 too many arguments)。

### 3. 变更范围

| 位置 | 变更 |
|------|------|
| cli/src/command/commands.cpp | 回滚 resolve scan 命令表;新增 convert 命令表项;帮助文本版本号 v1.4.1 |
| cli/src/command/commands.h | 注释同步 |
| cli/src/command/parser.cpp | 删除 scan-type/scan-value 校验;新增 convert-type/convert-value 校验;无子动作命令解析 |
| cli/src/interface/cmd.h | 回滚 value_to_pattern 声明;新增 cmd_convert 与 internal::typed_bytes 声明 |
| cli/src/interface/executor.cpp | 回滚 value_to_pattern 实现;实现 typed_bytes;新增 convert 分派 |
| cli/src/interface/cmd_convert.cpp | 新增:convert 命令处理器 |
| cli/src/interface/cmd_resolve.cpp | 回滚 scan 分支,恢复直接调用 pattern_scan |
| cli/src/printing/printer.cpp | version 输出 v1.4.1 |
| cli/CMakeLists.txt | 工程版本 1.4.1 |
| cli/src/CMakeLists.txt | core 库新增 cmd_convert.cpp |
| cli/test/unit/parser_test.cpp | convert 解析正例/负例 + scan 回归 |
| cli/test/unit/executor_test.cpp | typed_bytes 转换用例 |
| cli/test/integration/cli_integration_test.cpp | 回滚类型值扫描用例;新增 convert 用例 |
| cli/test/e2e/test_cli_e2e.py | e2e:convert 用例 + convert 输出喂给 scan + 版本号断言更新 |
| cli/test/e2e/cases.md | 用例清单改为 convert |

公共 API(deeptrace.h)零改动。

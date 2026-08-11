# deeptrace_cli - 版本变更记录

## v1.4.0 scan 支持不同类型值转十六进制字节匹配(相对 v1.3.0)

> 修改型流程:本版本为功能添加(功能位 +1),只影响 CLI 的 `resolve scan` 命令及其解析/转换/调用路径;
> 静态库(deeptrace)无 API 改动,能力完全复用既有 `pattern_scan`。未改动设计文档引用 v1.0.0 cli 设计。

### 1. 改动点清单

| # | 改动点 | 新增/修改/删除 | 影响命令组/功能 |
|---|--------|----------------|-----------------|
| 1 | `resolve scan <value> [type]` 增加可选 `type` 参数(byte/word/dword/qword/float/double/string/hex/pattern,默认 pattern) | 修改 | resolve/scan |
| 2 | 按类型把输入值转换为十六进制字节模式(整数小端、浮点 IEEE754 小端、字符串 ASCII、hex 连续字节),再交给 `pattern_scan` 扫描 | 新增 | resolve/scan |
| 3 | 参数校验:新增 `scan-type`(类型枚举)与按类型校验的 `scan-value` 规则 | 新增 | command/parser |
| 4 | 接口调用层新增 `value_to_pattern` 转换辅助函数(纯标准库,不调 OS API) | 新增 | interface/executor、interface/cmd_resolve |
| 5 | 帮助文本与命令表更新 `resolve scan` 用法/简介 | 修改 | command/commands |
| 6 | 版本号统一升至 v1.4.0(help/version 输出、CMake 工程版本) | 修改 | 全局 |

### 2. 能力边界(声明支持 / 不支持)

- 支持:整数类型(byte/word/dword/qword,十进制或 0x 前缀十六进制,小端字节序)、
  float/double(IEEE754 小端)、string(纯 ASCII 可打印字符)、hex(连续十六进制字节串)、
  pattern(既有 AOB,含 ?? 通配,行为不变,向后兼容)。
- 不支持:负数输入(整数解析为无符号,负数返回用法错误)、非 ASCII 字符串、大小端切换、
  通配符与类型值混用(类型值扫描不支持 ??)。以上不支持项返回明确的 `Error:` 与退出码 2。
- 兼容性:`resolve scan <pattern>` 不传 type 时行为与 v1.3.0 完全一致。

### 3. 变更范围

| 位置 | 变更 |
|------|------|
| cli/src/command/commands.cpp | resolve scan 命令表(usage/brief/params)+ 帮助文本版本号 |
| cli/src/command/parser.cpp | 新增 scan-type / scan-value 校验规则 |
| cli/src/interface/cmd.h | 新增 `internal::value_to_pattern` 声明 |
| cli/src/interface/executor.cpp | 实现 `internal::value_to_pattern` |
| cli/src/interface/cmd_resolve.cpp | scan 分支:先转换再调用 pattern_scan |
| cli/src/printing/printer.cpp | version 输出 v1.4.0 |
| cli/CMakeLists.txt | 工程版本 1.4.0 |
| cli/test/unit/parser_test.cpp | scan 参数校验正例/负例 |
| cli/test/unit/executor_test.cpp | value_to_pattern 转换用例 |
| cli/test/integration/cli_integration_test.cpp | 类型值扫描命中真实内存用例 |
| cli/test/e2e/test_cli_e2e.py | e2e 用例:类型值扫描 + 版本号断言更新 |

公共 API(deeptrace.h)零改动。

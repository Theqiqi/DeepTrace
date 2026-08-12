# CHANGELOG — deeptrace_cli v2.10.0

> v2.10.0 = v2.9.0 + mem batch 输出导出(CSV/JSON) 改动点清单

## 改动点清单

| # | 改动点 | 新增/修改 | 影响 |
|---|--------|-----------|------|
| 1 | `mem batch <read\|write>` 新增可选参数 `--format table\|csv\|json` 与 `--out <file>` | 新增 | 命令表/参数列表/解析器/printer/cmd_memory |
| 2 | `BatchRow` 数据结构加 `status`/`error` 字段 | 修改 | printer.h、cmd_memory.cpp |
| 3 | printer 输出能力:表格保留、新增 CSV/JSON 序列化(纯函数,可单测) | 新增 | printer.h/cpp |
| 4 | 接口层落盘 `write_text_file` 辅助 | 新增 | executor.cpp/cmd.h |
| 5 | read/write 两模式均可导出;error 行 value 空、error 消息独立字段 | 新增 | cmd_memory.cpp |
| 6 | 测试:printer 格式单测、parser 参数单测、集成导出断言、e2e 导出用例 | 新增 | test/ |

## 决策记录

- `--format` 缺省 `table`,完全保持 v2.9.0 输出,向后兼容。
- `--out` 复用 convert 的 out-flag 解析机制;落盘失败 → `Error: cannot write file: <path>` 退出 1。
- CSV 列固定 `name,address,value,status,error`;RFC4180 风格引号转义(值含逗号/引号/换行时双引号包裹、内部引号加倍)。
- JSON 为对象数组 `[{"name":..,"address":..,"value":..,"status":..,"error":..}]`,字符串按 JSON 转义。
- 失败项:status=error,value 空,error 消息独立字段;表格格式失败行 value 显示 `error`(与 v2.9.0 一致)。
- 版本号:功能位 +1 → v2.10.0。

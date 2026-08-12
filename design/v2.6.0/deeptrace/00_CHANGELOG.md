# deeptrace - 版本变更记录

## v2.6.0 符号按名查询能力(相对 v2.5.0)

> 由 CLI v2.6.0 流程通过「静态库流程交接记录」触发(CLI 需求:地址参数按名引用
> 脚本符号,需要静态库提供「按符号名查 per-PID 记录地址」能力)。
> 静态库完成契约回交 CLI 后由 CLI 接口调用层消费。

### 1. 改动点清单

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | 新增公共 API `Result script_symbol(const std::string& name, uintptr_t* out_addr)`(按名查脚本符号记录地址) | 新增 | service/script.h/cpp、include/deeptrace.h |
| 2 | 版本号 2.5.0 → 2.6.0 | 修改 | 全局 |

### 2. 能力边界(声明支持 / 不支持)

- `script_symbol`:按名查当前 session 的符号记录(load_script_symbols),返回地址。
  - 未找到 → NotFound;无 session → NotAttached;名字非法/out 为空 → InvalidArg。
- 只读查询,不修改记录;与 script_alloc/script_free 的记录格式完全一致。
- 不支持:跨进程符号、未 attach 查询(需先 attach)。

### 3. 变更范围

| 位置 | 变更 |
|------|------|
| DeepTrace/include/deeptrace.h | script_symbol 声明(script 段) |
| DeepTrace/src/service/script.h/cpp | script_symbol 实现(复用 load_script_symbols) |
| DeepTrace/test/integration/process_integration_test.cpp | script_symbol 用例(真实 target) |

既有公共 API 全部保留,向后兼容。

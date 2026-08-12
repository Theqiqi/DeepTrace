# deeptrace - 版本变更记录

## v2.3.0 脚本引擎能力(相对 v2.2.0)

> 由 CLI v2.3.0 流程通过「静态库流程交接记录」触发(CLI 需求:脚本关键字引擎,
> 参考 CE AA,需要带 label 汇编 / 按大小分配+符号 / 任意地址建线程 / hook 改写恢复 /
> 脚本记录持久化 5 项能力)。
> 静态库完成契约回交 CLI 后由 CLI 接口调用层消费。

### 1. 改动点清单

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | 新增公共 API `Result script_alloc(const std::string& name, size_t size, uintptr_t* out_addr)`(按大小分配+符号绑定) | 新增 | service/script(新模块) |
| 2 | 新增公共 API `Result asm_assemble_labels(const std::string& code, const std::map<std::string,uintptr_t>& symbols, std::vector<uint8_t>& out, std::string* out_text)`(带 label 汇编) | 新增 | service/asm(扩展) |
| 3 | 新增公共 API `Result thread_create_at(uintptr_t addr, uint32_t* out_tid)`(任意地址建线程) | 新增 | service/thread(扩展) |
| 4 | 新增公共 API `Result hook_set(uintptr_t addr, uintptr_t newmem, HookInfo& out)` / `Result hook_clear(uintptr_t addr)`(hook 改写与恢复) | 新增 | service/hook(新模块) |
| 5 | 新增公共 API `Result script_enable(const std::string& path)` / `Result script_disable(const std::string& path)` / `Result script_status(std::vector<ScriptInfo>& out)`(脚本记录持久化) | 新增 | service/script |
| 6 | 版本号 2.2.0 → 2.3.0 | 修改 | 全局 |

### 2. 能力边界(声明支持 / 不支持)

- `script_alloc`:分配 PAGE_EXECUTE_READWRITE + 符号绑定,不写内容、不建线程;同名重复 → InvalidArg。
- `asm_assemble_labels`:多行汇编 + label 引用(PC-relative 按符号地址计算);未定义符号 → BadFormat。
- `thread_create_at`:任意可执行地址创建远程线程(不要求注入记录,区别于 shellcode_run)。
- `hook_set/clear`:目标地址改写为 jmp + nop 填充,原始字节持久化;clear 恢复原始字节;
  幂等语义由脚本记录层保证。
- `script_enable/disable/status`:按 PID+路径记录启用状态,幂等(重复 enable/disable → Ok)。
- 不支持:跨进程符号、模块未加载的地址表达式、非可执行内存建线程(错误返回)。
- 既有公共 API 全部保留,向后兼容。

### 3. 变更范围

| 位置 | 变更 |
|------|------|
| DeepTrace/include/deeptrace.h | script_alloc/asm_assemble_labels/thread_create_at/hook_set/hook_clear/script_enable/disable/status 声明 + HookInfo/ScriptInfo |
| DeepTrace/src/service/ | script.h/cpp(新)、hook.h/cpp(新)、asm.h/cpp(扩展)、thread.h/cpp(扩展) |
| DeepTrace/CMakeLists.txt | 工程版本 2.3.0 |
| DeepTrace/test/integration/process_integration_test.cpp | 新 API 用例(真实 target) |
| docs/api/v2.3.0/ | API 文档更新 |

既有公共 API 全部保留,向后兼容。基础设施复用:RemoteAlloc/RemoteFree/WriteRemoteMemory/
CreateRemoteThreadEx/memory_read/memory_write/asm_one(Keystone)/store(记录存储)。

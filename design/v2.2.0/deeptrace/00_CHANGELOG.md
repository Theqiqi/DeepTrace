# deeptrace - 版本变更记录

## v2.2.0 shellcode 分阶段操作(相对 v2.1.0)

> 由 CLI v2.2.0 流程通过「静态库流程交接记录」触发(CLI 需求:汇编代码注入并执行,
> 需要「写入不执行 / 单独触发 / 释放清理」分阶段能力)。
> 静态库完成契约回交 CLI 后由 CLI 接口调用层消费。

### 1. 改动点清单

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | 新增公共 API `Result shellcode_alloc(const std::vector<uint8_t>& bytes, InjectInfo& out)`(只分配+写入,不执行) | 新增 | service/inject |
| 2 | 新增公共 API `Result shellcode_run(uintptr_t addr, InjectInfo& out)`(对已记录地址创建远程线程触发一次,可重复) | 新增 | service/inject |
| 3 | 新增公共 API `Result shellcode_free(uintptr_t addr)`(释放已分配内存 + 删除记录) | 新增 | service/inject |
| 4 | 版本号 2.1.0 → 2.2.0 | 修改 | 全局 |

### 2. 能力边界(声明支持 / 不支持)

- `shellcode_alloc`:分配 PAGE_EXECUTE_READWRITE + 写入 + 记录持久化,**不创建线程**。
- `shellcode_run`:地址必须存在于注入记录(kind=shellcode);每次调用创建**一条新**远程线程;
  可重复调用(复用触发);未记录地址 → NotFound。
- `shellcode_free`:地址必须存在于注入记录;释放内存 + 删除记录;可释放经
  alloc/inject/injectat/injectfile/exec 产生的任一 shellcode 记录。
- 不支持:非记录地址的操作;非 shellcode 类型记录(dll);未 attach 会话(NotAttached)。
- 既有 shellcode_inject/inject_at/status 与全部公共 API 保留,向后兼容。

### 3. 变更范围

| 位置 | 变更 |
|------|------|
| DeepTrace/include/deeptrace.h | shellcode_alloc/run/free 声明 |
| DeepTrace/src/service/inject.h/cpp | 3 个新 Service 函数(复用既有基础设施) |
| DeepTrace/CMakeLists.txt | 工程版本 2.2.0 |
| DeepTrace/test/integration/process_integration_test.cpp | alloc/run/free 用例(真实 target) |
| docs/api/v2.2.0/ | API 文档更新(注入部分) |

既有公共 API 全部保留,向后兼容。基础设施无新增(RemoteAlloc/RemoteFree/
WriteRemoteMemory/CreateRemoteThreadEx/IsRemoteThreadRunning 均已存在)。

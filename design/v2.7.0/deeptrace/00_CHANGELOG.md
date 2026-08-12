# deeptrace - 版本变更记录

## v2.7.0 就近分配能力(相对 v2.6.0)

> 由 CLI v2.7.0 流程通过「静态库流程交接记录」触发(CLI 需求:alloc near 第三
> 参数真实就近分配 ±2GB,需要静态库提供「按锚点就近分配」基础设施与公共 API)。
> 静态库完成契约回交 CLI 后由 CLI 脚本执行层消费。

### 1. 改动点清单

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | 基础设施新增 `RemoteAllocNear`(锚点 ±2GB 窗口 VirtualQueryEx 扫描,贴近锚点) | 新增 | infrastructure/memory/memory.h/cpp |
| 2 | 新增公共 API `Result script_alloc_near(name, size, anchor, owner, out_addr)`(就近分配 + 符号记录) | 新增 | service/script.h/cpp、include/deeptrace.h |
| 3 | 就近失败 → Error,不静默回退任意分配 | 新增(语义) | service/script.cpp |
| 4 | 版本号 2.6.0 → 2.7.0(打印/命令层字符串) | 修改 | CLI 侧(printer/commands) |

### 2. 能力边界(声明支持 / 不支持)

- `RemoteAllocNear(h, size, prot, anchor, out)`:
  - 在 [anchor - 0x7FFFFFFF, anchor + 0x7FFFFFFF] 窗口内,先向上找「距锚点
    最近且 size 可容纳」的空闲区(含锚点所在区),再向下找;命中区内在
    距锚点最近位置 VirtualAllocEx。窗口内无合适空闲区 → Result::Error。
  - 支持任意 prot;与 RemoteAlloc 返回语义一致(失败 Error)。
- `script_alloc_near`:校验(NotAttached/InvalidArg)与符号记录/落盘/回滚逻辑
  与 script_alloc 完全一致,仅分配路径换成 RemoteAllocNear。
- 不支持:跨进程分配、窗口外选址(near 声明即就近,窗口内无空闲 → Error)。
- 既有公共 API 全部保留,向后兼容。

### 3. 变更范围

| 位置 | 变更 |
|------|------|
| DeepTrace/include/deeptrace.h | script_alloc_near 声明(script 段) |
| DeepTrace/src/infrastructure/memory/memory.h/cpp | RemoteAllocNear 声明/实现 |
| DeepTrace/src/service/script.h/cpp | script_alloc_near 实现(复用符号记录/落盘逻辑) |
| DeepTrace/test/integration/process_integration_test.cpp | script_alloc_near 用例(真实 target:落点在 ±2GB 内、贴近锚点、窗口无空闲报错、符号记录一致) |

### 4. 实现期决策(代码审查后补充)

- **共享 helper**:script_alloc / script_alloc_near 提取模板 `alloc_symbol`
  (校验/查重/记录/落盘/回滚单份维护),分配器以 lambda 注入——未来记录格式
  变更只改一处。
- **RemoteAllocNear** 补 out_addr 空指针守卫;向下扫描删去被 `cand >= floor`
  蕴含的冗余低界检查(加注释说明)。
- 就近失败(窗口内无空闲)→ Error 路径未在真实进程确定性构造单测(硬环境
  依赖),由算法边界与集成测试落点验证覆盖,记录为已知覆盖缺口。

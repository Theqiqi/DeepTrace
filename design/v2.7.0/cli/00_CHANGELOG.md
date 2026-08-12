# deeptrace_cli - 版本变更记录

## v2.7.0 alloc near 真实就近分配(相对 v2.6.0)

> 输入:用户问「把 alloc 的 near 第三参数改为真正就近分配(±2GB 内),消除 RIP
> 相对位移超界的概率」。即 `alloc(name,size,"module.dll"+0x1234)` 的第三参数从
> v2.5.0 记录的「放置提示,不保证就近」升级为**真实就近分配**:在锚点地址
> ±2GB 窗口内寻找空闲内存,保证后续生成的 RIP 相对跳转/位移不超界。
> 经调查确认:**完整实现**。本版本为修改/添加功能型流程,版本号功能位 +1
> (v2.6.0 → v2.7.0)。

### 1. 改动点清单(初稿,待 04_需求分析 确认)

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | 静态库基础设施新增 `RemoteAllocNear`(锚点 ±2GB 窗口扫描空闲区,贴近锚点) | 新增 | DeepTrace/src/infrastructure/memory/memory.h/cpp |
| 2 | 静态库新增公共 API `script_alloc_near(name,size,anchor,&addr)`(就近分配 + 符号记录) | 新增 | DeepTrace/include/deeptrace.h、service/script.h/cpp |
| 3 | CLI:resolve_enable 解析出的 near 锚点地址传入 script_alloc_near(原来被丢弃) | 修改 | cli/src/interface/cmd_script.cpp |
| 4 | 就近分配失败回退:窗口内无空闲 → 报错(不静默退化为任意分配) | 修改 | 静态库 + CLI 错误语义 |
| 5 | 版本号 2.7.0(help/version/CMake/测试断言) | 修改 | 全局 |

### 2. 已确认设计决策

- **完整实现**:`alloc(name,size,"module.dll"+0x1234)` 第三参数作为锚点,分配在
  锚点 ±2GB(RIP 相对位移上限)窗口内,消除 RIP 相对跳转超界概率。锚点表达式
  语法沿用 v2.5.0(模块基址 + 偏移 / 裸地址),解析与模块加载校验不变。
- **窗口语义**:以锚点为中心 ±2GB(0x7FFFFFFF)。优先选择距锚点最近且满足
  size 的空闲地址段;就近失败(窗口内无足够空闲)时返回错误,不静默回退
  任意分配——保证「声明 near 就真的 near」。
- **无 near 第三参数**:行为完全不变(OS 任意选址),与 v2.5.0 一致。
- **错误语义**:near 表达式非法 → InvalidArg(解析层,退出 2);窗口内无空闲区
  → AllocFault(业务错误,退出 1);不改变既有错误码体系。
- **兼容性**:既有脚本(不写第三参数或写了未用的)行为不变;`script check`
  校验规则不变;人造指针/符号寻址(v2.6.0)不受影响。

### 3. 能力边界(新增)

- 就近分配窗口 = 锚点 ±2GB(RIP 相对 disp32 的理论上限),x64 下有效。
- 窗口扫描基于 VirtualQueryEx 遍历锚点两侧空闲区;命中「距锚点最近且足够大」
  的空闲段,在该段内取距锚点最近的位置。
- 就近分配仅作用于带 near 第三参数的 `alloc`;`createThread`/hook 的
  `alloc(near)` 亦经同一入口,自动获得就近语义。
- 锚点地址落在目标进程内任意位置均可(模块内/堆内/自定义内存),不要求锚点
  本身已提交(仅作参考点)。
- 若锚点 ±2GB 窗口被占满(极端布局),分配失败并报错,用户可换锚点或去掉
  near。

### 4. 实现期决策(代码审查后补充)

- **就近分配算法**(静态库 RemoteAllocNear):锚点 ±2GB(0x7FFFFFFF)窗口内
  两遍 VirtualQueryEx 扫描——先向上(含锚点所在空闲区 → 距锚点最近可能位置),
  再向下;命中区内 64KB 对齐候选,窗口内无空闲 → Error,绝不静默回退任意
  选址。实测(模块基址为锚点)落点距锚点约 127KB。
- **CLI 锚点产出**:resolve_enable 新增 near_anchors(alloc 符号名 → 锚点),
  模块形式 resolve_base+偏移、裸地址直接解析;exec_enable 按是否有锚点分派
  script_alloc_near / script_alloc(无 near 行为完全不变)。
- **审查修复(4 项)**:
  1. script_alloc / script_alloc_near 提取共享模板 helper `alloc_symbol`
     (校验/查重/记录/落盘/回滚单份维护),消除 ~90% 重复。
  2. resolve_enable 对 near 表达式的二次解析加防御:不可达的失败路径也设置
     out_module(回显原始表达式),避免空 `NotFound()` 消息。
  3. RemoteAllocNear 补 out_addr 空指针守卫(与兄弟包装器一致)。
  4. 向下扫描删去冗余低界检查(cand ≥ floor 已蕴含 ≥ low),加注释说明。
- **已知覆盖缺口**:锚点窗口内无空闲区 → Error 路径难以在真实进程确定性构造,
  未做独立用例(由算法边界与错误语义文档覆盖);base+offset 锚点求和按实际
  小偏移假设,不饱和处理(注释声明)。

### 5. 验证

- 静态库:单元 115 + 集成 49(含 ScriptAllocNear:落点在 ±2GB 内、符号查找
  一致、重复名/非法参/NotAttached 错误路径)。
- CLI:单元 154 + 集成 45(ScriptAllocNearPlacement:真实进程落点验证 +
  人造指针/符号寻址全链路仍绿) + e2e 195(+9 near 用例)。Debug/Release 全绿。
- git:init/idea/design/feat/fix(review findings)提交齐全,tag `v2.7.0`。

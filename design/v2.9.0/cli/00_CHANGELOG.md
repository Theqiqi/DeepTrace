# deeptrace_cli - 版本变更记录

## v2.9.0 批量定位器 JSON 配置(mem batch read/write,相对 v2.8.0)

> 输入:用户问「读取指针链需要吗?现在有没有读取地址+偏移的入口?加指针链搜索
> 还是用现有功能组合?结构化数据存储地址与偏移+支持模块解析地址」。经多轮讨论
> 定稿:**不做指针链专用命令**,采用「CLI 保持基本显性能力 + JSON 配置承载复杂
> 交互」模型——`mem batch read/write <file.json>` 批量执行 JSON 中定义的
> 定位器(指针链/模块+偏移/符号+偏移/绝对地址),零静态库改动。
> 本版本为新建功能型流程,版本号功能位 +1(v2.8.0 → v2.9.0)。

### 1. 改动点清单(初稿,待 04_需求分析 确认)

| # | 改动点 | 新增/修改/删除 | 影响 |
|---|--------|----------------|------|
| 1 | 新模块 batch:JSON 定位器解析器(OffsetPath 结构 + 校验) | 新增 | cli/src/interface/batch.h/cpp |
| 2 | `mem batch read <file.json>`:批量解析定位器 → 最终地址 → 按 type 读值 → 表格打印 | 新增 | cli/src/interface/cmd_memory.cpp |
| 3 | `mem batch write <file.json>`:批量解析定位器 → 最终地址 → 写 value | 新增 | 同上 |
| 4 | 顶层 process 校验:attach 后比对目标进程名(可选字段) | 新增 | batch 执行逻辑 |
| 5 | 类型 8 种:byte/word/dword/qword/float/double/string/bytes(+count) | 新增 | 格式化逻辑 |
| 6 | 版本号 2.9.0(help/version/测试断言) | 修改 | 全局 |

### 2. 已确认设计决策

- **模型**:JSON 文件 = 一组「定位器」(OffsetPath)。定位器 = 寻址步骤序列;
  单级(绝对地址/符号+偏移/模块+偏移)即「地址列表」,多级(offsets 数组)
  即「指针链」——同一格式通吃,批量读顺带支持。
- **JSON 结构**(用户定稿,扁平无嵌套):
  ```json
  { "version": 1, "process": "Game.exe",
    "values": {
      "player_health": { "base": "0x123456", "offsets": ["0x10","0x20"], "type": "float" },
      "sun_ptr":       { "symbol": "sunObjPtr", "offsets": ["0x8"], "type": "qword" },
      "abs":           { "base": "0x140001000", "type": "dword" },
      "name":          { "base": "0x100", "offsets": ["0x40"], "type": "string" },
      "buf":           { "base": "0x200", "type": "bytes", "count": 16 } }
  }
  ```
  寻址源三态互斥:`module`+`base`(模块内偏移) / `symbol`(脚本符号) /
  `base`(无 module,绝对地址);`offsets` 为多级偏移数组(每级先读 qword
  指针再偏移);`type` 8 种;`count` 仅 bytes 用;写模式每项多 `value`。
- **解析语义**:addr = module?module_base+base : symbol?script_symbol :
  base;逐级 *(addr)+off;最终地址;read 按 type 读值,write 写 value。
- **命令命名**(用户选定):`mem batch read <file>` / `mem batch write <file>`,
  挂 mem 组(批量读写属内存操作)。
- **输出**(用户选定):表格式 —— name + 最终地址 + 值,每行两列信息
  (地址 + 值),解决「读出来是地址还是值」的纠结。
- **文件即存储**:CLI 不提供 save/list 管理命令,用户自行维护 .json。
- **不做**:指针链搜索(CE pointer scan 反查,后续独立版本);独立 ptr
  命令组(用户否掉,批量能力挂 mem 组)。

### 3. 能力边界(声明支持 / 不支持)

- 支持:批量定位器读/写;寻址源三态;多级偏移链;8 种类型;process 校验。
- 不支持/错误:
  - version ≠ 1 → 用法错误(退出 2)。
  - 寻址源冲突(module+symbol 同现 / 缺寻址源)→ 用法错误(退出 2)。
  - type 非法 / count 缺失或非法(仅 bytes)→ 用法错误(退出 2)。
  - 模块未加载 → NotFound(退出 1);符号未注册 → NotFound(退出 1);
    链中间读失败(指针无效)→ ReadFault(退出 1);process 不匹配 → 退出 1。
  - 写模式 value 缺失 → 用法错误(退出 2);值类型非法 → 用法错误。
- 符号寻址(v2.6.0)、near 就近分配(v2.7.0)、mem write 符号(v2.8.0)
  等既有能力全部不变。

### 4. 实现期决策(代码审查后补充)

- (待补充)

### 5. 验证

- 单测(batch JSON 解析器:合法/非法/三态/类型映射)+ 集成(真实进程:
  module+base 链、符号+偏移链、绝对地址、string、bytes、批量写后读回)+
  e2e。
- git:流程每步提交齐全,tag `v2.9.0`。

# CHANGELOG — deeptrace_cli v2.12.0

> v2.12.0 = v2.11.0 + 指针链搜索(pointer map 两段式,静态库 + CLI 双改动)

## 改动点清单

| # | 改动点 | 新增/修改 | 影响 |
|---|--------|-----------|------|
| 1 | 静态库线程池(Infrastructure/threadpool,纯标准库) | 新增 | deeptrace |
| 2 | pointer map 快照 `pointer_map_snapshot`(反向反推指针链) | 新增 | deeptrace.h |
| 3 | pointer map 重扫 `pointer_map_rescan`(交集过滤假阳性) | 新增 | deeptrace.h |
| 4 | 默认模块锚定(链起点落模块基址,可配置关闭) | 新增 | 静态库 |
| 5 | CLI `resolve ptrscan <file.json>`(JSON 配置驱动) | 新增 | resolve 组 |
| 6 | 链输出兼容 mem batch 定位器输入(搜索→验证闭环) | 新增 | printer |
| 7 | 版本号 2.12.0 | 修改 | 全局 |

## 决策记录

- **两段式**(用户选 B):快照 + 重扫交集过滤,CE 完整方案,抗假阳性。
- **参数全开放**:max_offset/max_level 可调,默认 2048/5(同 CE)。
- **默认模块锚定**:链起点须落在指定模块基址内(如 game.exe),可配置关闭。
- **线程池自实现**(纯标准库 std::thread + hardware_concurrency,无三方依赖);
  线程数经 API 参数传入(默认 hardware_concurrency)。
- 版本号:功能位 +1 → v2.12.0。

## 实现期决策(v2.12.0 定稿后补充)

- **max_offset 允许 0**:0 = 精确指针匹配(链终值须等于目标地址,delta 为 0),
  用于扫描测试与精确场景;参数列表已同步(>= 0)。
- **配置错误输出约定**:`resolve ptrscan` 的 JSON/校验错误走 stderr
  (`print_error`,前缀 Error:),与 `mem batch` 一致;退出码 2。
- **JSON 数字字段为十进制**:target/地址字段用字符串(如 "0x7FF..."),
  max_offset/max_level/max_results/threads 等数字字段只接受十进制 JSON
  数字(JSON 规范不支持 0x 字面量,`"max_offset":0x1000` 会报
  "expected ',' or '}'" 解析错误)。
- **指针值写入字节序**:`mem write <addr> <hex> hex` 按存储字节序消费
  (小端),与 `mem read hex` 输出一致;写 8 字节指针须先转小端 hex。
- **多路径链保留**:同一槽位指向多个层目标(±max_offset 重叠)时,所有
  链路径均保留(next_chains 按槽位存路径向量,审查修复)。
- **共享 JSON 解析器**:batch(v2.9.0)的迷你 JSON 解析器提取为
  interface/json.h/cpp,prefix 参数化错误前缀(batch/ptrscan),batch
  公共行为不变(回归测试锁定)。
- **审查修复**:Candidate.final_addr 死代码移除;scan_pointers_to_any
  多目标歧义补充注释(链为候选,重扫验证);线程池补单测(wait/pending
  语义,enqueue→wait→enqueue 复用)。

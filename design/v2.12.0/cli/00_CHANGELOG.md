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

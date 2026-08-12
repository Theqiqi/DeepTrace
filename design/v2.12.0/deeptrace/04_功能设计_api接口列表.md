# deeptrace - 功能设计 / API 接口列表(修改模式,v2.12.0)

> 引用:既有 API 表。本版本新增:

## 1. 新增公共 API

```cpp
// 指针链配置。
struct PointerScanConfig {
    uintptr_t target = 0;       // 目标值地址(反向反推起点)
    uint32_t max_offset = 2048; // 每层指针指向目标 ±max_offset 视为命中
    uint32_t max_level = 5;     // 链最大深度(层数)
    uint32_t max_results = 10000; // 快照输出上限(防假阳性爆炸)
    std::string module;         // 锚定模块名(空 = 不锚定,默认锚定需 CLI 填)
    uint32_t thread_count = 0;  // 0 = hardware_concurrency
};

// 一条指针链:根地址 + 偏移序列。求值 addr=root; for off: addr=*(addr)+off。
struct PointerChain {
    uintptr_t root = 0;
    std::vector<uintptr_t> offsets;
};

// 反向反推指针链快照(全内存扫描 + 递归,线程池并行)。
// 未 attach → NotAttached;cfg 非法(max_offset=0 / target=0)→ InvalidArg;
// 锚定模块未加载 → NotFound。无结果 → Ok + 空列表。
Result pointer_map_snapshot(const PointerScanConfig& cfg,
                            std::vector<PointerChain>& out);

// 重扫:用新目标地址过滤旧链列表,仅保留仍解析到 new_target±max_offset 的链。
// 未 attach → NotAttached;空输入 → Ok + 空。
Result pointer_map_rescan(const std::vector<PointerChain>& base,
                          uintptr_t new_target,
                          const PointerScanConfig& cfg,
                          std::vector<PointerChain>& out);
```

## 2. 内部改动(不暴露)

- Infrastructure/threadpool/threadpool.h/cpp:内部 ThreadPool(纯标准库)。
- Algorithm 层 pointer_scan.h/cpp:纯内存 buffer 的反向反推算法
  (scan_pointers_to(data, len, target, max_offset))。
- Infrastructure memory:复用 EnumMemoryRegions + memory_read 分块框架。
- Service resolve.h/cpp:pointer_map_snapshot/rescan 组装(区域枚举 →
  分块读 → 线程池并行算法 → 锚定/截断)。

## 3. 数据结构

```cpp
namespace deeptrace {   // 公共(domain/types.h)
struct PointerScanConfig { ... };
struct PointerChain { ... };
}
```

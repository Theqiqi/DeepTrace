# deeptrace - 功能设计 / API 接口列表(v2.7.0,增量)

## 1. 新增公共 API

```cpp
// Allocate memory near a given anchor (within +/-2GB) and register it as a
// script symbol, mirroring script_alloc's record/save/rollback semantics.
// name   : script symbol name (ASCII identifier)
// size   : bytes to allocate (> 0)
// anchor : reference address; the allocation lands within anchor +/-2GB,
//          preferring the free region closest to the anchor
// owner  : script path owning the symbol record
// out_addr: receives the remote address on Ok
// Returns: Ok / Error (no free region within +/-2GB, or record save failed) /
//          NotAttached (no session) / InvalidArg (bad args)
Result script_alloc_near(const std::string& name, size_t size, uintptr_t anchor,
                         const std::string& owner, uintptr_t* out_addr);
```

## 2. 与既有 API 关系

| API | 关系 |
|-----|------|
| script_alloc | 任意选址版;script_alloc_near 为其 near 变体,记录/落盘/回滚完全一致 |
| script_free / script_symbol / script_status | 记录格式不变,自动兼容 near 分配出的符号 |
| RemoteAlloc | 裸 VirtualAllocEx;RemoteAllocNear 为其就近版(基础设施内部,不对外) |

## 3. 复用

- `valid_symbol_name` / `symbol_exists`(script.cpp 匿名命名空间,复用)
- `load_script_symbols` / `save_script_symbols`(store.h,复用)
- `RemoteAlloc` 的失败语义(Error)作为 RemoteAllocNear 失败语义基线

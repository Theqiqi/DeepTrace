# deeptrace - 功能设计 / API 接口列表(v2.6.0,增量)

## 1. 新增公共 API

```cpp
// Look up a script symbol's recorded address by name for the current session.
// Only reads the per-PID script symbol record; does not modify it.
// name   : script symbol name (ASCII identifier)
// out_addr: receives the recorded address on Ok
// Returns: Ok / NotFound (symbol not registered) /
//          NotAttached (no session) / InvalidArg (bad name or null out)
Result script_symbol(const std::string& name, uintptr_t* out_addr);
```

## 2. 与既有 API 关系

| API | 关系 |
|-----|------|
| script_alloc | 创建记录(script_symbol 查得到);重复名 → InvalidArg |
| script_free | 删除记录(script_symbol 随后 NotFound) |
| script_status | 全量列表(script_symbol 为按名单查,互补) |

## 3. 复用

- `valid_symbol_name`(script.cpp 匿名命名空间,复用)
- `load_script_symbols(pid)`(store.h,复用)
- 无新增基础设施。

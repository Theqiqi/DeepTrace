# deeptrace - 功能设计 / API 接口列表(修改模式,v2.11.0)

> 引用:既有 API 表。本版本新增:

## 1. 新增公共 API

```cpp
// 查询当前会话 attach 实际获得的进程权限掩码(access mask,与 Windows
// PROCESS_* 位一致)。未 attach → NotAttached;out_mask 为空 → InvalidArg。
Result session_permissions(uint32_t* out_mask);
```

## 2. 内部改动(不暴露)

- `Session` 增加 `uint32_t permissions = 0;`(service/session.h)。
- `attach` 成功时把实际 access 掩码写入 `Session.permissions`;
  `detach` 时清零。

## 3. 语义定义(权限位 → 人类/AI 语义名,CLI 打印层使用)

| 位 | PROCESS_* 名 | 语义名 |
|----|--------------|--------|
| 0x0001 | PROCESS_TERMINATE | terminate |
| 0x0002 | PROCESS_CREATE_THREAD | create_thread |
| 0x0008 | PROCESS_VM_OPERATION | vm_operate |
| 0x0010 | PROCESS_VM_READ | read |
| 0x0020 | PROCESS_VM_WRITE | write |
| 0x0040 | PROCESS_DUP_HANDLE | dup_handle |
| 0x0080 | PROCESS_CREATE_PROCESS | create_process |
| 0x0100 | PROCESS_SET_QUOTA | set_quota |
| 0x0200 | PROCESS_SET_INFORMATION | set_info |
| 0x0400 | PROCESS_QUERY_INFORMATION | query |
| 0x0800 | PROCESS_SUSPEND_RESUME | suspend_resume |
| 0x1000 | PROCESS_QUERY_LIMITED_INFORMATION | query_limited |

> 语义名是 CLI 展示约定(人类/AI 友好),静态库只返回掩码,不做名映射。

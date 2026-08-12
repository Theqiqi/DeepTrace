# deeptrace - 功能设计 / API 接口列表(修改模式,v2.2.0)

> 引用:v2.0.0/deeptrace/04_功能设计_api接口列表.md 既有 API 表。本版本新增:

## 1. 新增公共 API

```cpp
// 分配 + 写入 shellcode,不创建线程;记录持久化(running=false)。
Result shellcode_alloc(const std::vector<uint8_t>& bytes, InjectInfo& out);

// 对已记录地址(kind=shellcode)创建一条新远程线程执行;记录 TID 更新。
Result shellcode_run(uintptr_t addr, InjectInfo& out);

// 释放已记录地址的远程内存并删除记录。
Result shellcode_free(uintptr_t addr);
```

- `InjectInfo` 复用既有公共数据结构(kind/remote_base/thread_id/running/size)。
- 三个 API 均复用既有 `InjectRecord`(kind=shellcode, hex 字节, addr, tid)存储。

## 2. 基础设施复用(内部,不新增)

| 基础设施 | 用途 | 既有 |
|----------|------|------|
| RemoteAlloc | 分配 PAGE_EXECUTE_READWRITE | infrastructure/memory |
| WriteRemoteMemory | 写入字节 | infrastructure/memory |
| RemoteFree | 释放(VirtualFreeEx) | infrastructure/memory |
| CreateRemoteThreadEx | 创建远程线程 | infrastructure/inject |
| load_injects / save_injects | 记录持久化 | service/store |

## 3. 行为说明

- `shellcode_alloc`:bytes 非空 + 已 attach → 分配 → 写入 → 记录(kind=shellcode,
  hex, addr, tid=0);写入失败 → RemoteFree 后返回错误(无残留)。
- `shellcode_run`:按 addr 在记录中查找 kind=shellcode(NotFound)→
  CreateRemoteThreadEx(addr, 0) → 更新记录 thread_id → out 填充(running=true)。
- `shellcode_free`:按 addr 查找 kind=shellcode(NotFound)→ 等待记录线程结束
  (5s 超时返回 Timeout 不释放)→ 删除记录并保存 → RemoteFree。

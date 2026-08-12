# deeptrace - 功能设计 / API 接口列表(修改模式,v2.3.0)

> 引用:v2.2.0/deeptrace/04_功能设计_api接口列表.md 既有 API 表。本版本新增:

## 1. 新增公共 API

```cpp
// 按大小分配远程内存(PAGE_EXECUTE_READWRITE)并绑定符号名(脚本符号表,按 PID 持久化)。
// 不写内容、不建线程。同名重复 → InvalidArg。
Result script_alloc(const std::string& name, size_t size, uintptr_t* out_addr);

// 带 label 的汇编:多行文本 + 符号表(name→addr),jmp/call 引用按符号地址计算
// PC-relative 位移。未定义符号 → BadFormat。
Result asm_assemble_labels(const std::string& code,
                           const std::map<std::string, uintptr_t>& symbols,
                           std::vector<uint8_t>& out, std::string* out_text);

// 在任意可执行地址创建远程线程执行一次(不要求注入记录,区别于 shellcode_run)。
Result thread_create_at(uintptr_t addr, uint32_t* out_tid);

// hook:目标地址改写为 jmp newmem(+ nop 填充),保存原始字节到 hook 记录(按 PID 持久化)。
Result hook_set(uintptr_t addr, uintptr_t newmem, HookInfo& out);
// 恢复目标地址原始字节,删除 hook 记录。无记录 → NotFound。
Result hook_clear(uintptr_t addr);

// 脚本启用状态持久化(按 PID + 路径),幂等:重复 enable/disable → Ok。
Result script_enable(const std::string& path);
Result script_disable(const std::string& path);
Result script_status(std::vector<ScriptInfo>& out);
```

## 2. 新增公共数据结构

```cpp
// hook 记录信息
struct HookInfo {
    uintptr_t target = 0;       // hook 目标地址
    uintptr_t newmem = 0;       // 跳转目标(新内存)
    std::vector<uint8_t> orig_bytes;  // 被覆盖的原始字节
    size_t size = 0;            // 覆盖长度(改写区字节数)
};

// 脚本记录信息
struct ScriptInfo {
    std::string path;           // 脚本文件路径(脚本身份)
    std::string state;          // "enabled" | "disabled"
    std::vector<HookInfo> hooks;        // 该脚本关联的 hook(启用时)
    std::vector<std::pair<std::string, uintptr_t>> allocs;  // 符号→地址
};
```

## 3. 基础设施复用(内部,不新增)

| 基础设施 | 用途 | 既有 |
|----------|------|------|
| RemoteAlloc | script_alloc 分配 | infrastructure/memory |
| RemoteFree | dealloc 释放 | infrastructure/memory |
| CreateRemoteThreadEx | thread_create_at | infrastructure/inject |
| memory_read / memory_write | hook 读原字节/写 jmp/恢复 | infrastructure/memory |
| asm_one(Keystone) | asm_assemble_labels 逐条编码 | infrastructure/assembly |
| load/save/store | 脚本符号表/hook 记录持久化 | service/store(扩展记录类型) |

## 4. 行为说明

- `script_alloc`:已 attach + name 非空 + size>0 + 符号表无同名 → RemoteAlloc →
  符号登记;失败无残留。
- `asm_assemble_labels`:两遍处理——先解析 label 定义位置(逐条 asm_one 估长),
  再以符号地址重编码含引用的指令(Keystone 绝对地址编码 rel 位移)。
- `thread_create_at`:CreateRemoteThreadEx(addr, 0) → tid 输出(不要求记录)。
- `hook_set`:读目标原始字节(长度= max(5, 需覆盖长度))→ 计算 `jmp newmem` 编码 →
  写入 + nop 填充 → 保存记录;`hook_clear`:读记录 → 写回原始字节 → 删记录。
- `script_enable/disable`:脚本记录文件按 PID 存储(scripts.dat),幂等判定。

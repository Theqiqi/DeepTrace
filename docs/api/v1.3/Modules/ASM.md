# 模块:汇编

x64 汇编器(基于 Keystone)。**不要求会话**,可独立使用。

## deeptrace::asm_assemble

### 语法

```cpp
Result asm_assemble(const std::string& code, std::vector<uint8_t>& out,
                    std::string* out_text);
```

### 参数

| 参数 | 类型 | 说明 |
|------|------|------|
| `code` | `const std::string&` | 汇编指令文本,多语句用 `;` 或换行分隔 |
| `out` | `std::vector<uint8_t>&` | 输出参数,汇编后的机器码字节 |
| `out_text` | `std::string*` | 可选,机器码的十六进制文本;传 `nullptr` 忽略 |

### 返回值

| 返回值 | 含义 |
|--------|------|
| `Result::Ok` | 全部语句汇编成功 |
| `Result::BadFormat` | 存在无法汇编的指令 |

### 说明

将 x64 汇编文本汇编为机器码。支持多语句输入,语句间以 `;` 或换行分隔;空语句自动
跳过。任一语句汇编失败即整体返回 `BadFormat`(此时 `out` 为空)。典型用途:生成
Shellcode 字节后配合 `shellcode_inject` 注入,或构造跳转/补丁字节。

前置条件:无。后置条件:无。

### 示例

```cpp
std::vector<uint8_t> bytes;
if (deeptrace::asm_assemble("mov rax, 1; ret", bytes, nullptr) == deeptrace::Result::Ok) {
    // bytes == { 48 C7 C0 01 00 00 00, C3 }
}
```

### 头文件

```cpp
#include "deeptrace.h"
```

### 参见

- [deeptrace::shellcode_inject](INJECT.md#deeptraceshellcode_inject)

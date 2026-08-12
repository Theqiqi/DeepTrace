# CLI - 变更记录(独立小版本,v2.13.0)

> 上版本:v2.12.0(指针链搜索)。本版本:转换层闭环补 2 个缺口。

## 需求变更清单

| # | 变更 | 类型 | 影响命令组 |
|---|------|------|------------|
| 1 | `disasm file <path.bin> [count]`:本地 .bin 反汇编(离线分析 shellcode) | 新增 | disasm |
| 2 | `bin2hex <path.bin> [format]`:.bin → hex 字节(hex2bin 逆操作) | 新增 | 新组 bin2hex |
| 3 | 静态库 `disasm_buffer` 公共 API(纯 buffer 解码,零会话依赖) | 新增 | 静态库 |
| 4 | 版本号 2.13.0 | 修改 | 全局 |

## 决策记录

- **职责分层**:转换层(纯数据、无进程、可离线)补齐 asm↔bin↔hex 闭环;
  脚本层(db 小片段)与 shellcode 层(独立载荷生命周期)职责正交,不重复。
- **不引入 Linux 工具**:汇编/反汇编引擎(Keystone/Capstone)已内嵌,
  外部工具(nasm/objdump/xxd)引入第二套语法语义,Windows 用户环境未必有 WSL。
- `disasm file` 复用 `print_instructions`(与进程内 disasm 同款输出),
  基址从 0 开始;count 默认 100。
- `bin2hex` 复用 `print_bytes_formatted`(hex 默认,可选 dec/bin/ascii/c-array),
  输出可直接喂回 `shellcode inject` / `hex2bin`。
- 版本号:功能位 +1 → v2.13.0。

## 实现期决策(v2.13.0 定稿后补充)

- **disasm_buffer 公共 API**:纯 buffer 解码 + base_addr 参数(进程内存用
  实际地址,本地文件用 0);disasm_at 重构为读内存 + decode_buffer,行为不变
  (ReadFault/InvalidArg/截断语义回归锁定)。
- **`disasm file` 无会话依赖**:needs_session_attach 排除 disasm/file,
  `-p 死进程 disasm file` 也能正常工作(纯数据操作)。
- **空文件语义**:disasm file → 0(打印表头,0 条指令,与 disasm at 一致);
  bin2hex → 2(empty file,无内容可输出)。
- **c-array 复用**:printer::print_bytes_formatted 新增 c-array 分支(与
  `asm assemble --c-array` 同形状),cmd_asm 的 c-array 分支改为委托调用。
- **hex2bin 输入为紧凑 hex**(无空格,parser valid_hex_bytes 既有规则),
  e2e 往返用例用 "9090C3"。
- **审查修复**:-p 多余 attach 排除、cmd_asm c-array 委托、bin2hex
  dec/bin/ascii 集成断言、空文件文档措辞。

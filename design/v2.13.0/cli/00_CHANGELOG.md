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

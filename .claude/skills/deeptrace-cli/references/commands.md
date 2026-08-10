# deeptrace_cli 命令完整参考

> 所有语法与真实 `deeptrace_cli -h` 一致;输出样本来自真实运行。
> 通用:`deeptrace_cli [选项] <组> <动作> [参数...]`;选项 `-p <pid>` 大多数命令必需。

## ps — 进程

| 命令 | 说明 |
|------|------|
| `ps list` | 列出所有进程(列:PID/NAME/THREADS/PPID) |
| `ps attach <pid>` | 附加到进程(输出 OK) |
| `ps detach` | 分离当前进程(输出 OK) |
| `ps info` | 当前进程信息(PID/Name/Threads/ParentPID) |
| `ps suspend` / `ps resume` | 挂起/恢复进程(输出 OK) |
| `ps kill [exit_code]` | 结束进程(默认退出码 0) |

## mem — 内存

| 命令 | 说明 |
|------|------|
| `mem read <addr> [size] [format]` | 读内存;format=hex\|dec\|bin\|ascii,默认 hex,默认 size=1 |
| `mem write <addr> <value> [format]` | 写内存;format=hex\|dec,默认 hex(如 `CAFEBABE` 是 4 字节) |
| `mem dump <addr> <size>` | 十六进制转储(地址+字节+可打印字符) |
| `mem regions` | 内存区域列表(BASE/SIZE/PROTECTION/STATE) |
| `mem readval <addr> <type>` | 读类型化数值;type=byte\|word\|dword\|qword\|float\|double |

## module — 模块

| 命令 | 说明 |
|------|------|
| `module list` | 已加载模块列表(BASE/SIZE/NAME) |
| `module find <name>` | 按名查找模块 |
| `module base <name>` | 模块基址(输出单行地址) |
| `module exports <module>` | 导出函数列表(ADDRESS/NAME) |
| `module dump <name> [output_file]` | 转储模块内容(hex 或存文件) |

## thread — 线程

| 命令 | 说明 |
|------|------|
| `thread list` | 线程列表(TID/PRIORITY/START) |
| `thread suspend <tid>` | 挂起线程 |
| `thread resume <tid>` | 恢复线程 |
| `thread kill <tid>` | 结束线程 |

## debug — 调试

| 命令 | 说明 |
|------|------|
| `debug attach` | 进入调试(不终止目标;会话不跨命令) |
| `debug detach` | 退出调试(单独运行报 NotAttached 属正常) |
| `debug pause` / `debug resume` | 暂停/恢复(直接用 `-p` 即可,不必先 attach) |
| `debug step [tid]` | 单步进入(tid=0 默认首线程) |
| `debug next [tid]` | 单步跳过 |
| `debug break <addr>` | 软件断点(输出 `breakpoint set at ... (orig 0x..)`) |
| `debug clear <addr>` | 清除软件断点 |
| `debug hbreak <addr> [type] [len]` | 硬件断点;type=0执行/1写/2读写,默认 0 1 |
| `debug hclear <addr>` | 清除硬件断点 |
| `debug guard <addr> <size>` | 页守卫断点 |
| `debug unguard <addr> <size>` | 移除页守卫 |
| `debug status` | 调试状态(attached/pid/breakpoints/hw_breakpoints) |
| `debug registers [tid]` | 全部寄存器(REG/VALUE,含 rip/eflags) |
| `debug register <name> [tid]` | 单个寄存器(输出 `rip = 0x...`) |

## disasm — 反汇编

| 命令 | 说明 |
|------|------|
| `disasm at <addr> [count]` | 指定地址反汇编(默认 10 条;ADDRESS/BYTES/INSTRUCTION 三列) |
| `disasm range <start> <end>` | 范围反汇编 |

## resolve — 解析

| 命令 | 说明 |
|------|------|
| `resolve base <module>` | 模块基址 |
| `resolve scan <pattern>` | AOB 特征码扫描;`??` 通配任意字节,如 `"48 8B ?? ?? 00"`;输出匹配地址列表 |

## watch — 监视

| 命令 | 说明 |
|------|------|
| `watch list` | 监视列表(IDX/DESCRIPTION/ADDRESS/TYPE/VALUE/VALID;读实时值) |
| `watch add <desc> <addr> <type>` | 添加监视 |
| `watch remove <index>` | 按序号删除(序号是 IDX 列) |
| `watch refresh` | 刷新全部监视值 |
| `watch clear` | 清空监视 |

## dll — DLL 注入

| 命令 | 说明 |
|------|------|
| `dll inject <path>` | 注入 DLL(路径用 Windows 格式;64 位进程需 64 位 DLL) |
| `dll eject <path-or-address>` | 卸载注入的 DLL |
| `dll list` / `dll status` | 注入记录(KIND/PATH/ADDRESS/TID/RUNNING) |

## asm — 汇编

| 命令 | 说明 |
|------|------|
| `asm assemble <code> [--hex] [--c-array]` | 汇编为字节;多指令 `;` 分隔;**默认输出即 hex**(`--hex` 为显式指定);`--c-array` 输出 `unsigned char code[] = {...};` |

## shellcode — 壳码

| 命令 | 说明 |
|------|------|
| `shellcode inject <hex_bytes>` | 注入壳码(自动分配内存) |
| `shellcode injectat <addr> <hex_bytes>` | 指定地址注入 |
| `shellcode status` | 壳码注入状态 |

## 输出格式备注

- 表格:定宽列,纯 ASCII;地址 `0x` + 16 位十六进制大写
- 字节:2 位大写十六进制,空格分隔(如 `44 33 22 11`)
- 宽字符:不可打印替换为 `?`
- 成功命令输出 `OK`;错误输出 `Error: <原因>(<上下文>)` 到 stderr

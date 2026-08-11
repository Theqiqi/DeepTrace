# deeptrace-cli 使用参考

命令格式:`deeptrace_cli [选项] <命令组> <动作> [参数...]`

- 选项(命令前):`-p <pid>` 指定目标进程(大多数命令必需);`-h` 帮助;`-v` 版本。
- 退出码:`0` 成功 / `1` 执行失败 / `2` 用法错误。
- 地址:十六进制 `0x` 前缀(如 `0x14000D000`)。
- 状态持久化:`%TEMP%\deeptrace_<pid>\`(watch/注入记录跨命令保留;调试断点仅存在于脚本会话内)。

## 1. 进程(ps)

| 动作 | 示例 | 说明 |
|------|------|------|
| `list` | `deeptrace_cli ps list` | 进程列表(PID/名称/线程数/PPID) |
| `attach <pid>` | `deeptrace_cli ps attach 1234` | 保持目标进程 |
| `detach` | `deeptrace_cli ps detach` | 结束会话 |
| `info` | `deeptrace_cli -p 1234 ps info` | 进程信息 |
| `suspend` / `resume` | `deeptrace_cli -p 1234 ps suspend` | 暂停/恢复进程 |
| `kill [code]` | `deeptrace_cli -p 1234 ps kill 0` | 终止进程(慎用) |

## 2. 内存(mem)

| 动作 | 示例 | 说明 |
|------|------|------|
| `read <addr> [size] [fmt]` | `deeptrace_cli -p 1234 mem read 0x14000D000 4 hex` | 读内存;fmt: hex/dec/bin/ascii |
| `write <addr> <val> [fmt]` | `deeptrace_cli -p 1234 mem write 0x14000D000 CAFEBABE hex` | 写内存(仅可写区域) |
| `dump <addr> <size>` | `deeptrace_cli -p 1234 mem dump 0x14000D000 16` | hex+ASCII 转储 |
| `regions` | `deeptrace_cli -p 1234 mem regions` | 内存区域 |
| `readval <addr> <type>` | `deeptrace_cli -p 1234 mem readval 0x14000D000 dword` | 类型化读取(byte/word/dword/qword/float/double) |

## 3. 模块(module)

`list` / `find <name>` / `base <name>` / `exports <module>` / `dump <name> [file]`

```bash
deeptrace_cli -p 1234 module base deeptrace_target.exe   # → 0x0000000140000000
```

## 4. 线程(thread)

`list` / `suspend <tid>` / `resume <tid>` / `kill <tid>`

## 5. 调试(debug)——唯一入口 `run`

> v2.1.0 起 debug 组只有 `debug run <script.json>` 一个命令;其余单命令(step/break/registers 等)已移除,调用报 `unknown command`。

**一次调用 = 一次完整调试会话**(attach → debug_attach → 逐条步骤 → 清理 → detach),会话状态仅在内存中;断点/守护页在会话结束自动恢复,目标进程不被破坏。

### 脚本格式

```json
[
  {"op": "<操作>", "<字段>": "<值>", ...}
]
```

所有字段值为字符串;地址为 `0x` 十六进制字符串。

| op | 字段 | 说明 |
|----|------|------|
| `status` | — | 会话/断点状态 |
| `registers` | `tid`(可选,默认 0=首线程) | 全部寄存器 |
| `register` | `name`, `tid`(可选) | 单个寄存器(如 `"rip"`) |
| `break` | `addr` | 软件断点 |
| `clear` | `addr` | 清除软件断点 |
| `hbreak` | `addr`, `type`(0=执行/1=写/2=读写), `length`(1/2/4/8) | 硬件断点 |
| `hclear` | `addr` | 清除硬件断点 |
| `guard` / `unguard` | `addr`, `size` | 页守护(一次性) |
| `pause` / `resume` | — | 暂停/恢复目标 |
| `step` / `next` | `tid`(可选) | 单步 / 步过调用 |
| `continue` | `timeout_ms`(可选,默认 5000) | 运行到断点/异常/退出/超时 |
| `read` | `addr`, `size`, `format`(可选) | 读内存 |
| `write` | `addr`, `bytes`(空格分隔 hex,如 `"BE BA FE CA"`) | 写内存 |
| `disasm` | `addr`, `count`(可选) | 反汇编 |
| `watch_add` | `desc`, `addr`, `type` | 添加监视 |
| `watch_remove` / `watch_list` / `watch_refresh` / `watch_clear` | `index`(remove) | 监视管理 |

### 示例

```bash
# session.json
# [ {"op": "break", "addr": "0x14000D000"},
#   {"op": "continue", "timeout_ms": "10000"},
#   {"op": "registers"},
#   {"op": "clear", "addr": "0x14000D000"} ]
deeptrace_cli -p 1234 debug run session.json
```

输出:`[N] <op> <参数>` 步骤头 + 结果;`continue` 命中打印 `breakpoint hit at <addr> (rip = ...)`,超时打印 `continue timeout (N ms)`。退出码:`0` 全部成功 / `1` 步骤运行时失败 / `2` 脚本格式/校验错误。

## 6. 反汇编(disasm)

```bash
deeptrace_cli -p 1234 disasm at 0x14000D018 3
deeptrace_cli -p 1234 disasm range 0x14000D000 0x14000D100
```

## 7. 解析(resolve)

```bash
deeptrace_cli -p 1234 resolve base deeptrace_target.exe   # 模块基址
deeptrace_cli -p 1234 resolve scan "DE AD BE EF"          # AOB 扫描,?? 通配
```

## 8. 数据转换(convert)

把不同类型输入转成十六进制字节(配合 `resolve scan` 使用):

```bash
deeptrace_cli convert dword 0x11223344   # → 44 33 22 11(小端字节序)
deeptrace_cli convert string "Hi"        # → 48 69
# 类型: byte/word/dword/qword/float/double/string/hex
```

## 9. 监视(watch)

```bash
deeptrace_cli -p 1234 watch add counter 0x14000D000 dword
deeptrace_cli -p 1234 watch list / watch refresh / watch remove 0 / watch clear
```

## 10. DLL 注入(dll)

```bash
deeptrace_cli -p 1234 dll inject C:\\path\\to\\testdll.dll   # Windows 路径
deeptrace_cli -p 1234 dll eject C:\\path\\to\\testdll.dll
deeptrace_cli -p 1234 dll list / dll status
```

## 11. 汇编(asm)

```bash
deeptrace_cli asm assemble "nop; ret"            # → 90C3
deeptrace_cli asm assemble "nop" --c-array       # → unsigned char code[] = { 0x90 };
```

## 12. Shellcode(shellcode)

```bash
deeptrace_cli -p 1234 shellcode inject "9090C3"
deeptrace_cli -p 1234 shellcode injectat 0x14000D000 "9090C3"
deeptrace_cli -p 1234 shellcode status
```

## 13. 常见错误速查

| 报错 | 含义 |
|------|------|
| `Error: unknown command: 'step'` | 调用了已移除的 debug 单命令 → 用 `debug run` 脚本 |
| `Error: NotAttached` | 未指定/未附加目标进程 → 加 `-p <pid>` |
| `Error: AccessDenied` | 权限不足 → 管理员运行;64 位目标/反作弊属正常 |
| `Error: ReadFault` / `WriteFault` | 地址不可读 / 区域只读 → `mem regions` 查可读可写区域 |
| `Error: NoSuchProcess(pid)` | 进程不存在 → `ps list` 重新确认 PID |

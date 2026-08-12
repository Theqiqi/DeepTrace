# deeptrace-cli 使用技能(命令 / 用法)

命令格式:`deeptrace_cli [选项] <命令组> <动作> [参数...]`

- 选项(命令前):`-p <pid>` 指定目标进程(大多数命令必需);`-h` 帮助;`-v` 版本。
- 退出码:`0` 成功 / `1` 执行失败 / `2` 用法错误。
- 地址:十六进制 `0x` 前缀(如 `0x14000D000`),**或脚本符号名**(`script run` 注册后可当地址用,如 `mem read sunObjPtr`,v2.6.0)。
- 状态持久化:`%TEMP%\deeptrace_<pid>\`(watch/注入/脚本/hook 记录跨命令保留;调试断点仅存在于脚本会话内)。

## 1. 进程(ps)

| 动作 | 示例 | 说明 |
|------|------|------|
| `list` | `deeptrace_cli ps list` | 进程列表(PID/名称/线程数/PPID) |
| `attach <pid>` | `deeptrace_cli ps attach 1234` | 附加目标进程;**输出实际授予的权限列表**(v2.11.0) |
| `detach` | `deeptrace_cli ps detach` | 结束会话 |
| `info` | `deeptrace_cli -p 1234 ps info` | 进程信息 |
| `suspend` / `resume` | `deeptrace_cli -p 1234 ps suspend` | 暂停/恢复进程 |
| `kill [code]` | `deeptrace_cli -p 1234 ps kill 0` | 终止进程(慎用) |

## 2. 内存(mem)

| 动作 | 示例 | 说明 |
|------|------|------|
| `read <addr> [size] [fmt]` | `deeptrace_cli -p 1234 mem read 0x14000D000 4 hex` | 读内存;fmt: hex/dec/bin/ascii;addr 可用符号名 |
| `write <addr> <val> [fmt]` | `deeptrace_cli -p 1234 mem write 0x14000D000 CAFEBABE hex` | 写内存(仅可写区域);addr 可用符号名(动态改人造指针目标) |
| `dump <addr> <size>` | `deeptrace_cli -p 1234 mem dump 0x14000D000 16` | hex+ASCII 转储 |
| `regions` | `deeptrace_cli -p 1234 mem regions` | 内存区域 |
| `readval <addr> <type>` | `deeptrace_cli -p 1234 mem readval 0x14000D000 dword` | 类型化读取(byte/word/dword/qword/float/double) |
| `batch <read\|write> <file.json> [--format table\|csv\|json] [--out <file>]` | `deeptrace_cli -p 1234 mem batch read scan.json` | 按 JSON 定位器批量读/写;`--format csv\|json` 导出给其他工具/AI(v2.9-2.10) |

### batch 定位器 JSON(与 `resolve ptrscan` 输出同格式)

```json
{ "process": "game.exe", "locators": [
  { "name": "health", "type": "dword",
    "steps": [ { "kind": "module", "name": "game.exe", "offset": "+0x1AF89C0" },
               { "kind": "deref" },
               { "kind": "add", "offset": "+0x38" } ] }
] }
```

step kind:`module`(模块基址+偏移)/ `symbol`(脚本符号+偏移)/ `deref`(解引用)/ `add`(加偏移)/ 绝对地址。type: byte/word/dword/qword/float/double/string/bytes。CSV 列为 `name,address,value,status,error`(解析失败行 status=error)。

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
deeptrace_cli disasm file payload.bin        # 反汇编本地 .bin 文件,无需附加(v2.13.0)
```

## 7. 解析(resolve)

```bash
deeptrace_cli -p 1234 resolve base deeptrace_target.exe   # 模块基址
deeptrace_cli -p 1234 resolve scan "DE AD BE EF"          # AOB 扫描,?? 通配
deeptrace_cli -p 1234 resolve ptrscan scan.json           # 指针链扫描(v2.12.0)
```

### ptrscan 配置 JSON

```json
{ "version": 1, "target": "0x14000D008", "module": "deeptrace_target.exe",
  "max_offset": 2048, "max_level": 5, "max_results": 10000, "threads": 0 }
```

输出每行一条链(`deeptrace_target.exe+1AF89C0 +38 +104 +8`,有符号偏移)。重启后地址变动:重新定位 target 后,加 `"rescan": { "target": "0x..." }` 过滤假阳性。打印的链可直接放进 `mem batch` 定位器(搜索→验证闭环)。

## 8. 监视(watch)

```bash
deeptrace_cli -p 1234 watch add counter 0x14000D000 dword   # addr 可用符号名
deeptrace_cli -p 1234 watch list / watch refresh / watch remove 0 / watch clear
```

## 9. DLL 注入(dll)

```bash
deeptrace_cli -p 1234 dll inject C:\path\to\testdll.dll   # Windows 路径
deeptrace_cli -p 1234 dll eject C:\path\to\testdll.dll
deeptrace_cli -p 1234 dll list / dll status
```

## 10. 汇编(asm)

```bash
deeptrace_cli asm assemble "nop; ret"            # → 90C3
deeptrace_cli asm assemble "nop" --c-array       # → unsigned char code[] = { 0x90 };
deeptrace_cli asm file code.asm --hex            # 汇编 .asm 源文件(v2.2.0)
deeptrace_cli asm file code.asm --out code.bin   # 输出裸 .bin
```

## 11. Shellcode(shellcode)——完整生命周期

| 动作 | 说明 |
|------|------|
| `inject <hex>` | 分配并立即执行(自动分配) |
| `injectat <addr> <hex>` | 写入指定地址并执行 |
| `injectfile <path.bin>` | 读 .bin 文件并执行 |
| `alloc <source>` | **只分配+写入,不执行**,打印地址(source: hex 或 .bin;**hex 必须紧凑无空格**) |
| `run <address>` | 触发一次(可重复) |
| `free <address>` | 释放内存+清除记录(先等运行中的线程 5s,执行中拒绝释放) |
| `exec <source>` | 一步完成:转换→写入→触发(source: hex/.bin/.asm) |
| `status` | 注入状态 |

```bash
deeptrace_cli -p 1234 shellcode alloc "4831C0C3"   # → 打印分配地址
deeptrace_cli -p 1234 shellcode run 0x...          # → 触发一次(可重复)
deeptrace_cli -p 1234 shellcode free 0x...         # → OK
```

## 12. 数据转换(convert)

把不同类型输入转成十六进制字节(配合 `resolve scan` / `mem write` 使用):

```bash
deeptrace_cli convert dword 0x11223344   # → 44 33 22 11(小端字节序)
deeptrace_cli convert string "Hi"        # → 48 69
# 类型: byte/word/dword/qword/float/double/string/hex
```

## 13. 二进制文件转换(hex2bin / bin2hex)

```bash
deeptrace_cli hex2bin "9090C3" payload.bin   # hex → .bin(hex 必须紧凑无空格;非法报 invalid hex-bytes)
deeptrace_cli bin2hex payload.bin            # .bin → hex(90 90 C3);format: hex|dec|bin|ascii|c-array
```

**转换闭环(全部离线,无需附加)**: `asm file` → `hex2bin` → `bin2hex`/`disasm file` 检查 → `shellcode injectfile`/`alloc` 写入执行。

## 14. AA 脚本引擎(script)

> 运行 CE(Cheat Engine)风格 `.aa` 脚本:两个块 `[ENABLE]`(启用时做什么)与 `[DISABLE]`(如何撤销)。enable/disable 均**幂等**(重复执行第二次无操作),可安全重跑。

| 动作 | 示例 | 说明 |
|------|------|------|
| `check <file>` | `deeptrace_cli script check x.aa` | 只检查语法+汇编,不附加不执行;通过输出 `OK (5 steps: ...)`,错误 exit 2 |
| `run <file>` | `deeptrace_cli -p 1234 script run x.aa` | 执行 [ENABLE] 块(幂等) |
| `disable <file>` | `deeptrace_cli -p 1234 script disable x.aa` | 执行 [DISABLE] 块(幂等) |
| `status` | `deeptrace_cli -p 1234 script status` | 已启用脚本及其 alloc/hook 列表 |

### 关键字

| 关键字 | 语法 | 说明 |
|--------|------|------|
| `alloc` | `alloc(name,size[,anchor])` | 仅 [ENABLE];分配命名内存;第三参为就近锚点(`"module.dll"+off` 或绝对地址,±2GB 内避免 RIP 相对跳转超界,v2.7.0) |
| `registersymbol` | `registersymbol(name)` | 仅 [ENABLE];注册符号(人造指针,可被外部命令寻址) |
| `unregistersymbol` | `unregistersymbol(name)` | 仅 [DISABLE] |
| `label` | `label(name)` | 汇编标签声明 |
| `createThread` | `createThread(name)` | 在标签处创建远程线程 |
| `dealloc` | `dealloc(name)` | 仅 [DISABLE];释放 |
| `db` | `db <hex bytes>` | 写裸字节(如恢复被 hook 覆盖的原始代码) |
| `nop` | `nop [count]` | NOP 填充(1..16) |
| `<name>:` | `newmem:` | 定义标签 |
| `"module"+offset:` | `"GameAssembly.dll"+7D5778:` | hook 目标行;enable 时改写为 jmp 跳入你的代码,disable 时恢复 |
| 其他行 | `mov rax,1` / `jmp newmem` | 汇编指令(keystone),可引用标签/符号 |

### 示例

```
[ENABLE]
alloc(newmem,0x100)
registersymbol(newmem)
newmem:
mov rax,1
ret
[DISABLE]
dealloc(newmem)
```

```bash
deeptrace_cli -p 1234 script run x.aa
# alloc newmem = 0x00000000001F0000 (256 bytes)
# script enabled
```

符号可在任何命令中当地址用:`mem read newmem 4`、`mem write sunObjPtr ...`(改人造指针目标)、`watch add ptr sunObjPtr qword`。`mem read newmem` 等价于读人造指针。

## 15. 常见错误速查

| 报错 | 含义 |
|------|------|
| `Error: unknown command: 'step'` | 调用了已移除的 debug 单命令 → 用 `debug run` 脚本 |
| `Error: NotAttached` | 未指定/未附加目标进程 → 加 `-p <pid>` |
| `Error: AccessDenied` | 权限不足 → 管理员运行;64 位目标/反作弊属正常 |
| `Error: ReadFault` / `WriteFault` | 地址不可读 / 区域只读 → `mem regions` 查可读可写区域 |
| `Error: NoSuchProcess(pid)` | 进程不存在 → `ps list` 重新确认 PID |
| `invalid hex-bytes: '...'` | hex 带空格/非法 → 用紧凑 hex(如 `9090C3`) |
| exit 2(用法错误) | 参数/JSON 校验失败,执行前拒绝(如 `script check` 语法错、batch 定位器非法) |

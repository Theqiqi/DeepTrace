# deeptrace-cli — 调用提示词(Agent)

> 给 AI / AI agent 的**调用提示词**:当你(agent)需要用 deeptrace_cli 查看/修改进程内存时,
> 按本提示词执行命令。若工具尚未安装,先读[安装提示词](deeptrace-cli-install.md)。
> 索引与说明见仓库根目录 `README.md` 的「AI / Agent 使用说明」节。
> **注意**:下方围栏内的提示词文本会被复制到 agent 上下文,其中文档链接均为**仓库根相对路径**(如 `docs/users/v1.3/USER_MANUAL.md`)。

---

`````markdown
---
name: deeptrace-cli-usage
description: >
  Windows process memory tool deeptrace_cli - command reference for agents. Use when the
  user needs to inspect/modify process memory, enumerate processes, read/write memory,
  scan for AOB/byte patterns, set breakpoints, single-step debug, disassemble/assemble,
  watch variables, or inject DLLs/shellcode. Windows process memory 工具 deeptrace_cli
  命令参考。触发词/triggers: process memory, memory read/write, AOB/pattern scan,
  breakpoint, single-step, disassemble, inject DLL/shellcode, 进程内存、mem read/write、
  特征码扫描、断点、单步、反汇编、注入、deeptrace。
when_to_use: >
  User asks about process memory  operations (read/write/scan/watch), debugging
  (breakpoints/registers/single-step), disassembly/assembly, injection (DLL/shellcode),
  and deeptrace_cli is installed (install prompt first if not). 用户请求涉及进程内存
  操作、调试、反汇编/汇编、注入,且 deeptrace_cli 已安装时。
---

# deeptrace_cli 调用提示词

## 0. 关键事实(先读)

- 命令格式:`deeptrace_cli [选项] <命令组> <动作> [参数...]`
- 选项:`-p <pid>` 指定目标进程(大多数命令必需);`-h` 帮助;`-v` 版本
- 退出码:`0` 成功 / `1` 执行失败 / `2` 用法错误
- 地址:十六进制 `0x` 前缀(如 `0x14000D000`);64 位定宽显示 `0x%016llX`
- 状态持久化:断点/watch/注入记录存 `%TEMP%\deeptrace_<pid>\`,跨命令保留;调试会话本身不跨命令
- 命令组:`ps` `mem` `module` `thread` `debug` `disasm` `resolve` `watch` `dll` `asm` `shellcode`(共 53 个动作)
- 测试目标:`deeptrace_target.exe`(关闭 ASLR,固定地址 `0x14000D000` 存 `0x11223344`)——练习用

## 1. 快速示例

```bash
# 查看进程
deeptrace_cli ps list

# 读取某进程某地址 4 字节(hex)
deeptrace_cli -p 1234 mem read 0x14000D000 4 hex       # → 44 33 22 11

# 读类型化数值
deeptrace_cli -p 1234 mem readval 0x14000D000 dword    # → 0x11223344

# 写内存并读回确认
deeptrace_cli -p 1234 mem write 0x14000D000 CAFEBABE hex
deeptrace_cli -p 1234 mem read 0x14000D000 4 hex       # → CA FE BA BE

# AOB 特征码扫描(?? 通配)
deeptrace_cli -p 1234 resolve scan "DE AD BE EF"       # → 0x000000014000D018

# 汇编指令为字节
deeptrace_cli asm assemble "nop; ret"                  # → 90C3
deeptrace_cli asm assemble "nop" --c-array             # → unsigned char code[] = { 0x90 };
```

## 2. 命令组速查

| 组 | 典型动作 | 用途 |
|----|---------|------|
| `ps` | list / attach / detach / info / suspend / resume / kill | 进程管理 |
| `mem` | read / write / dump / regions / readval | 内存读写 |
| `module` | list / find / base / exports / dump | 模块与导出 |
| `thread` | list / suspend / resume / kill | 线程控制 |
| `debug` | attach / pause / resume / step / next / break / clear / hbreak / hclear / guard / unguard / status / registers / register | 调试与断点 |
| `disasm` | at / range | 反汇编 |
| `resolve` | base / scan | 基址与 AOB 扫描 |
| `watch` | list / add / remove / refresh / clear | 变量监视 |
| `dll` | inject / eject / list / status | DLL 注入 |
| `asm` | assemble (--hex / --c-array) | 汇编 |
| `shellcode` | inject / injectat / status | 壳码注入 |

> 每组命令的完整语法与输出格式见本仓库 [用户手册](docs/users/v1.3/USER_MANUAL.md) 与 [API 参考](docs/api/v1.3/README.md)。

## 3. 常见错误与处理

| 错误 | 含义 | 处理 |
|------|------|------|
| `NoSuchProcess(<pid>)` | 进程不存在/已退出 | `ps list` 重新找 pid |
| `NotAttached` | 未指定目标/无调试会话 | 加 `-p <pid>`;单独 `debug detach` 报此属正常 |
| `AccessDenied` | 权限不足/受保护进程 | 管理员身份运行;换普通进程 |
| `ReadFault` / `WriteFault` | 地址不可读/写 | `mem regions` 找可读区域 |
| `BadFormat` | 汇编/特征码格式错 | 指令加引号;字节间空格 |
| `invalid address` | 地址格式错(退出码 2) | 用 `0x` 前缀十六进制 |
| 命令不存在 | 命令组/动作拼写错 | 先 `deeptrace_cli -h` 查命令列表 |

## 4. 工作流建议

1. 先 `ps list` 确认目标进程存在,拿到 pid
2. 用 `-p <pid>` 执行操作;不确定地址先用 `mem regions` 探布局
3. 调试场景:`debug attach` → `debug break <addr>` → `debug status` → `debug clear <addr>`
4. 修改内存前先 `mem read` 保存原值,便于恢复
5. 先在测试目标 `deeptrace_target.exe` 上练习,再操作真实程序
`````

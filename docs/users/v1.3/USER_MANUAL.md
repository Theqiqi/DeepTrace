# 用户手册(USER_MANUAL)

> 目标读者:有基础的用户(已读过[快速开始](GETTING_STARTED.md))。
> 本文按命令组介绍每个功能的用法。每个功能包含:什么时候用、操作步骤、预期输出、注意事项。
> 输出样本均来自真实运行 `deeptrace_cli.exe`(针对测试目标 `deeptrace_target.exe`,地址固定)。

## 0. 通用说明

### 命令格式

```
deeptrace_cli [选项] <命令组> <动作> [参数...]
```

- **选项**(放在命令前):`-p <进程号>` 指定目标进程;`-h` 帮助;`-v` 版本。
- **命令组 + 动作**:如 `ps list`(进程-列表)、`mem read`(内存-读取)。
- **地址写法**:十六进制,以 `0x` 开头,如 `0x14000D000`。
- **提示**:任何命令都能用 `deeptrace_cli -h` 查看完整命令列表。

### 退出码

| 退出码 | 含义 |
|--------|------|
| 0 | 成功 |
| 1 | 执行失败(如进程不存在、读内存失败) |
| 2 | 用法错误(命令写错、参数不对) |

### 关于「目标进程」

大多数命令需要先指定目标进程:`-p <进程号>`。进程号(PID)用 `ps list` 查看。如果不带 `-p`,默认操作**当前附加的进程**(通常你之前用 `-p` 指定过的进程,或先 `ps attach` 附加)。

> 每次运行 `deeptrace_cli` 都是独立的一次操作;断点、监视(watch)、注入记录会**跨命令保留**(存在临时目录,见 [FAQ](FAQ.md#4-断点watch为-什么跨命令还在))。

---

## 1. 进程(ps)

### 1.1 查看进程列表 — `ps list`

- **什么时候用**:想知道系统里有哪些程序在运行,或查找目标程序的进程号。
- **操作**:
  ```
  deeptrace_cli ps list
  ```
- **预期输出**(真实样本,节选):
  ```
  PID        NAME                                     THREADS  PPID
  0          [System Process]                         24       0
  4          System                                   361      0
  ```
  每行一个进程:进程号 / 名称 / 线程数 / 父进程号。
- **注意**:列表很长,可在命令提示符窗口向上滚动,或右键窗口→「标记」后复制。

### 1.2 附加到进程 — `ps attach <进程号>`

- **什么时候用**:想持续以某进程为目标(之后不带 `-p` 也能操作它)。
- **操作**:
  ```
  deeptrace_cli ps attach 1234
  ```
- **预期输出**:
  ```
  OK
  ```
- **注意**:进程号不存在时提示 `Error: NoSuchProcess(<进程号>)`。

### 1.3 分离 — `ps detach`

- **什么时候用**:结束与当前进程的会话。
- **操作**:`deeptrace_cli ps detach`
- **预期输出**:`OK`

### 1.4 查看进程信息 — `ps info`

- **什么时候用**:确认目标进程、看它的详细信息。
- **操作**:`deeptrace_cli -p 1234 ps info`
- **预期输出**(真实样本):
  ```
  PID: 26128
  Name: deeptrace_target.exe
  Threads: 5
  ParentPID: 3592
  ```

### 1.5 挂起 / 恢复 / 结束进程

| 动作 | 命令 | 效果 |
|------|------|------|
| 挂起 | `deeptrace_cli -p 1234 ps suspend` | 暂停进程所有线程(冻结) |
| 恢复 | `deeptrace_cli -p 1234 ps resume` | 让挂起的进程继续 |
| 结束 | `deeptrace_cli -p 1234 ps kill` | 结束进程(可加退出码 `ps kill 0`) |

- **预期输出**:均为 `OK`。
- **注意**:`ps kill` 会**直接终止**目标程序,请谨慎使用。

---

## 2. 内存(mem)

### 2.1 读取内存 — `mem read <地址> [大小] [格式]`

- **什么时候用**:查看目标进程某个地址的内容。格式可选 `hex`(十六进制,默认)/`dec`(十进制)/`bin`(二进制)/`ascii`(字符)。
- **操作**(读地址 `0x14000D000` 开始的 4 个字节):
  ```
  deeptrace_cli -p 1234 mem read 0x14000D000 4 hex
  ```
- **预期输出**(真实样本):
  ```
  44 33 22 11
  ```
  这是 4 个字节的十六进制(每个字节 2 位,空格分隔)。
- **注意**:地址不可读时提示 `Error: ReadFault`;没有权限提示 `Error: AccessDenied`(见[故障排除](TROUBLESHOOTING.md))。

### 2.2 写入内存 — `mem write <地址> <值> [格式]`

- **什么时候用**:修改目标进程内存。值默认十六进制(如 `CAFEBABE` 是 4 字节)。
- **操作**(写入后再读回确认):
  ```
  deeptrace_cli -p 1234 mem write 0x14000D000 CAFEBABE hex
  deeptrace_cli -p 1234 mem read 0x14000D000 4 hex
  ```
- **预期输出**(真实样本):
  ```
  OK
  CA FE BA BE
  ```
- **注意**:写内存可能让目标程序崩溃或行为改变,先在测试程序上练习。

### 2.3 十六进制转储 — `mem dump <地址> <大小>`

- **什么时候用**:想同时看十六进制字节和对应字符(像十六进制编辑器)。
- **操作**:`deeptrace_cli -p 1234 mem dump 0x14000D000 16`
- **预期输出**(真实样本):
  ```
  0x000000014000D000  44 33 22 11 D0 0F 49 40 88 77 66 55 44 33 22 11  |D3"..I@.wfUD3".|
  ```
  左侧是地址,中间是字节,右侧 `|...|` 是字节对应的可打印字符(不可打印显示为 `.`)。

### 2.4 列出内存区域 — `mem regions`

- **什么时候用**:查看目标进程内存布局(哪些地址范围可读可写),找可用的内存区域。
- **操作**:`deeptrace_cli -p 1234 mem regions`
- **预期输出**(真实样本,节选):
  ```
  BASE               SIZE           PROTECTION STATE
  0x0000000000000000 65536          0x00000001    65536
  0x0000000140000000 4096           0x00000002    4096
  ...
  ```
- **注意**:列含义:起始地址 / 大小 / 保护属性 / 状态。数值是 Windows 原始值,普通用户看地址范围即可。

### 2.5 读取类型化数值 — `mem readval <地址> <类型>`

- **什么时候用**:不想要原始字节,直接读成数值。类型:`byte`(1 字节)/`word`(2)/`dword`(4)/`qword`(8)/`float`(小数)/`double`(双精度小数)。
- **操作**:`deeptrace_cli -p 1234 mem readval 0x14000D000 dword`
- **预期输出**(真实样本):
  ```
  0x11223344
  ```

---

## 3. 模块(module)

### 3.1 列出模块 — `module list`

- **什么时候用**:查看目标进程加载了哪些程序文件(主程序 + DLL)。
- **操作**:`deeptrace_cli -p 1234 module list`
- **预期输出**(真实样本,节选):
  ```
  BASE               SIZE         NAME
  0x0000000140000000 73728        deeptrace_target.exe
  0x00007FFC98D00000 2514944      ntdll.dll
  ...
  ```

### 3.2 查找/获取基址 — `module find <名称>` / `module base <名称>`

- **什么时候用**:找某个模块的加载地址(基址)。
- **操作**:`deeptrace_cli -p 1234 module base deeptrace_target.exe`
- **预期输出**(真实样本):
  ```
  0x0000000140000000
  ```

### 3.3 列出导出函数 — `module exports <模块>`

- **什么时候用**:查看 DLL 导出了哪些函数(常用于找目标函数地址)。
- **操作**:`deeptrace_cli -p 1234 module exports kernel32.dll`
- **预期输出**:函数名与地址两列表格(名称 / 地址)。

### 3.4 转储模块 — `module dump <名称> [输出文件]`

- **什么时候用**:把模块内容导出为十六进制文本,或保存到文件。
- **操作**:`deeptrace_cli -p 1234 module dump deeptrace_target.exe dump.txt`
- **预期输出**:`OK`(文件生成)或不带文件名时在屏幕输出十六进制。

---

## 4. 线程(thread)

### 4.1 列出线程 — `thread list`

- **什么时候用**:查看目标进程的线程。
- **操作**:`deeptrace_cli -p 1234 thread list`
- **预期输出**(真实样本):
  ```
  TID        PRIORITY   START
  8124       8          0x0000000000000000
  ...
  ```

### 4.2 挂起 / 恢复 / 结束线程

| 动作 | 命令 |
|------|------|
| 挂起 | `deeptrace_cli -p 1234 thread suspend <线程号>` |
| 恢复 | `deeptrace_cli -p 1234 thread resume <线程号>` |
| 结束 | `deeptrace_cli -p 1234 thread kill <线程号>` |

- **预期输出**:均为 `OK`。
- **注意**:`thread kill` 会结束该线程,可能导致程序异常。

---

## 5. 调试(debug)

> 调试功能让目标进程进入「被调试」状态,可以暂停、单步、设断点、看寄存器。

### 5.1 进入调试 — `debug attach`

- **操作**:
  ```
  deeptrace_cli -p 1234 debug attach
  ```
- **预期输出**:`OK`。
- **注意**:`debug attach` 不会终止目标进程(已验证);命令结束后调试自动退出,目标进程继续正常运行。对受保护进程可能提示 `Error: AccessDenied`。
- **说明**:每次运行命令都是一次独立操作,调试会话不跨命令保留——所以单独运行 `debug detach` 会提示 `Error: NotAttached`(当前没有调试会话,属正常)。

### 5.2 暂停 / 恢复 — `debug pause` / `debug resume`

- **什么时候用**:让进程停住(方便改内存/看状态)或继续运行。
- **操作**:`deeptrace_cli -p 1234 debug pause` → 进程暂停;`debug resume` → 继续。
- **预期输出**:`OK`。
- **注意**:`debug pause`/`debug resume` 直接用 `-p` 指定进程即可,不要求先 `debug attach`(会自动处理暂停)。

### 5.3 单步 — `debug step [线程号]` / `debug next [线程号]`

- **什么时候用**:一行一行执行代码,观察每条指令效果。`step` 会进入函数内部;`next` 跳过函数调用。
- **操作**:`deeptrace_cli -p 1234 debug step`
- **预期输出**:`OK`(可结合 `debug register rip` 看当前执行位置变化)。

### 5.4 软件断点 — `debug break <地址>` / `debug clear <地址>`

- **什么时候用**:让程序执行到某地址时暂停。
- **操作**:
  ```
  deeptrace_cli -p 1234 debug break 0x14000D000
  ```
- **预期输出**(真实样本):
  ```
  breakpoint set at 0x000000014000D000 (orig 0x44)
  ```
  `orig` 是该地址被断点替换前的原始字节(清除断点时自动还原)。
- **清除**:`deeptrace_cli -p 1234 debug clear 0x14000D000` → `OK`。

### 5.5 硬件断点 — `debug hbreak <地址> [类型] [长度]`

- **什么时候用**:不修改内存的断点(类型 `0`=执行,`1`=写入,`2`=读写)。数量有限(通常 4 个)。
- **操作**:`deeptrace_cli -p 1234 debug hbreak 0x14000D000 0 1`
- **清除**:`deeptrace_cli -p 1234 debug hclear <地址>`。

### 5.6 页守卫断点 — `debug guard <地址> <大小>` / `debug unguard <地址> <大小>`

- **什么时候用**:监视一块内存区域的访问。
- **操作**:`deeptrace_cli -p 1234 debug guard 0x14000D000 16`

### 5.7 调试状态 — `debug status`

- **什么时候用**:确认是否在调试、断点数量。
- **操作**:`deeptrace_cli -p 1234 debug status`
- **预期输出**(真实样本):
  ```
  attached: yes
  pid: 26128
  breakpoints: 1
  hw_breakpoints: 0
  ```

### 5.8 寄存器 — `debug registers [线程号]` / `debug register <名称> [线程号]`

- **什么时候用**:查看 CPU 寄存器(调试的关键信息)。
- **操作**:`deeptrace_cli -p 1234 debug registers`
- **预期输出**(真实样本,节选):
  ```
  REG      VALUE
  rax      0x0000000000000034
  ...
  rip      0x00007FFC98E606E4
  eflags   0x0000000000000246
  ```
- **只看一个**:`deeptrace_cli -p 1234 debug register rip` → `rip = 0x00007FFC98E606E4`。

---

## 6. 反汇编(disasm)

### 6.1 反汇编 — `disasm at <地址> [条数]` / `disasm range <起始> <结束>`

- **什么时候用**:把一段内存(机器码)翻译成汇编指令,看懂代码在做什么。
- **操作**:
  ```
  deeptrace_cli -p 1234 disasm at 0x14000D018 3
  ```
- **预期输出**(真实样本):
  ```
  ADDRESS            BYTES                INSTRUCTION
  0x000000014000D018 DE AD BE EF 48 8B    fisubr word ptr [rbp - 0x74b71042]
  0x000000014000D01E 45 08 90 90 90 90 CC or byte ptr [r8 - 0x336f6f70], r10b
  0x000000014000D025 C3                   ret
  ```
  每行:地址 / 原始机器码 / 汇编指令。
- **范围反汇编**:`deeptrace_cli -p 1234 disasm range 0x14000D000 0x14000D100`(从起址到结束地址)。

---

## 7. 解析(resolve)

### 7.1 解析模块基址 — `resolve base <模块名>`

- **什么时候用**:同 `module base`,取模块基址。
- **操作**:`deeptrace_cli -p 1234 resolve base deeptrace_target.exe`
- **预期输出**:
  ```
  0x0000000140000000
  ```

### 7.2 特征码扫描(AOB)— `resolve scan <特征码>`

- **什么时候用**:不知道地址,但知道一段特征字节(如某个固定值 `DE AD BE EF`),在整个进程内存中找它出现的位置。`??` 表示任意字节。
- **操作**:
  ```
  deeptrace_cli -p 1234 resolve scan "DE AD BE EF"
  ```
- **预期输出**(真实样本):
  ```
  0x000000014000D018
  ```
- **注意**:特征码要加引号,字节间用空格;`??` 通配(如 `"48 8B ?? ?? 00"`)。匹配到的地址可能不止一个。

---

## 8. 监视(watch)

### 8.1 添加监视 — `watch add <描述> <地址> <类型>`

- **什么时候用**:想持续观察某个地址的值变化(类型同 `mem readval`:byte/word/dword/qword/float/double)。
- **操作**:
  ```
  deeptrace_cli -p 1234 watch add counter 0x14000D000 dword
  ```
- **预期输出**:`OK`。

### 8.2 查看/刷新 — `watch list` / `watch refresh`

- **操作**:`deeptrace_cli -p 1234 watch refresh`
- **预期输出**(真实样本):
  ```
  IDX    DESCRIPTION              ADDRESS            TYPE     VALUE                VALID
  0      counter                  0x000000014000D000 dword    0x11223344           yes
  ```
  `VALID: yes` 表示成功读到值;`no` 表示暂时读不到(如地址不可读)。
- **`watch list`** 输出与 refresh 相同(列表本身也会读实时值)。

### 8.3 删除/清空 — `watch remove <序号>` / `watch clear`

- **操作**:`deeptrace_cli -p 1234 watch remove 0`(删第 0 条)→ `OK`;`watch clear` 清空全部 → `OK`。
- **注意**:序号是表格第一列 `IDX`。

---

## 9. DLL 注入(dll)

### 9.1 注入 DLL — `dll inject <dll路径>`

- **什么时候用**:让目标进程加载一个 DLL(如游戏外挂/插件)。
- **操作**:
  ```
  deeptrace_cli -p 1234 dll inject C:\path\to\testdll.dll
  ```
- **预期输出**:`OK` 或注入信息(路径/地址/线程号)。
- **注意**:路径用 Windows 格式(`C:\...` 或 `C:/...`)。64 位进程必须注入 64 位 DLL。

### 9.2 卸载 — `dll eject <路径或地址>`

- **操作**:`deeptrace_cli -p 1234 dll eject C:\path\to\testdll.dll` → `OK`。

### 9.3 查看 — `dll list` / `dll status`

- **操作**:`deeptrace_cli -p 1234 dll list`
- **预期输出**(真实样本,空状态):
  ```
  KIND     PATH                                     ADDRESS            TID        RUNNING
  ```
  有注入记录时每行一条:`类型 / 路径 / 远端地址 / 线程号 / 是否运行中`。

---

## 10. 汇编(asm)

### 10.1 汇编 — `asm assemble <代码> [--hex] [--c-array]`

- **什么时候用**:把汇编指令翻译成机器码(写壳码/补丁时的必备功能)。多条指令用 `;` 分隔。
- **操作**:
  ```
  deeptrace_cli asm assemble "nop; ret"
  ```
- **预期输出**(真实样本):
  ```
  90C3
  ```
  (`90`=nop,`C3`=ret)
- **--hex 输出**:默认就是十六进制;`--hex` 显式指定。
- **--c-array 输出**(真实样本):
  ```
  deeptrace_cli asm assemble "nop" --c-array
  unsigned char code[] = { 0x90 };
  ```
  直接生成 C 语言字节数组,方便粘贴到代码里。
- **注意**:汇编代码要加引号(`"..."`)。不支持的指令提示 `Error: BadFormat`。

---

## 11. 壳码(shellcode)

### 11.1 注入壳码 — `shellcode inject <十六进制字节>`

- **什么时候用**:把一段机器码(如 `90 90 C3`)注入目标进程并执行,工具会自动分配内存。
- **操作**:
  ```
  deeptrace_cli -p 1234 shellcode inject "9090C3"
  ```
- **预期输出**:注入信息(地址/线程号)。
- **注意**:壳码有风险,会让目标进程崩溃;先在测试程序上验证。

### 11.2 指定地址注入 — `shellcode injectat <地址> <字节>`

- **操作**:`deeptrace_cli -p 1234 shellcode injectat 0x14000D000 "9090C3"`

### 11.3 查看状态 — `shellcode status`

- **操作**:`deeptrace_cli -p 1234 shellcode status` → 同 `dll list` 格式的表格(`KIND` 为 `shellcode`)。

---

## 12. 命令速查表

| 命令 | 作用 |
|------|------|
| `ps list` | 查看进程 |
| `ps attach <pid>` / `ps detach` | 附加/分离进程 |
| `ps info` / `ps suspend` / `ps resume` / `ps kill` | 进程信息/挂起/恢复/结束 |
| `mem read <addr> [size] [fmt]` | 读内存 |
| `mem write <addr> <val> [fmt]` | 写内存 |
| `mem dump <addr> <size>` | 十六进制转储 |
| `mem regions` | 内存区域 |
| `mem readval <addr> <type>` | 读类型化数值 |
| `module list` / `find` / `base` / `exports` / `dump` | 模块操作 |
| `thread list` / `suspend` / `resume` / `kill` | 线程操作 |
| `debug attach` / `detach` / `pause` / `resume` | 调试控制 |
| `debug step` / `next` | 单步 |
| `debug break` / `clear` / `hbreak` / `hclear` / `guard` / `unguard` | 断点 |
| `debug status` / `registers` / `register <name>` | 调试状态/寄存器 |
| `disasm at <addr> [n]` / `disasm range <a> <b>` | 反汇编 |
| `resolve base <mod>` / `resolve scan <pattern>` | 基址/特征码扫描 |
| `watch add/list/remove/refresh/clear` | 监视 |
| `dll inject/eject/list/status` | DLL 注入 |
| `asm assemble <code> [--hex] [--c-array]` | 汇编 |
| `shellcode inject/injectat/status` | 壳码注入 |

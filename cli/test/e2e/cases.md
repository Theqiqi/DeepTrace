# E2E 测试用例清单(deeptrace_cli)

> 独立 Python 体系(`test_cli_e2e.py`),不参与 CMake。
> 覆盖无参运行、-h/--help、错误路径、-p <pid> 真实目标进程操作、清理后退出。
> 修改模式:v2.2.0 新增 asm file / hex2bin / shellcode alloc|run|free|exec|injectfile
> (汇编代码注入并执行,分阶段操作);v2.3.0 新增 script 组(AA 风格脚本引擎
> run/disable/status,幂等);v2.4.0 新增 `script check`(只检查不执行);
> v2.5.0 新增脚本符号引用(人造指针:mov [sym],reg / mov reg,sym,任意指令);
> v2.6.0 新增符号寻址(地址参数接受脚本符号名,如 `mem read sunObjPtr`,
> 配合人造指针外部读值);v2.7.0 新增 alloc near 真实就近分配(第三参数
> 解析为锚点,落点保证在锚点 ±2GB 内,消除 RIP 相对位移超界概率);
> v2.8.0 确认并补测 `mem write <symbol>`(脚本外直接写人造指针值,动态改
> 指针目标;能力在 v2.6.0 已由 resolve_addr 接通,本版本补测试覆盖);
> v2.9.0 新增批量定位器 JSON(mem batch read/write,指针链/模块+偏移/
> 符号+偏移/绝对地址,8 种类型,文件即存储);v2.10.0 新增 batch 导出
> (--format csv|json + --out,供其他工具/AI 消费,含 status/error 字段);
> v2.11.0 ps attach 输出实际权限摘要(语义化名列表,read|write|...);
> 版本号同步 v2.11.0;既有用例全量回归。

## 1. 全局与基础

| 用例 | 前置 | 操作 | 预期输出 | 退出码 |
|------|------|------|----------|--------|
| 无参运行 | - | `deeptrace_cli` | stderr 含 Missing command | 1 |
| -h | - | `deeptrace_cli -h` | stdout 含 mem read / convert / debug run | 0 |
| --help | - | `deeptrace_cli --help` | 同 -h | 0 |
| -v | - | `deeptrace_cli -v` | stdout 含 deeptrace_cli v2.11.0 | 0 |
| 未知命令组 | - | `deeptrace_cli bogus cmd` | stderr 含 unknown command group | 2 |
| attach 不存在的进程 | - | `deeptrace_cli ps attach 99999999` | Error | 1 |
| attach 权限透出(v2.11.0) | 目标已启动 | `deeptrace_cli ps attach <pid>` | OK (permissions: read\|write\|...) 含 read/write | 0 |
| 非法参数(v2.6.0:符号形状现在合法,真非法形状仍拒绝) | - | `deeptrace_cli mem read "foo bar"` / `mem read a-b` | Error | 2 |

## 2. convert(新增,v1.4.1,无需目标进程)

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| byte | `convert byte 255` | `FF` | 0 |
| word 小端 | `convert word 0x0102` | `02 01` | 0 |
| dword 小端 | `convert dword 100` | `64 00 00 00` | 0 |
| qword 小端 | `convert qword 0x1122334455667788` | `88 77 66 55 44 33 22 11` | 0 |
| float IEEE754 | `convert float 1.0` | `00 00 80 3F` | 0 |
| double IEEE754 | `convert double 1.0` | `00 00 00 00 00 00 F0 3F` | 0 |
| string ASCII | `convert string hi` | `68 69` | 0 |
| hex 透传 | `convert hex DEADBEEF` | `DE AD BE EF` | 0 |
| 非法 type | `convert bogus 1` | stderr 含 invalid type | 2 |
| 非法 value | `convert dword xyz` | stderr 含 invalid value for type 'dword' | 2 |
| 越界 | `convert byte 256` | Error | 2 |
| 缺 value | `convert dword` | stderr 含 missing argument: value | 2 |

## 3. debug run(新增,v2.0.0,一次调用 = 一次调试会话)

前置:启动 deeptrace_target.exe,解析 PID / g_int 地址。
脚本样例为仓库真实文件 `cli/test/scripts/*.json`,测试读取后替换
`%G_INT%` 占位符为运行时地址,写入临时副本执行。

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| 脚本化会话(状态/寄存器/读/断点/监视) | 用 `debug_session.json`(status/registers/read/break/clear/watch_add/watch_list/watch_clear),`-p <pid> debug run` | 各步 `[N] op ...` 头 + 结果;watch_list 显示 0x11223344 | 0 |
| 副作用:会话后目标存活 | 上例后 `ps list` | 目标 pid 仍在 | 0 |
| write 空格分隔 hex | 用 `debug_write.json`(bytes "BE BA FE CA")后 `mem read` | 读出 BE BA FE CA | 0 |
| 脚本文件不存在 | `debug run no_such.json` | stderr 含 cannot open script file | 2 |
| 脚本 op 未知 | 用 `debug_bad.json`(op frobnicate) | stderr 含 unknown op | 2 |

## 3.1 debug 单命令已删除(v2.1.0,负例)

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| debug 单命令全部拒绝 | `-p <pid> debug step` / `break` / `registers` / `attach` / `status` 等 15 个动作 | stderr 含 unknown command | 2 |
| 拒绝后目标无损 | 上例后 `mem read g_int` | 仍为 44(无 0xCC 残留) | 0 |

## 4. resolve scan(恢复 pattern-only)

前置:启动 deeptrace_target.exe,解析 PID / g_int / g_bytes 地址。

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| AOB 模式扫描(既有回归) | `resolve scan "DE AD BE EF"` | 命中地址含 g_bytes | 0 |
| convert 输出喂给 scan(链式) | `convert dword 287454020` → 取输出 `44 33 22 11` → `resolve scan "44 33 22 11"` | 命中地址含 g_int | 0 |
| 旧类型语法已移除(回归) | `resolve scan 100 dword` | stderr 含 too many arguments | 2 |

## 4.5 asm file / hex2bin / shellcode 分阶段(v2.2.0,新增)

前置:启动 deeptrace_target.exe,解析 PID。临时 .asm/.bin 文件写入 BIN_DIR
(Windows 可读路径),测试后清理。

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| asm file 汇编+写 bin | 写 `xor eax,eax\nret` 到 .asm,`asm file <path> --out <bin>` | 输出 31C0C3;.bin 文件 3 字节 31 C0 C3 | 0 |
| asm file 文件不存在 | `asm file no_such.asm` | stderr 含 cannot read file | 2 |
| hex2bin 写文件 | `hex2bin DEADBEEF out.bin` | stdout 含 wrote;文件 4 字节 | 0 |
| hex2bin 非法 hex | `hex2bin ABC out.bin` | Error | 2 |
| alloc 只写入不执行 | `-p <pid> shellcode alloc C3` | 输出地址(running 列 no) | 0 |
| alloc 非法 source | `-p <pid> shellcode alloc zzzz` | stderr 含 invalid shellcode source | 2 |
| run 触发一次 | `-p <pid> shellcode run <addr>` | 输出地址/TID | 0 |
| run 可重复 | 再次 `run <addr>` | 0 |
| free 释放 | `-p <pid> shellcode free <addr>` | OK | 0 |
| free 后 run NotFound | `run <addr>` | Error 含 NotFound | 1 |
| 副作用:分阶段后目标存活 | 上例后 `ps list` | 目标 pid 仍在 | 0 |
| exec 流水线(.bin) | hex2bin C3 → `exec <bin>` | 输出地址/TID(一次调用完整流程) | 0 |
| exec 流水线(.asm 内存直转) | 写 `ret` 到 .asm → `exec <asm>` | 输出地址/TID | 0 |
| exec 非法 source | `exec zzzz` | stderr 含 invalid shellcode source | 2 |
| injectfile 文件注入 | hex2bin C3 → `injectfile <bin>` | 输出地址/TID(立即执行) | 0 |
| injectfile 文件不存在 | `injectfile no_such.bin` | stderr 含 cannot read file | 2 |

> exec/injectfile 产生的记录测试后 free 清理;全部 shellcode 用例结束断言目标存活。

## 4.6 script 组:AA 风格脚本引擎(v2.3.0,新增)

前置:启动 deeptrace_target.exe,解析 PID。脚本样例为仓库真实文件
`cli/test/scripts/script_call.aa`(call 型 alloc+createThread+ret)、
`script_hook.aa`(hook 型,"模块"+偏移改写,`%HOOK_OFF%` 运行时替换)、
`script_bad.aa`(语法错误负例)、`script_badasm.aa`(汇编失败回滚负例);
测试读取后写入临时副本(BIN_DIR,Windows 可读路径)执行,测试后清理。

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| run 执行 ENABLE(call 型) | 用 `script_call.aa` → `-p <pid> script run <aa>` | stdout 含 alloc newmem / createThread | 0 |
| run 幂等(重复) | 再次 `script run <aa>` | stdout 含 already enabled | 0 |
| status 列出启用脚本 | `-p <pid> script status` | stdout 含脚本文件名 + enabled | 0 |
| disable 执行 DISABLE | `-p <pid> script disable <aa>` | stdout 含 dealloc newmem + OK | 0 |
| disable 幂等(重复) | 再次 `script disable <aa>` | stdout 含 already disabled | 0 |
| run 脚本语法错误 | 用 `script_bad.aa`(`[FOO]`)→ `script run` | stderr 含 unknown block | 2 |
| 副作用:脚本往返后目标存活 | 上例后 `ps list` | 目标 pid 仍在 | 0 |

> 集成测试另覆盖:`script_hook.aa` hook 改写/恢复字节往返、`script_badasm.aa`
> 中途失败回滚(无残留记录/符号)、无 -p 时 NotAttached 退出 1。

## 4.7 script check:只检查不执行(v2.4.0,新增)

前置:无需目标进程(纯本地校验,不 attach、无副作用)。脚本样例为仓库真实
文件 `cli/test/scripts/*.aa`,测试读取后写入临时副本(BIN_DIR)执行。

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| 有效脚本通过 | `script check <script_call.aa>` | stdout 含 `OK (` 与 steps 统计 | 0 |
| hook 脚本(偏移替换后)通过 | `script check <script_hook.aa 替换 %HOOK_OFF%=0x1000>` | `OK (...)` | 0 |
| 语法错误 | `script check <script_bad.aa>` | stderr 含 script parse error | 2 |
| 汇编预检失败 | `script check <script_badasm.aa>` | stderr 含 BadFormat | 2 |
| 文件不存在 | `script check no_such.aa` | stderr 含 cannot read file | 2 |
| hook 后无 jmp | `script check <内联脚本>` | stderr 含 hook target must be followed | 2 |
| jmp 未定义 label | `script check <内联脚本>` | stderr 含 undefined label | 2 |
| hook 块内第二条指令 | `script check <内联脚本>` | stderr 含 only 'jmp <label>' | 2 |
| 副作用:check 后目标存活 | 上例后 `ps list` | 目标 pid 仍在 | 0 |

> check 无退出 1:不 attach、不执行,失败均为输入/脚本问题 → 退出 2。
> 能力边界:不校验模块加载(需 attach,由 run 校验);不做 alloc/write/线程。

## 4.8 人造指针:脚本符号引用(v2.5.0,新增)

前置:启动 deeptrace_target.exe,解析 PID。脚本样例为仓库真实文件
`cli/test/scripts/script_aptr.aa`(alloc 双槽位 + createThread;线程写入
slotA=moffs64、slotB=RIP 相对两种编码)。

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| aptr 脚本 check 通过 | `script check <script_aptr.aa>` | stdout 含 `OK (` | 0 |
| aptr run 执行 | `-p <pid> script run <script_aptr.aa>` | stdout 含 createThread | 0 |
| status 列出双槽位 | `-p <pid> script status` | alloc slotA / alloc slotB 带地址 | 0 |
| slotA(moffs64)读回 | `mem read <slotA> 8 hex` | 88 77 66 55 44 33 22 11 | 0 |
| slotB(RIP 相对)读回 | `mem read <slotB> 8 hex` | 00 FF EE DD CC BB AA 99 | 0 |
| aptr disable 释放 | `-p <pid> script disable <aa>` | stdout 含 dealloc | 0 |
| 副作用:aptr 往返后目标存活 | 上例后 `ps list` | 目标 pid 仍在 | 0 |

> 集成测试另覆盖:真实进程上双编码写入值读回验证、owner 字段保留(status 归属
> 脚本而非 (unowned))。

## 4.9 符号寻址:地址参数接受脚本符号(v2.6.0,新增)

前置:启动 deeptrace_target.exe,解析 PID。`script run` 执行 script_aptr.aa
后,地址参数直接写符号名(slotA/slotB),CLI 解析为记录地址。

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| 按符号 mem read | `-p <pid> mem read slotA 8 hex` | 88 77 66 55 44 33 22 11 | 0 |
| 按符号 mem read(slotB) | `-p <pid> mem read slotB 8 hex` | 00 FF EE DD CC BB AA 99 | 0 |
| 按符号 readval | `-p <pid> mem readval slotA qword` | 0x1122334455667788 | 0 |
| 按符号 mem write(hex) | `-p <pid> mem write slotA 8877665544332211 hex` → `mem read slotA 8 hex` | 88 77 66 55 44 33 22 11(新指针值读回) | 0 |
| 按符号 mem write(dec) | `-p <pid> mem write slotA 1122334455667788 dec` → `mem read slotA` | 4C 9C 8C DA C1 FC 03 00(8 字节小端) | 0 |
| 写后恢复原指针值 | `mem write slotA 8877665544332211 hex` | 0(watch 用例继续看到原值) | 0 |
| mem write 未知符号 | `-p <pid> mem write nosuch 1122334455667788 dec` | Error 含 NotFound | 1 |
| 按符号 watch add | `-p <pid> watch add aptr_sym slotA qword` → `watch list` | 0x1122334455667788 | 0 |
| 未知符号(业务错误) | `-p <pid> mem read no_such_sym 8` | Error 含 NotFound | 1 |
| watch 未知符号 | `-p <pid> watch add d nosuch qword` | Error 含 NotFound | 1 |
| disable 后符号失效 | `script disable` 后 `mem read slotA 8` | Error 含 NotFound | 1 |
| 副作用:符号寻址后目标存活 | 上例后 `ps list` | 目标 pid 仍在 | 0 |

> 数字地址行为完全不变(数字优先);符号形状(`[A-Za-z_][A-Za-z0-9_]*`)通过解析,
> 存在性由接口层 attach 后查记录决定。集成测试另覆盖 script_symbol 静态库
> API(alloc 后查到同一地址、free 后 NotFound)与 v2.8.0 的
> SymbolAddressingMemWriteRoundTrip(mem write <符号> hex/dec 写入后经公共
> API 读回字节一致、未知符号 NotFound、值格式非法退出 2、disable 后清理)。

## 4.10 alloc near 真实就近分配(v2.7.0,新增)

前置:启动 deeptrace_target.exe,解析 PID 与模块基址。内联脚本
`alloc(nearmem,64,"deeptrace_target.exe"+1000)`(锚点 = 模块基址 + 0x1000)。

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| 模块基址解析 | `-p <pid> module base deeptrace_target.exe` | 输出 16 位 hex 基址 | 0 |
| near run 执行 | `-p <pid> script run <内联 aa>` | stdout 含 alloc nearmem + 地址 | 0 |
| 落点在 ±2GB 内 | 解析 alloc 输出地址,与锚点距离 | ≤ 0x7FFFFFFF | 0 |
| near 符号仍可寻址 | `-p <pid> mem read nearmem 8 hex` | 0(读取成功) | 0 |
| near disable 释放 | `-p <pid> script disable <aa>` | 0 |
| 副作用:near 往返后目标存活 | 上例后 `ps list` | 目标 pid 仍在 | 0 |

> 就近分配优先贴近锚点(集成测试实测落点距锚点约 127KB);窗口内无空闲区
> 时分配失败退出 1,绝不静默退化为任意选址。静态库集成测试另覆盖
> script_alloc_near 记录/符号查找/重复名/NotAttached 错误路径。

## 4.11 批量定位器 JSON:mem batch read/write(v2.9.0,新增)

前置:启动 deeptrace_target.exe,解析 PID、g_int/g_int64 地址与模块基址。
`script run` 执行 script_ptrchain.aa(仅分配三槽),用 `mem write` 布线:
data_slot = 0x1122334455667788、ptr_slot = data_slot 地址(小端字节)、
str_slot = "hello"(68656c6c6f)。JSON 文件即定位器定义(文件即存储)。

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| 脚本分配三槽 | `-p <pid> script run script_ptrchain.aa` → `script status` | 列出 ptr_slot/data_slot/str_slot | 0 |
| 批量读(链) | `mem batch read <json>`:chain_qword=符号 ptr_slot+偏移 0x0 | 0x1122334455667788(链末端=data_slot) | 0 |
| 批量读(直接符号) | data_direct=符号 data_slot | 0x1122334455667788 | 0 |
| 批量读(string) | str=符号 str_slot,type string | hello | 0 |
| 批量读(bytes) | buf=符号 str_slot,type bytes,count 5 | 68 65 6C 6C 6F | 0 |
| 批量读(模块+偏移) | mod_qword=module deeptrace_target.exe + g_int64 偏移 | 0x1122334455667788 | 0 |
| 批量读(绝对地址) | abs_dword=base g_int 地址 | 0x11223344 | 0 |
| 批量写(链) | `mem batch write`:chain_write 符号链 value 0x99AABBCCDDEEFF00 → `mem read data_slot` | 00 FF EE DD CC BB AA 99 | 0 |
| 批量写(string) | str_write=符号 str_slot value world → `mem read str_slot` | 77 6F 72 6C 64 | 0 |
| 批量写(模块+偏移) | mod_write=module+base value 0x8877665544332211 → `mem read g_int64` | 11 22 33 44 55 66 77 88(后恢复) | 0 |
| 写模式缺 value | JSON 项无 value | 退出 2 | 2 |
| JSON 导出(读) | `mem batch read <json> --format json` | stdout 为可 json.loads 的数组(6 项,status=ok,链值=0x99AABBCCDDEEFF00,写后链端) | 0 |
| CSV 导出落盘 | `mem batch read <json> --format csv --out out.csv` | 文件首行 name,address,value,status,error;含 chain_qword,0x...行 | 0 |
| JSON 导出(失败项) | `mem batch read <bad> --format json` | 数组含 status=error + error 含 NotFound | 1 |
| 写模式 JSON 导出 | `mem batch write <json> --format json --out out.json` | 文件含 chain_write/str_write 且 status=ok | 0 |
| --format 非法值 | `mem batch read x.json --format yaml` | Error 含 invalid --format | 2 |
| --format 缺值 | `mem batch read x.json --format` | Error 含 missing argument | 2 |
| --out 写失败 | `mem batch read <json> --format csv --out Z:\\:\\bad\\x.csv` | Error 含 cannot write file | 1 |
| 版本非法 | `{"version": 2, ...}` | 退出 2 | 2 |
| 未知符号(业务) | `{"values":{"x":{"symbol":"no_such_sym"}}}` | Error 含 NotFound | 1 |
| process 不匹配 | `{"process":"notepad.exe",...}` | Error 含 process mismatch | 1 |
| 文件不存在 | `mem batch read no_such.json` | 退出 2 | 2 |
| 副作用:批量后目标存活 | 上例后 `ps list` | 目标 pid 仍在 | 0 |

> JSON 顶层 version/process/values;values 每项三态寻址源互斥(module+base /
> symbol / 绝对 base)+ offsets 多级偏移链;type 8 种(byte/word/dword/qword/
> float/double/string/bytes+count)。逐条失败继续其余条目,存在失败条目最终
> 退出 1。单测覆盖解析/校验(18 例),集成测试另覆盖 BatchLocatorReadWrite
> (真实进程链读写 + 错误路径)。

## 5. 既有回归(引用)

- ps/mem/module/thread/debug/disasm/asm/watch/dll 既有用例全量回归
- 副作用检查:debug run 会话后目标进程仍存活;dll inject/eject 成对完成;
  shellcode alloc/exec/injectfile 记录测试后 free 清理,无残留注入记录
- 清理:测试结束 taskkill deeptrace_target.exe,无残留进程

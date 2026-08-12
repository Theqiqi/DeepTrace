# E2E 测试用例清单(deeptrace_cli)

> 独立 Python 体系(`test_cli_e2e.py`),不参与 CMake。
> 覆盖无参运行、-h/--help、错误路径、-p <pid> 真实目标进程操作、清理后退出。
> 修改模式:v2.2.0 新增 asm file / hex2bin / shellcode alloc|run|free|exec|injectfile
> (汇编代码注入并执行,分阶段操作);v2.3.0 新增 script 组(AA 风格脚本引擎
> run/disable/status,幂等);版本号同步 v2.3.0;既有用例全量回归。

## 1. 全局与基础

| 用例 | 前置 | 操作 | 预期输出 | 退出码 |
|------|------|------|----------|--------|
| 无参运行 | - | `deeptrace_cli` | stderr 含 Missing command | 1 |
| -h | - | `deeptrace_cli -h` | stdout 含 mem read / convert / debug run | 0 |
| --help | - | `deeptrace_cli --help` | 同 -h | 0 |
| -v | - | `deeptrace_cli -v` | stdout 含 deeptrace_cli v2.3.0 | 0 |
| 未知命令组 | - | `deeptrace_cli bogus cmd` | stderr 含 unknown command group | 2 |
| attach 不存在的进程 | - | `deeptrace_cli ps attach 99999999` | Error | 1 |
| 非法参数 | - | `deeptrace_cli mem read zzz` | Error | 2 |

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

前置:启动 deeptrace_target.exe,解析 PID。临时 .aa 脚本写入 BIN_DIR(Windows
可读路径),测试后清理。

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| run 执行 ENABLE(call 型) | 写 `alloc+createThread+ret` 脚本 → `-p <pid> script run <aa>` | stdout 含 alloc newmem / createThread | 0 |
| run 幂等(重复) | 再次 `script run <aa>` | stdout 含 already enabled | 0 |
| status 列出启用脚本 | `-p <pid> script status` | stdout 含脚本文件名 + enabled | 0 |
| disable 执行 DISABLE | `-p <pid> script disable <aa>` | stdout 含 dealloc newmem + OK | 0 |
| disable 幂等(重复) | 再次 `script disable <aa>` | stdout 含 already disabled | 0 |
| run 脚本语法错误 | 写 `[FOO]` → `script run` | stderr 含 unknown block | 2 |
| 副作用:脚本往返后目标存活 | 上例后 `ps list` | 目标 pid 仍在 | 0 |

## 5. 既有回归(引用)

- ps/mem/module/thread/debug/disasm/asm/watch/dll 既有用例全量回归
- 副作用检查:debug run 会话后目标进程仍存活;dll inject/eject 成对完成;
  shellcode alloc/exec/injectfile 记录测试后 free 清理,无残留注入记录
- 清理:测试结束 taskkill deeptrace_target.exe,无残留进程

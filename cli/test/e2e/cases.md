# E2E 测试用例清单(deeptrace_cli)

> 独立 Python 体系(`test_cli_e2e.py`),不参与 CMake。
> 覆盖无参运行、-h/--help、错误路径、-p <pid> 真实目标进程操作、清理后退出。
> 修改模式:v2.0.0 新增 `debug run` 脚本化调试会话用例(一次调用 = 一次会话),
> 版本号同步 v2.0.0;既有用例全量回归。

## 1. 全局与基础

| 用例 | 前置 | 操作 | 预期输出 | 退出码 |
|------|------|------|----------|--------|
| 无参运行 | - | `deeptrace_cli` | stderr 含 Missing command | 1 |
| -h | - | `deeptrace_cli -h` | stdout 含 mem read / convert / debug run | 0 |
| --help | - | `deeptrace_cli --help` | 同 -h | 0 |
| -v | - | `deeptrace_cli -v` | stdout 含 deeptrace_cli v2.0.0 | 0 |
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

前置:启动 deeptrace_target.exe,解析 PID / g_int 地址。脚本文件用临时 json。

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| 脚本化会话(状态/寄存器/读/断点/监视) | 写脚本含 status/registers/read/break/clear/watch_add/watch_list/watch_clear,`-p <pid> debug run s.json` | 各步 `[N] op ...` 头 + 结果;watch_list 显示 0x11223344 | 0 |
| 副作用:会话后目标存活 | 上例后 `ps list` | 目标 pid 仍在 | 0 |
| write 空格分隔 hex | 脚本 `{"op":"write","addr":g_int,"bytes":"BE BA FE CA"}` 后 `mem read` | 读出 CA FE BA BE | 0 |
| 脚本文件不存在 | `debug run no_such.json` | stderr 含 cannot open script file | 2 |
| 脚本 op 未知 | 脚本 `[{"op":"frobnicate"}]` | stderr 含 unknown op | 2 |

## 4. resolve scan(恢复 pattern-only)

前置:启动 deeptrace_target.exe,解析 PID / g_int / g_bytes 地址。

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| AOB 模式扫描(既有回归) | `resolve scan "DE AD BE EF"` | 命中地址含 g_bytes | 0 |
| convert 输出喂给 scan(链式) | `convert dword 287454020` → 取输出 `44 33 22 11` → `resolve scan "44 33 22 11"` | 命中地址含 g_int | 0 |
| 旧类型语法已移除(回归) | `resolve scan 100 dword` | stderr 含 too many arguments | 2 |

## 5. 既有回归(引用)

- ps/mem/module/thread/debug/disasm/asm/watch/dll 既有用例全量回归
- 副作用检查:debug attach 后目标进程仍存活;dll inject/eject 成对完成
- 清理:测试结束 taskkill deeptrace_target.exe,无残留进程

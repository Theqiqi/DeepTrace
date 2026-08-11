# E2E 测试用例清单(deeptrace_cli)

> 独立 Python 体系(`test_cli_e2e.py`),不参与 CMake。
> 覆盖无参运行、-h/--help、错误路径、-p <pid> 真实目标进程操作、清理后退出。
> 修改模式:v1.4.0 按改动点补 `resolve scan` 类型值扫描用例,既有用例全量回归。

## 1. 全局与基础

| 用例 | 前置 | 操作 | 预期输出 | 退出码 |
|------|------|------|----------|--------|
| 无参运行 | - | `deeptrace_cli` | stderr 含 Missing command | 1 |
| -h | - | `deeptrace_cli -h` | stdout 含 mem read / shellcode inject | 0 |
| --help | - | `deeptrace_cli --help` | 同 -h | 0 |
| -v | - | `deeptrace_cli -v` | stdout 含 deeptrace_cli v1.4.0 | 0 |
| 未知命令组 | - | `deeptrace_cli bogus cmd` | stderr 含 unknown command group | 2 |
| attach 不存在的进程 | - | `deeptrace_cli ps attach 99999999` | Error | 1 |
| 非法参数 | - | `deeptrace_cli mem read zzz` | Error | 2 |

## 2. resolve scan(改动点:v1.4.0 类型值输入)

前置:启动 deeptrace_target.exe,解析 PID / g_int / g_bytes 地址。

| 用例 | 操作 | 预期输出 | 退出码 |
|------|------|----------|--------|
| AOB 模式扫描(既有) | `resolve scan "DE AD BE EF"` | 命中地址含 g_bytes | 0 |
| dword 值扫描 | `resolve scan 287454020 dword` | 命中地址含 g_int(0x11223344 LE) | 0 |
| float 值扫描 | `resolve scan 3.14159 float` | 扫描完成输出地址 | 0 |
| string 值扫描 | `resolve scan hi string` | 扫描完成输出地址/无匹配 | 0 |
| 非法 type | `resolve scan 100 bogus` | stderr 含 invalid type | 2 |
| 非法 value | `resolve scan xyz dword` | stderr 含 invalid value for type 'dword' | 2 |

## 3. 既有回归(引用)

- ps/mem/module/thread/debug/disasm/asm/watch/dll 既有用例全量回归
- 副作用检查:debug attach 后目标进程仍存活;dll inject/eject 成对完成
- 清理:测试结束 taskkill deeptrace_target.exe,无残留进程

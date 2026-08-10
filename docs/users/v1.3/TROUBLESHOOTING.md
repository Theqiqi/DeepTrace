# 故障排除(TROUBLESHOOTING)

> 目标读者:遇到问题的用户。先看[常见问题](FAQ.md),再查本文的对照表。
> 错误提示(Error 行)均来自真实运行输出。

## 1. 错误提示对照表

| 屏幕提示(节选) | 含义 | 怎么办 |
|----------------|------|--------|
| `Error: Missing command. Use -h or --help for help.` | 没输入命令 | 加上命令,如 `deeptrace_cli ps list`;或 `deeptrace_cli -h` 看帮助 |
| `Error: unknown command group: 'bogus'` | 命令组拼写错误 | 检查命令组名(ps/mem/module/thread/debug/disasm/resolve/watch/dll/asm/shellcode) |
| `Error: invalid address: 'zzz'` | 地址格式不对 | 地址用十六进制 `0x` 开头,如 `0x14000D000` |
| `Error: NoSuchProcess(99999999)` | 进程不存在 | 用 `ps list` 重新找进程号;进程可能已退出 |
| `Error: NotAttached` | 还没有附加/指定目标进程;或当前没有调试会话(单独运行 `debug detach`) | 加 `-p <进程号>`,或先 `ps attach <进程号>`;调试会话不跨命令保留,属正常现象(见[用户手册 5.1](USER_MANUAL.md#51-进入调试--debug-attach)) |
| `Error: AccessDenied` | 权限不足 | 以管理员身份运行命令窗口;换普通进程测试 |
| `Error: ReadFault` | 地址不可读 | 先用 `mem regions` 找可读区域;检查地址 |
| `Error: WriteFault` | 地址不可写 | 该内存区域只读;找可写区域(protection 允许写) |
| `Error: NotFound` | 找不到指定内容(模块/导出等) | 检查名称拼写;`module list` 确认已加载 |
| `Error: Timeout` | 操作超时(如 DLL 注入等待) | 重试;确认目标进程未挂起/未崩溃 |
| `Error: BadFormat` | 格式错误(汇编指令/特征码等) | 检查指令语法;特征码字节间加空格 |
| `Error: InvalidArg` | 参数值不合法 | 检查参数类型与范围(如类型必须 byte/word/dword/qword/float/double) |
| `Usage: deeptrace_cli [options] <command> [args...]` | 用法错误(退出码 2) | 检查命令与参数,`deeptrace_cli -h` 看用法 |
| `internal exception: ...` | 程序内部异常 | 记录错误信息,确认是最新版本后反馈给维护者 |

**退出码速记**:`0` 成功 / `1` 执行失败 / `2` 用法错误。脚本里可用 `echo %errorlevel%` 查看。

## 2. 已知限制

- **仅支持 Windows x64**:目标进程必须是 64 位程序(32 位进程不支持)。
- **目标进程位宽**:工具与目标进程同是 64 位时才稳定工作。
- **受保护进程**:带反作弊/反调试保护的游戏可能拒绝读写,属预期行为。
- **内存区域**:`mem write` 只能写「可写」区域;某些区域(如代码段)只读。
- **硬件断点数量有限**:通常 4 个(DR0-DR3)。
- **状态文件残留**:目标进程退出后 `%TEMP%\deeptrace_<进程号>\` 记录文件残留(无害)。
- **一次一条命令**:工具是命令行,每次运行执行一条命令后退出(断点/watch 状态会保留,见 [FAQ 第 4 条](FAQ.md#4-断点watch为-什么跨命令还在))。

## 3. 建议:先在测试程序上练习

仓库自带测试目标程序 `deeptrace_target.exe`,它关闭了地址随机化,内存地址固定,方便练习:

1. 双击(或命令行)启动 `deeptrace_target.exe`,窗口会显示:
   ```
   PID: 26128
   g_int       = 0x11223344  @0x14000D000
   g_bytes[0]  = 0xDE @0x14000D018
   ```
   (进程号每次可能不同,地址固定)
2. 用文档中的示例地址操作它:
   ```
   deeptrace_cli -p <上面显示的PID> mem read 0x14000D000 4 hex
   ```
   应输出 `44 33 22 11`(即 `0x11223344` 的字节)。
3. 练习完结束它:`deeptrace_cli -p <PID> ps kill`,或直接关闭窗口。

> 在测试程序上把命令练熟,再对真实目标操作,能避免误操作真实程序。

## 4. 验证安装是否正常

```
deeptrace_cli -v        :: 应显示 deeptrace_cli v1.0.0
deeptrace_cli -h        :: 应显示命令列表
deeptrace_cli ps list   :: 应显示进程表格
```

三条都正常,说明安装与运行环境没有问题;问题出在具体命令上,回到第 1 节对照表排查。

---
name: deeptrace-cli
description: >
  Windows process memory tool deeptrace_cli - install (prefer the official release
  download over a source build) and use it to inspect/modify process memory, enumerate
  processes, read/write memory, scan for AOB/byte patterns, set breakpoints, single-step
  debug, disassemble/assemble, watch variables, and inject DLLs/shellcode. Use when the
  user needs any process-memory / debugging operation on Windows x64. Windows 进程内存
  工具 deeptrace_cli 的安装(优先下载发布版,免构建)与使用。触发词/triggers: process
  memory, memory read/write, AOB/pattern scan, breakpoint, single-step, disassemble,
  inject DLL/shellcode, 进程内存、mem read/write、特征码扫描、断点、单步、反汇编、注入、
  deeptrace。
---

# deeptrace-cli

在 Windows x64 目标进程上执行进程内存操作与调试的 CLI 工具。一条命令 = 一个独立操作;调试为脚本化会话。

## 安装

**优先下载官方发布版本**(免构建);仅当发布版不可用时才从源码构建。

1. 确认发布版存在(HTTP 200 才继续;404/网络失败则跳到第 3 步源码构建):

   ```bash
   curl -sI -f \
     "https://github.com/Theqiqi/DeepTrace/releases/download/v2.1.0/deeptrace_cli-v2.1.0-win64.zip"
   ```

2. 下载发布版 zip 并解压(版本号以仓库最新 tag 为准,当前 `v2.1.0`;压缩包内含单个 `deeptrace_cli.exe`,Release 静态运行时,免依赖):

   ```bash
   curl -fL -o deeptrace_cli.zip \
     "https://github.com/Theqiqi/DeepTrace/releases/download/v2.1.0/deeptrace_cli-v2.1.0-win64.zip"
   unzip -o deeptrace_cli.zip -d deeptrace_cli
   # Windows cmd/PowerShell 无 unzip 时:
   # powershell -c "Expand-Archive -Path deeptrace_cli.zip -DestinationPath deeptrace_cli"
   ```

2. 验证安装:

   ```bash
   ./deeptrace_cli/deeptrace_cli.exe -v   # 应输出 deeptrace_cli v2.1.0
   ./deeptrace_cli/deeptrace_cli.exe -h   # 应输出命令列表(12 组)
   ```

3. 下载不到发布版时,再从源码构建(需 Windows + VS2022/MSVC + CMake≥3.24 + Ninja + vcpkg;WSL 用 `*_wsl.sh` 同名脚本):

   ```bash
   deeptrace/script/build_release.bat      # 先库(CLI 经 find_library 引用库产物)
   cli/script/build_release.bat            # 后 CLI
   cli/script/package.bat v2.1.0           # 产物: cli/out/dist/deeptrace_cli-v2.1.0-win64.zip
   ```

详见 [references/INSTALL.md](references/INSTALL.md)。

## 使用

命令格式:`deeptrace_cli [选项] <命令组> <动作> [参数...]`;大多数操作需要 `-p <pid>`。

核心示例:

```bash
deeptrace_cli ps list                                    # 进程列表
deeptrace_cli -p 1234 mem read 0x14000D000 4 hex         # 读内存 → 44 33 22 11
deeptrace_cli -p 1234 mem write 0x14000D000 CAFEBABE hex # 写内存(可写区域)
deeptrace_cli -p 1234 module base deeptrace_target.exe   # 模块基址
deeptrace_cli -p 1234 disasm at 0x14000D018 3            # 反汇编
deeptrace_cli -p 1234 resolve scan "DE AD BE EF"         # AOB 扫描(?? 通配)
deeptrace_cli -p 1234 debug run session.json             # 调试会话(唯一调试入口)
```

关键事实:

- 退出码:`0` 成功 / `1` 执行失败 / `2` 用法错误。
- 地址:`0x` 前缀十六进制(如 `0x14000D000`)。
- 状态持久化:`%TEMP%\deeptrace_<pid>\`(watch/注入跨命令保留);调试断点只存在于 `debug run` 会话内,会话结束自动恢复。
- **调试只有 `debug run <script.json>` 一个入口**(v2.1.0):一次调用 = 一次完整调试会话。脚本为 JSON 步骤数组,步骤覆盖 `break/clear/hbreak/hclear/guard/unguard/pause/resume/step/next/continue/status/registers/register` + `read/write/disasm/watch_*`;未知操作报错退出码 2。
- 测试目标 `deeptrace_target.exe` 关闭 ASLR,固定地址 `0x14000D000` 存 `0x11223344`(练习用)。

完整命令表与脚本格式见 [references/USAGE.md](references/USAGE.md)。

## 参考

- [安装参考](references/INSTALL.md) / [使用参考](references/USAGE.md)
- 用户手册: `docs/users/v2.1.0/USER_MANUAL.md`
- 开发者文档: `docs/developers/v2.1.0/README.md`
- API 参考: `docs/api/v2.1.0/README.md`

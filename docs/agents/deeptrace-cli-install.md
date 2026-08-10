# deeptrace-cli — 安装提示词(Agent)

> 给 AI / AI agent 的**安装提示词**:当你(agent)收到任务需要用到 deeptrace_cli 时,先按本提示词完成安装与验证。
> 安装完成后,调用软件请使用配套的[调用提示词](deeptrace-cli-usage.md)。
> 索引与说明见仓库根目录 `README.md` 的「AI / Agent 使用说明」节。
> **注意**:下方围栏内的提示词文本会被复制到 agent 上下文,其中文档链接均为**仓库根相对路径**(如 `docs/users/v1.3.0/USER_MANUAL.md`)。

---

`````markdown
---
name: deeptrace-cli-install
description: >
  Install and verify the deeptrace_cli Windows process memory tool (build from source
  or check an existing binary). Use BEFORE any deeptrace_cli command, when the binary
  is missing or its version is unknown. 安装并验证 deeptrace_cli(Windows 进程内存工具),
  在需要执行进程内存操作前先确保工具可用。
when_to_use: >
  Agent needs to run deeptrace_cli commands but the executable is not found, or the
  tool must be built/verified first. 当 agent 需要调用 deeptrace_cli 但可执行文件
  不存在、或需要先构建/验证工具时。
---

# deeptrace-cli 安装提示词

## 1. 检查是否已安装

在调用任何 deeptrace_cli 命令之前,先确认可执行文件存在:

```bash
# 构建产物位置
cli/out/bin/Debug/deeptrace_cli.exe     # Debug 版
cli/out/bin/Release/deeptrace_cli.exe   # Release 版(静态运行时,可分发)
```

- 存在 → 验证可用并直接使用(见第 3 节),跳过安装。
- 不存在 → 继续第 2 节从源码构建。

## 2. 从源码构建

```bash
# 顺序固定:先 deeptrace 库,后 cli(CLI 经 find_library 引用库产物)
deeptrace/script/build_debug.bat        # Windows
cli/script/build_debug.bat
# WSL 环境用 *_wsl.sh 同名脚本(自动桥接 cmd.exe)

# Release(/MT 静态运行时)与打包
deeptrace/script/build_release.bat
cli/script/build_release.bat
cli/script/package.bat v1.3.0             # 产物: cli/out/dist/deeptrace_cli-v1.3.0-win64.zip
```

构建要求:Windows x64 + VS2022(MSVC)+ CMake≥3.24 + Ninja + vcpkg。
三方引擎(keystone/capstone)已内置 `deeptrace/third_party/`,无需联网。

> 若已提供打包好的压缩包(`deeptrace_cli-<版本>-win64.zip`),解压后直接得到单个
> `deeptrace_cli.exe`,无需构建。

## 3. 验证安装

```bash
./cli/out/bin/Debug/deeptrace_cli.exe -v   # 应输出 deeptrace_cli v1.3.0
./cli/out/bin/Debug/deeptrace_cli.exe -h   # 应输出命令列表(11 组 53 个命令)
```

两条输出正常即安装完成。之后调用软件使用[调用提示词](docs/agents/deeptrace-cli-usage.md)。

## 4. 环境提示

- 本工具是命令行程序,单次运行一条命令;目标进程为 Windows x64 程序。
- 构建失败排查见[开发者文档](docs/developers/v1.3.0/BUILDING.md)「常见编译问题」。
`````

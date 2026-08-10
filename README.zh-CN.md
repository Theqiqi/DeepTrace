<p align="center">
  <img src="https://img.shields.io/badge/platform-Windows%20x64-blue" alt="平台:Windows x64"/>
  <img src="https://img.shields.io/badge/language-C%2B%2B20-yellowgreen" alt="C++20"/>
  <img src="https://img.shields.io/badge/build-CMake%20%2B%20Ninja%20%2B%20MSVC-informational" alt="CMake + Ninja + MSVC"/>
  <img src="https://img.shields.io/badge/version-v1.3-blueviolet" alt="v1.3"/>
  <img src="https://img.shields.io/badge/API-55%20functions-green" alt="55 个公共 API"/>
  <img src="https://img.shields.io/badge/license-none%20yet-lightgrey" alt="许可证"/>
</p>

# DeepTrace

**Windows 进程内存工具:静态库(`deeptrace`)+ 命令行前端(`deeptrace_cli`),用于查看与修改正在运行的程序。**

Read this in: [English](README.md) | [简体中文](README.zh-CN.md)

DeepTrace 可以枚举进程、读写进程内存、查看模块与导出表、控制线程、在调试器下设置断点与单步、反汇编代码、汇编指令、特征码(AOB)扫描、监视变量,以及注入 DLL 或壳码——全部通过一个可移植的单文件可执行程序完成。

```
deeptrace_cli -p 1234 mem read 0x14000D000 4 hex
44 33 22 11
```

## 功能特性

**`deeptrace` — 静态库(C++20,55 个公共 API)**

- **进程** — 枚举 / 附加 / 分离 / 挂起 / 恢复 / 结束
- **内存** — 读 / 写 / 转储 / 区域 / 类型化数值读取(byte…double)
- **模块** — 列表 / 查找 / 基址 / 导出表 / 转储
- **线程** — 列表 / 挂起 / 恢复 / 结束
- **调试** — 附加 / 暂停 / 恢复 / 单步进入与跳过,软件 + 硬件 + 页守卫断点,寄存器
- **反汇编与汇编** — x64 反汇编(Capstone)、x64 汇编(Keystone)
- **解析** — 模块基址、AOB 特征码扫描(`??` 通配)
- **监视** — 持久化的变量监视,实时取值
- **注入** — DLL 与壳码注入,含运行状态跟踪
- **状态持久化** — 断点 / 监视 / 注入记录跨命令保留(`%TEMP%\deeptrace_<pid>\`)

**`deeptrace_cli` — 命令行工具(53 个命令,纯 ASCII 输出)**

```
ps, mem, module, thread, debug, disasm, resolve, watch, dll, asm, shellcode
```

- 单次命令,稳定退出码(`0` 成功 / `1` 执行失败 / `2` 用法错误)
- 无交互、适合脚本化、定宽表格输出

## 快速开始

### 获取程序

下载发行压缩包(`deeptrace_cli-<版本>-win64.zip`)——内含单个可移植的 `deeptrace_cli.exe`(静态运行时,无需任何 DLL)。

或者从源码构建(见[构建](#构建))。

### 试一下

```bat
:: 查看帮助 / 版本
deeptrace_cli -h
deeptrace_cli -v

:: 列出正在运行的进程
deeptrace_cli ps list

:: 读取某进程某地址的 4 个字节(十六进制)
deeptrace_cli -p 1234 mem read 0x14000D000 4 hex

:: 读取类型化数值
deeptrace_cli -p 1234 mem readval 0x14000D000 dword
0x11223344

:: 汇编指令为字节
deeptrace_cli asm assemble "nop; ret"
90C3
```

> 上面的地址 `0x14000D000` 与数值 `0x11223344` 来自测试目标程序 `deeptrace_target.exe`(关闭 ASLR、固定基址)。该测试目标仅在 Debug 构建中产出(`cli/out/bin/Debug/`)——在操作真实程序之前,可以先用它安全地练习。

## 构建

要求:**Windows 10/11 x64**、**Visual Studio 2022**(MSVC)、**CMake ≥ 3.24**、**Ninja**、**vcpkg**。Keystone/Capstone 源码已内置在 `deeptrace/third_party/`,无需联网。

```bat
:: 1. 构建 deeptrace 静态库(Debug)
deeptrace\script\build_debug.bat

:: 2. 构建 deeptrace_cli(Debug)
cli\script\build_debug.bat

:: Release 构建(/MT 静态运行时)
deeptrace\script\build_release.bat
cli\script\build_release.bat

:: 打包 zip 到 cli\out\dist\
cli\script\package.bat v1.3
```

WSL 环境使用对应的 `*_wsl.sh` 脚本(自动桥接 cmd.exe)。

## 测试

```bat
deeptrace\out\bin\Debug\deeptrace_unit_test.exe          :: 96 个单元测试(hex/scan/disasm/asm/format)
deeptrace\out\bin\Debug\deeptrace_integration_test.exe   :: 真实目标进程集成测试
cli\out\bin\Debug\deeptrace_cli_unit_test.exe            :: parser/printer/executor
cli\out\bin\Debug\deeptrace_cli_integration_test.exe     :: 完整 CLI 链路
python3 cli/test/e2e/test_cli_e2e.py                     :: 47 项端到端检查
```

## 仓库结构

```
deeptrace/   静态库(domain / algorithm / infrastructure / service)+ include/deeptrace.h
cli/         命令行工具(command / interface / printing 三层)
design/      设计文档(v1.0 / v1.1 / v1.2)
docs/        API 参考、开发者文档、用户手册(v1.3)
sandbox/     实验验证项目(不参与交付)
```

## 文档

| 文档 | 读者 | 内容 |
|------|------|------|
| [API 参考](docs/api/v1.3/README.md) | 集成库的开发者 | 全部 55 个函数、类型、错误码 |
| [开发者文档](docs/developers/v1.3/README.md) | 贡献者 / 维护者 | 架构、构建、测试、扩展、ADR |
| [用户手册](docs/users/v1.3/README.md) | 终端用户 | 快速开始、命令参考、FAQ、故障排除 |

## AI / Agent 使用说明

> **本节是给 AI 与 AI agent 看的索引**(Claude Code、Codex、Cursor、自定义 agent、LLM 工具等)。如果你是一个正在阅读本 README 的 AI,请按以下步骤执行:**① 安装软件 → ② 安装提示词 → ③ 用提示词调用软件**。

**第 ① 步 — 安装软件。** 本仓库是 Windows x64 C++20 进程内存工具。`deeptrace_cli` 必须先构建(或从发行 zip 解压)才能使用。安装/验证的 agent 版说明见[安装提示词](docs/agents/deeptrace-cli-install.md)(构建顺序:先 `deeptrace` 库,后 `deeptrace_cli`)。

**第 ② 步 — 安装提示词。** 本仓库提供两个标准 SKILL 格式(YAML frontmatter + 指令)的 agent 提示词,请加载到你的 agent 中,使其知道何时以及如何使用本工具:

| 提示词 | 用途 | 文件 |
|--------|------|------|
| **安装提示词** | 引导 agent 在使用前构建/验证 `deeptrace_cli` | [docs/agents/deeptrace-cli-install.md](docs/agents/deeptrace-cli-install.md) |
| **调用提示词** | 调用 `deeptrace_cli` 的命令与工作流(进程内存/调试/反汇编/注入等) | [docs/agents/deeptrace-cli-usage.md](docs/agents/deeptrace-cli-usage.md) |

**第 ③ 步 — 用提示词调用软件。** 加载调用提示词后,agent 即可执行如 `deeptrace_cli -p <pid> mem read 0x14000D000 4 hex` 的命令。关键事实:退出码 `0`/`1`/`2`(成功/执行失败/用法错误);地址为 `0x` 前缀十六进制;断点/watch/注入状态存于 `%TEMP%\deeptrace_<pid>\`;测试目标 `deeptrace_target.exe` 关闭 ASLR,固定地址存已知值(如 `0x14000D000` = `0x11223344`)。

其他参考:[用户手册](docs/users/v1.3/USER_MANUAL.md)(完整命令参考)、[API 参考](docs/api/v1.3/README.md)(库 API,用于代码集成)。

## 许可证

尚未指定。如有许可相关问题,见 [issue tracker](https://github.com/Theqiqi/DeepTrace/issues)。

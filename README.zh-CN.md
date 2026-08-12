<p align="center">
  <img src="https://img.shields.io/badge/platform-Windows%20x64-blue" alt="平台:Windows x64"/>
  <img src="https://img.shields.io/badge/language-C%2B%2B20-yellowgreen" alt="C++20"/>
  <img src="https://img.shields.io/badge/build-CMake%20%2B%20Ninja%20%2B%20MSVC-informational" alt="CMake + Ninja + MSVC"/>
  <img src="https://img.shields.io/badge/version-v2.13.0-blueviolet" alt="v2.13.0"/>
  <img src="https://img.shields.io/badge/API-74%20functions-green" alt="74 个公共 API"/>
  <img src="https://img.shields.io/badge/license-MIT-blue" alt="许可证"/>
</p>

# DeepTrace

**Windows 进程内存工具:静态库(`deeptrace`)+ 命令行前端(`deeptrace_cli`),用于查看与修改正在运行的程序。**

Read this in: [English](README.md) | [简体中文](README.zh-CN.md)

DeepTrace 可以枚举进程、读写进程内存、查看模块与导出表、控制线程、在调试器下设置断点与单步、反汇编代码、汇编指令、特征码(AOB)扫描、监视变量、运行 CE(Cheat Engine)风格脚本,以及注入 DLL 或壳码——全部通过一个可移植的单文件可执行程序完成。

```
deeptrace_cli -p 1234 mem read 0x14000D000 4 hex
44 33 22 11
```

## 功能特性

**`deeptrace` — 静态库(C++20,74 个公共 API)**

- **进程** — 枚举 / 附加(含实际授予权限摘要)/ 分离 / 挂起 / 恢复 / 结束
- **内存** — 读 / 写 / 转储 / 区域 / 类型化数值读取(byte…double)
- **模块** — 列表 / 查找 / 基址 / 导出表 / 转储
- **线程** — 列表 / 挂起 / 恢复 / 结束 / 在指定地址创建远程线程
- **调试** — 附加 / 暂停 / 恢复 / 单步进入与跳过,软件 + 硬件 + 页守卫断点,寄存器
- **反汇编与汇编** — x64 反汇编(Capstone)、x64 汇编(Keystone,含基于标签的多行汇编)
- **解析** — 模块基址、AOB 特征码扫描(`??` 通配)、指针链扫描(从目标值反推,rescan 过滤假阳性)
- **监视** — 持久化的变量监视,实时取值
- **脚本引擎** — CE 风格 `.aa` 脚本(alloc / registersymbol / createThread / hook),启用-禁用幂等,`script check` 离线校验
- **Hook** — 改写目标代码跳入你的字节,禁用时恢复原始代码
- **注入** — DLL 与壳码,含运行状态跟踪;壳码 `alloc → run → free` 生命周期(只写不执行、按需触发、释放)
- **状态持久化** — 监视 / 注入 / 脚本 / hook 记录跨命令保留(`%TEMP%\deeptrace_<pid>\`);调试断点只存在于 `debug run` 会话内,会话结束自动恢复

**`deeptrace_cli` — 命令行工具(15 组 55 个命令,纯 ASCII 输出)**

```
ps, mem, module, thread, debug, disasm, resolve, convert, watch, dll, asm, shellcode, script, hex2bin, bin2hex
```

- 单次命令,稳定退出码(`0` 成功 / `1` 执行失败 / `2` 用法错误)
- 无交互、适合脚本化、定宽表格输出
- **符号寻址** — 任意 `<地址>` 参数可接受脚本符号(如 `script run` 后 `mem read sunObjPtr`,v2.6.0)
- **`mem batch`** — 按 JSON 指针链定位器批量读/写,`--format csv|json` 导出结果给其他工具/AI
- **`resolve ptrscan`** — 指针链扫描,输出可直接喂给 `mem batch`(搜索 → 验证闭环)
- **转换闭环(离线)** — `asm file` → `hex2bin` → `bin2hex` / `disasm file` → `shellcode injectfile`(asm↔bin↔hex 互转,无需附加目标进程)
- 调试为脚本化会话 — `debug run <script.json>` 是唯一调试入口(v2.1.0):一次调用 = 一次完整会话(断点/页守卫自动恢复)

## 快速开始

### 获取程序

下载发行压缩包(`deeptrace_cli-<版本>-win64.zip`)——内含单个可移植的 `deeptrace_cli.exe`(静态运行时,无需任何 DLL)。

> ⚠️ 发布版滞后于仓库代码:最新已发布 zip 是 **v2.1.0**,仓库代码为 **v2.13.0**。v2.13.0 的功能(脚本引擎、`mem batch`、`resolve ptrscan`、转换层)只在源码构建中存在——需要时请从源码构建(见[构建](#构建))。

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

:: 离线检查 AA 脚本(语法 + 汇编,无需附加目标)
:: my_script.aa 内容:
::   [ENABLE]
::   alloc(newmem,0x100)
::   newmem:
::   mov rax,1
::   ret
::   [DISABLE]
::   dealloc(newmem)
deeptrace_cli script check my_script.aa
OK (5 steps: 1 alloc, 2 asm, 0 hook, 0 createThread, 0 db)
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
cli\script\package.bat v2.13.0
```

WSL 环境使用对应的 `*_wsl.sh` 脚本(自动桥接 cmd.exe)。

## 测试

```bat
deeptrace\out\bin\Debug\deeptrace_unit_test.exe          :: 137 个单元测试(hex/scan/disasm/asm/script/hook/pointer-scan)
deeptrace\out\bin\Debug\deeptrace_integration_test.exe   :: 53 个真实目标进程集成用例
cli\out\bin\Debug\deeptrace_cli_unit_test.exe            :: 207 个 parser/printer/executor 单元测试
cli\out\bin\Debug\deeptrace_cli_integration_test.exe     :: 51 个完整 CLI 链路集成用例
python3 cli/test/e2e/test_cli_e2e.py                     :: 270 项端到端检查
```

## 仓库结构

```
deeptrace/   静态库(domain / algorithm / infrastructure / service)+ include/deeptrace.h
cli/         命令行工具(command / interface / printing 三层)
design/      设计文档(v1.0.0 … v2.13.0)
docs/        API 参考、开发者文档、用户手册(v2.13.0)
agents/      AI agent 设置指南(agents/README.md)+ 两个技能(agents/deeptrace-cli-install.md、agents/deeptrace-cli-usage.md)
sandbox/     实验验证项目(不参与交付)
```

## 文档

| 文档 | 读者 | 内容 |
|------|------|------|
| [API 参考](docs/api/v2.13.0/README.md) | 集成库的开发者 | 全部 74 个函数、类型、错误码 |
| [开发者文档](docs/developers/v2.13.0/README.md) | 贡献者 / 维护者 | 架构、构建、测试、扩展、ADR |
| [用户手册](docs/users/v2.13.0/README.md) | 终端用户 | 快速开始、命令参考、FAQ、故障排除 |

## AI / Agent 使用说明

> **本节是给 AI 与 AI agent 看的索引**(自定义 agent、LLM 工具等)。如果你是一个正在阅读本 README 的 AI,请按以下步骤执行:**① 读 agent 文档 → ② 安装工具到当前工作目录 → ③ 把技能下载到 .agents/ → ④ 用技能调用软件**。

**第 ① 步 — 读 agent 文档。** [agents/README.md](agents/README.md) 是给 AI agent 看的设置指南(文档为英文):项目概览、关键事实,以及两个设置动作——把 `deeptrace_cli` 安装到当前工作目录、把技能下载到 `.agents/`。

**第 ② 步 — 安装工具到当前工作目录(优先下载发布版)。** 本仓库是 Windows x64 C++20 进程内存工具。从 [v2.1.0 Release](https://github.com/Theqiqi/DeepTrace/releases/tag/v2.1.0) 下载 `deeptrace_cli-v2.1.0-win64.zip`(单个 exe,免依赖)并**解压到当前目录**,使 `deeptrace_cli.exe` 可用;仅当没有发布版、**或需要 v2.13.0 新功能**时才从源码构建(构建顺序:先 `deeptrace` 库,后 `deeptrace_cli`)。见 [agents/README.md](agents/README.md) 或安装技能 [deeptrace-cli-install.md](agents/deeptrace-cli-install.md)。

**第 ③ 步 — 把技能下载到 .agents/。** 把两个技能复制到当前目录的 `.agents/` 中:`agents/deeptrace-cli-install.md`(安装/编译)与 `agents/deeptrace-cli-usage.md`(用法),均为中文。

**第 ④ 步 — 用技能调用软件。** 复制完成后,agent 即可执行如 `deeptrace_cli -p <pid> mem read 0x14000D000 4 hex` 的命令。关键事实:退出码 `0`/`1`/`2`(成功/执行失败/用法错误);地址为 `0x` 前缀十六进制,**或脚本符号名**(`mem read sunObjPtr`);watch/注入/脚本/hook 状态存于 `%TEMP%\deeptrace_<pid>\`;调试为脚本化会话,唯一入口 `debug run <script.json>`(v2.1.0);脚本为 CE 风格 `.aa` 文件,用 `script run <file.aa>` 执行(幂等);测试目标 `deeptrace_target.exe` 关闭 ASLR,固定地址存已知值(如 `0x14000D000` = `0x11223344`)。

其他参考:[用户手册](docs/users/v2.13.0/USER_MANUAL.md)(完整命令参考)、[API 参考](docs/api/v2.13.0/README.md)(库 API,用于代码集成)。

## 许可证

采用 [MIT 许可证](LICENSE) 分发。Copyright (c) 2026 Theqiqi。

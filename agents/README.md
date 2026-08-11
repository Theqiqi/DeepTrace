# DeepTrace — AI / Agent 文档

> 本目录是**给 AI / AI agent 看的文档**(Claude Code、Codex、Cursor、自定义 agent、LLM 工具等):
> 当你需要在本仓库中操作进程内存、调试、反汇编,或需要构建/调用 `deeptrace_cli` 时,先读本文件。
> 可操作的**技能(skill)** 位于 [`agents/skills/deeptrace-cli/SKILL.md`](../agents/skills/deeptrace-cli/SKILL.md),加载后即可按步骤安装并调用工具。

## 1. 项目是什么

仓库包含两个独立的 Windows x64 C++20 项目:

| 项目 | 版本 | 说明 |
|------|------|------|
| **deeptrace**(`deeptrace/`) | 2.0.0 | 进程内存操作**静态库**:进程/内存/模块/线程/调试/反汇编/汇编/解析/监视/注入,**56 个公共 API** |
| **deeptrace_cli**(`cli/`) | 2.1.0 | **命令行程序**:把库能力包装成命令(`ps/mem/module/thread/debug/disasm/resolve/watch/dll/asm/shellcode`),纯 ASCII 输出 |

版本约定:三段式 tag(`v2.1.0` 为当前版本);`deeptrace_cli -v` 输出 `deeptrace_cli v2.1.0`。

## 2. 安装(优先下载发布版)

> 与以往"从源码构建"不同:**优先下载官方发布版本**,构建仅作兜底。

1. 从 GitHub Releases 下载打包 zip(版本号以仓库最新 tag 为准,当前 `v2.1.0`):
   `https://github.com/Theqiqi/DeepTrace/releases/download/v2.1.0/deeptrace_cli-v2.1.0-win64.zip`
   解压得到单个 `deeptrace_cli.exe`(Release 静态运行时,免依赖)。
2. 验证:`deeptrace_cli.exe -v`(输出 `deeptrace_cli v2.1.0`)+ `-h`(命令列表)。
3. 无发布版时再从源码构建(需 Windows + VS2022/MSVC + CMake≥3.24 + Ninja + vcpkg):
   `deeptrace/script/build_release.bat` → `cli/script/build_release.bat` → `cli/script/package.bat v2.1.0`。

详细步骤见 skill 的 [`references/INSTALL.md`](../agents/skills/deeptrace-cli/references/INSTALL.md)。

## 3. 关键事实(调用前必读)

- 命令格式:`deeptrace_cli [选项] <命令组> <动作> [参数...]`;大多数操作需要 `-p <pid>` 指定目标进程。
- 退出码:`0` 成功 / `1` 执行失败 / `2` 用法错误。
- 地址:十六进制 `0x` 前缀(如 `0x14000D000`),64 位定宽显示。
- 状态持久化:`%TEMP%\deeptrace_<pid>\`(watch/注入记录跨命令保留);调试断点只存在于脚本会话内。
- **调试唯一入口是 `debug run <script.json>`**(v2.1.0):一次调用 = 一次完整调试会话,脚本 JSON 步骤数组全量覆盖调试能力;其余 debug 单命令(step/break/registers 等)已移除,调用报 `unknown command`。
- 测试目标 `deeptrace_target.exe`:关闭 ASLR,固定地址 `0x14000D000` 存 `0x11223344`(练习用)。

## 4. 命令组总览

| 组 | 用途 | 常用动作 |
|----|------|----------|
| `ps` | 进程 | `list` / `attach` / `detach` / `info` / `suspend` / `resume` / `kill` |
| `mem` | 内存 | `read` / `write` / `dump` / `regions` / `readval` |
| `module` | 模块 | `list` / `find` / `base` / `exports` / `dump` |
| `thread` | 线程 | `list` / `suspend` / `resume` / `kill` |
| `debug` | 调试 | **`run <script.json>`**(唯一入口) |
| `disasm` | 反汇编 | `at <addr> [n]` / `range <a> <b>` |
| `resolve` | 解析 | `base <mod>` / `scan "<pattern>"`(AOB,支持 `??` 通配) |
| `convert` | 数据转换 | `<type> <value>` → hex 字节(配合 scan;type: byte/word/dword/qword/float/double/string/hex) |
| `watch` | 监视 | `add` / `list` / `remove` / `refresh` / `clear` |
| `dll` | DLL 注入 | `inject` / `eject` / `list` / `status` |
| `asm` | 汇编 | `assemble "<code>" [--hex] [--c-array]` |
| `shellcode` | 注入 | `inject` / `injectat` / `status` |

## 5. 文档地图(v2.1.0)

| 文档 | 读者 | 内容 |
|------|------|------|
| [用户手册](../docs/users/v2.1.0/USER_MANUAL.md) | 使用者/AI | 全部命令用法、`debug run` 脚本格式与示例、FAQ、故障排除 |
| [API 参考](../docs/api/v2.1.0/README.md) | 集成方/AI | 56 个公共 API、类型、错误码、示例 |
| [开发者文档](../docs/developers/v2.1.0/README.md) | 开发者/AI | 架构、构建、测试、扩展、技术决策(ADR) |

## 6. 技能(Skill)

可加载的技能:`agents/skills/deeptrace-cli/SKILL.md`(标准 SKILL.md 格式:YAML frontmatter `name`/`description` + 安装/使用/参考)。加载后 agent 会:优先下载发布版安装 → 验证 → 按命令表调用。

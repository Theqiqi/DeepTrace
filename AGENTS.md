# AGENTS.md — AI 代理仓库指南

> 本文件帮助 AI 代理(Claude Code、Codex 等)快速理解本仓库结构、构建方式与可用工具。
> 面向开发者的完整文档见 [docs/developers/v1.3/README.md](docs/developers/v1.3/README.md)。

## 仓库是什么

Windows 进程内存工具,两个独立 CMake 项目:

- **`deeptrace/`** — C++20 静态库(进程/内存/模块/线程/调试/反汇编/汇编/扫描/监视/注入,55 个公共 API,公共头 `deeptrace/include/deeptrace.h`)
- **`cli/`** — 命令行工具 `deeptrace_cli`(11 组 53 个命令,纯 ASCII 输出)

另有 `design/`(设计文档)、`docs/`(API/开发者/用户三套文档,按 v1.3 归档)、`sandbox/`(实验,不交付)。

## 快速开始

```bash
# 构建(顺序固定:先 deeptrace 库,后 cli)
deeptrace/script/build_debug.bat   # Windows;WSL 用 build_debug_wsl.sh
cli/script/build_debug.bat

# 运行
./cli/out/bin/Debug/deeptrace_cli.exe -p <pid> mem read 0x14000D000 4 hex
```

## 常用命令

```bash
./cli/out/bin/Debug/deeptrace_cli.exe -h                       # 全部命令
./cli/out/bin/Debug/deeptrace_cli.exe ps list                  # 进程列表
./cli/out/bin/Debug/deeptrace_cli.exe -p <pid> ps info         # 进程信息
./cli/out/bin/Debug/deeptrace_cli.exe -p <pid> mem readval 0x14000D000 dword
```

完整命令参考:`.claude/skills/deeptrace-cli/`(skill)与 [用户手册](docs/users/v1.3/USER_MANUAL.md)。

## 测试

```bash
deeptrace/out/bin/Debug/deeptrace_unit_test.exe        # 96 单测
deeptrace/out/bin/Debug/deeptrace_integration_test.exe
cli/out/bin/Debug/deeptrace_cli_unit_test.exe
cli/out/bin/Debug/deeptrace_cli_integration_test.exe
python3 cli/test/e2e/test_cli_e2e.py                   # 47 项 e2e
```

## 约定与注意事项

- **构建顺序**:cli 依赖 deeptrace 构建产物(`find_library` 引用 `deeptrace/out/lib/<配置>/deeptrace.lib`),必须先构建 deeptrace
- **静态库不合并三方依赖**:keystone/capstone 需 CLI 显式链接(cli/src/CMakeLists.txt 已处理,勿删)
- **测试目标**:`deeptrace_target.exe` 关闭 ASLR,固定地址存已知值,集成/e2e 依赖它
- **状态文件**:断点/watch/注入记录存 `%TEMP%\deeptrace_<pid>\`,测试需自行清理
- **CLI 生产代码禁止**:windows.h 等平台头、第三方库、阻塞式输入;输出纯 ASCII
- **WSL**:用 `*_wsl.sh` 脚本(桥接 cmd.exe);.bat 必须 CRLF 行尾(.gitattributes 已强制)
- **命令输出核对**:修改 CLI 行为后需回归 `cli/test/e2e/test_cli_e2e.py`(输出是产品契约)

## 开发流程

仓库的开发流程约定在 `.flow/`(如 `cpp_static_flow.md`、`cli_无交互_development_process.md`、`developer_docs_development_process.md` 等),涉及新功能/新文档时按对应流程执行。

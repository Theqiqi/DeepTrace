# DeepTrace 开发者文档

> 目标读者:所有入项目开发者(新入 / 贡献者 / 维护者)。
> 函数级 API 说明见 [API 文档](../../api/v1.3/README.md),本文档不重复。

## 项目简介

本仓库包含两个独立的 Windows x64 C++20 项目:

- **deeptrace**(`deeptrace/`)— 进程内存操作**静态库**:进程/内存/模块/线程/调试/反汇编/汇编/解析/监视/注入,55 个公共 API。
- **deeptrace_cli**(`cli/`)— **命令行程序**:把库能力包装为命令(ps/mem/module/thread/debug/disasm/asm/resolve/watch/dll/shellcode),纯 ASCII 输出。

## 目录速览

```
deeptrace/   静态库(src 四层:domain/algorithm/infrastructure/service + include/deeptrace.h)
cli/         命令行(src 三层:command/interface/printing + main.cpp)
design/      设计文档(v1.0 / v1.1 / v1.2)
docs/api/    公共 API 参考文档
docs/developers/  开发者文档(本文档集)
sandbox/     实验验证项目(不参与交付)
```

## 快速开始

```bat
:: 1. 构建 deeptrace 静态库(Debug)
deeptrace\script\build_debug.bat
:: 2. 构建 deeptrace_cli(Debug)
cli\script\build_debug.bat
:: 3. 运行
cli\out\bin\Debug\deeptrace_cli.exe -h
cli\out\bin\Debug\deeptrace_cli.exe -p <pid> mem read <地址> 4 hex
```

WSL 环境用同名 `*_wsl.sh` 脚本(自动桥接 cmd.exe)。

## 文档地图

| 文档 | 目标读者 | 内容 |
|------|---------|------|
| [BUILDING.md](BUILDING.md) | 新入项目 | 环境要求、Debug/Release 构建、WSL、打包、常见问题 |
| [ARCHITECTURE.md](ARCHITECTURE.md) | 新入 / 维护者 | 分层架构、数据流、跨项目依赖、状态持久化 |
| [TESTING.md](TESTING.md) | 贡献者 | 单元/集成/e2e 测试、target 程序、编写新测试 |
| [EXTENDING.md](EXTENDING.md) | 贡献者 | 添加命令/API/算法/引擎的扩展指南 |
| [DESIGN_DECISIONS.md](DESIGN_DECISIONS.md) | 维护者 | 技术决策记录(ADR) |
| [ANALYSIS.md](ANALYSIS.md) | 维护者 | 分析阶段产出(代码分析/读者画像/文档需求) |
| [DESIGN.md](DESIGN.md) | 维护者 | 设计阶段产出(结构设计/示例设计/技术选型) |
| [CHANGELOG.md](CHANGELOG.md) | 维护者 | 文档变更历史 |

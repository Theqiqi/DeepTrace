# deeptrace_cli 用户文档 - 版本变更记录

## v1.3.0(发布,2026-08-10)

首个用户文档版本,与代码 tag `v1.3`、API 文档 `docs/api/v1.3/`、开发者文档 `docs/developers/v1.3/` 对齐。

- **发布内容**:README/GETTING_STARTED/USER_MANUAL/FAQ/TROUBLESHOOTING/ANALYSIS/DESIGN/CHANGELOG 全套落盘
- **输出样本**:全部命令输出来自真实运行 deeptrace_cli.exe(Debug 构建)+ deeptrace_target.exe,非虚构
- **审查**:可用性/完整性/一致性审查 + 审查代理复核;修正 5 处(debug detach 单独运行报 NotAttached、FAQ/TROUBLESHOOTING 锚点、Error: Error(...) 格式说明、入门示例输出软化、debug pause 无需先 attach 说明)
- **验证**:README 8 行(≤10);链接/锚点检查通过;输出样本与真实捕获逐条核对一致

## v1.3(初始版本)

首个用户文档版本,与代码 tag `v1.3`、API 文档 `docs/api/v1.3/`、开发者文档 `docs/developers/v1.3/` 对齐。

- **范围**:deeptrace_cli 命令行工具(deeptrace_cli.exe)
- **文档清单**:
  - `README.md` — 产品简介(一句话)+ 快速链接
  - `GETTING_STARTED.md` — 快速开始:下载安装、首次查看进程、首次读取内存
  - `USER_MANUAL.md` — 用户手册:逐命令组任务(ps/mem/module/thread/debug/disasm/resolve/watch/dll/asm/shellcode)
  - `FAQ.md` — 常见问题(按频率排序)
  - `TROUBLESHOOTING.md` — 故障排除(错误对照表)
  - `CHANGELOG.md` — 本文档
- **输出样本**:全部命令输出来自真实运行 `deeptrace_cli.exe`(Debug 构建),针对 `deeptrace_target.exe` 测试目标进程捕获
- **对应代码版本**:deeptrace_cli v1.0.0(命令列表见 `deeptrace_cli -h`)

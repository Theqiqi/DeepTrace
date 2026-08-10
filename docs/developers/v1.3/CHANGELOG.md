# deeptrace 开发者文档 - 版本变更记录

## v1.3(初始版本)

首个开发者文档版本,与代码 tag `v1.3`、API 文档 `docs/api/v1.3/` 对齐。

- **范围**:deeptrace 静态库(deeptrace/)+ 命令行程序 deeptrace_cli(cli/)双项目
- **文档清单**:
  - `README.md` — 项目简介 + 快速开始(入口)
  - `BUILDING.md` — 编译指南(Debug/Release、WSL 桥接、常见问题)
  - `ARCHITECTURE.md` — 架构概览(deeptrace 四层 + cli 三层、数据流、跨项目依赖)
  - `TESTING.md` — 测试指南(单元/集成/e2e、测试目标程序、编写新测试)
  - `EXTENDING.md` — 扩展开发指南(添加命令/API/算法、引擎替换)
  - `DESIGN_DECISIONS.md` — 技术决策记录(ADR)
  - `ANALYSIS.md` / `DESIGN.md` — 分析/设计阶段产出(代码分析、读者画像、文档需求、结构设计)
  - `CHANGELOG.md` — 本文档
- **对应代码版本**:deeptrace 库 v1.0.0、deeptrace_cli v1.0.0(公共 API 55 个,见 `docs/api/v1.3/`)

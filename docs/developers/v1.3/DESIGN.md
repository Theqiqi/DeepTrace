# 开发者文档 - 设计阶段(v1.3)

> 本文件是 `.flow/developer_docs_development_process.md` 第 2 阶段的产出:
> 2.1 文档结构设计
> 2.2 示例设计
> 2.3 技术选型

---

## 2.1 文档结构设计

### 2.1.1 目录结构

```
docs/developers/v1.3/
├── README.md               # 项目简介 + 快速开始(总入口,≤50 行)
├── BUILDING.md             # 编译指南
├── ARCHITECTURE.md         # 架构概览
├── TESTING.md              # 测试指南
├── EXTENDING.md            # 扩展开发指南
├── DESIGN_DECISIONS.md     # 技术决策记录(ADR)
├── ANALYSIS.md             # 分析阶段产出(代码分析/读者/需求)
├── DESIGN.md               # 本文档(设计阶段产出)
└── CHANGELOG.md            # 文档版本变更历史
```

### 2.1.2 各文档章节规划

**README.md**(≤50 行)
1. 项目简介(deeptrace 库 + deeptrace_cli 一句话)
2. 目录速览
3. 快速开始(编译 2 条命令 + 运行 1 条命令)
4. 文档地图(链接全部文档)

**BUILDING.md**
1. 环境要求(Windows x64 / VS2022 MSVC / CMake+Ninja / vcpkg / WSL 可选)
2. Debug 构建(deeptrace → cli,带产物验证)
3. Release 构建(同上,/MT 说明)
4. WSL 构建桥接
5. 打包(zip 归档)
6. 常见编译问题(LNK2038 运行库不匹配、vcpkg SSL、keystone LLVM python 等)

**ARCHITECTURE.md**
1. 总览图(deeptrace 四层 + cli 三层)
2. deeptrace 分层说明(每层职责 + 禁止项 + 依赖方向)
3. cli 分层说明(每层职责 + 禁止项 + 依赖方向)
4. 数据流(命令从 argv 到输出的完整链路)
5. 跨项目依赖(find_library 引用)
6. 状态持久化(状态文件)
7. 会话生命周期(attach/detach、debug attach/detach)

**TESTING.md**
1. 测试体系总览表
2. 运行单元测试
3. 运行集成测试(需 target)
4. 运行 e2e(需 Debug 构建 + testdll.dll)
5. target 程序说明(关闭 ASLR 等)
6. 编写新测试(模板 + 要求)

**EXTENDING.md**
1. 扩展点总览(命令层/API 层/算法层/引擎层)
2. 添加新 CLI 命令(完整示例 + 预期输出)
3. 添加新公共 API(分层改动清单)
4. 添加新算法(纯计算 + 单测)
5. 替换引擎(keystone/capstone 先例)
6. 测试要求

**DESIGN_DECISIONS.md**
1. 为什么静态库 + CLI 双项目
2. 为什么四层分层 / 三层分层
3. 为什么 Keystone / Capstone(源码自建而非 vcpkg)
4. 为什么 Debug=/MDd、Release=/MT
5. 为什么状态文件持久化到 %TEMP%
6. 为什么 target 关闭 ASLR
7. 为什么静态库不合并依赖(消费方显式链接)
8. 为什么 cs_disasm 而不用 cs_disasm_iter(崩溃踩坑)

### 2.1.3 交叉引用规划(双向可达)

- README → 全部文档(总入口)
- BUILDING/ARCHITECTURE/TESTING/EXTENDING/DESIGN_DECISIONS ↔ 互链相关章节
- 函数级说明 → 链接 `docs/api/v1.3/`(Modules/*.md),不重复编写
- 每篇文档顶部注明目标读者

---

## 2.2 示例设计

| 示例 | 所在文档 | 形式 | 验证方式 |
|------|---------|------|---------|
| 从零编译到运行 | README / BUILDING | 命令序列 | 编译产物存在 + CLI 运行 |
| 添加新命令 `ps list2` | EXTENDING | 代码片段 + 预期输出 | 与现有 cmd_process.cpp 对照,逻辑等价 |
| 添加新公共 API | EXTENDING | 分层改动清单 | 与现有 API 对照 |
| 添加新算法 | EXTENDING | 代码骨架 | 对照 algorithm/scan.h 风格 |
| 调用库的完整程序 | 链接到 API 文档 | 已有可编译示例 | docs/api/v1.3/Examples/src/(已编译验证) |

约束:编译类示例必须是真实可编译代码,不用伪代码;文档内嵌命令均有验证记录。

---

## 2.3 技术选型

| 项 | 选择 | 理由 |
|----|------|------|
| 文档格式 | Markdown | 与 design/、.flow/、docs/api/ 现有文档一致;IDE/浏览器/Code 均可读 |
| 代码块 | fenced code block,标注语言 | 语法高亮、复制友好 |
| 命名 | 全英文文件名、中文内容 | 与 docs/api/ 一致,文件系统兼容 |
| 函数说明 | 链接 API 文档 | 避免重复,单一事实来源 |
| 架构图 | ASCII/Mermaid 混合 | 纯 Markdown 可渲染,无外部依赖 |

不混用格式,全文统一 Markdown。
